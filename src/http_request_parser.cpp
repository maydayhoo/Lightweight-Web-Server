#include "../include/http_request_parser.h"

#define REQ_BUFFER_SIZE 4096

/**
 * read_buf:   should end with '\0'
 * read_bytes: useful bytes num
 */
PARSE_STAGE Http_Request_Parser::parse(char *read_buf, int read_bytes) {    

    req_buffer_to_parse.append(read_buf);
    req_buffer_end_idx += read_bytes;

    // 要么处理完buf中的数据，
    // 要么已经获得了一个完整请求
    // 要么就是处理出现了问题
    while (cur_check_idx < req_buffer_end_idx && 
        (cur_woking_stage != PS_OK && cur_woking_stage != PS_PARSE_FAIL)) {

        switch (cur_woking_stage) {
            case PS_HEADER:  // 1. 解析请求头

                cur_woking_stage = parse_req_header();
                break;
            case PS_REQLINE: // 2. 解析请求行

                cur_woking_stage = parse_req_lines();
                break;
            case PS_BODY:   //  3. 解析请求体

                cur_woking_stage = parse_req_body();
                break;
            case PS_OK:     //  4. 解析一个完整的HTTP请求报文完成

                http_code = HTTP_UTILS::HTTPCODE::OK;
                break;
            case PS_PARSE_FAIL: // 5. 解析失败，设置HTTP CODE

                // 我认为这是服务器内部的问题，因此返回500
                http_code = HTTP_UTILS::HTTPCODE::INTERNAL_SERVER_ERROR;
                break;
        }
    }
    
    // 在输入数据中发现一个完整的HTTP请求
    if (cur_woking_stage == PS_OK) {
        
        // 将后面的数据，转移到前面去，
        // 因为前面的数据代表一个完整请求报文的数据，不再使用
        // 这样做是无法减少string已经开辟的空间的，即Capacity大小
        req_buffer_to_parse.assign( \
            req_buffer_to_parse.c_str() + cur_check_idx, \
            req_buffer_end_idx - cur_check_idx
        );
        req_buffer_to_parse.shrink_to_fit();
        
        // 重新设置buf的起始位置
        req_buffer_end_idx -= cur_check_idx;
        cur_check_idx = 0;
    }else if (cur_woking_stage == PS_PARSE_FAIL){
        // 解析出现错误

        req_buffer_to_parse.clear();
        cur_check_idx = 0;
        req_buffer_end_idx = 0;
        req_buffer_to_parse.shrink_to_fit();
    }

    return cur_woking_stage;
}

// 解析请求头
PARSE_STAGE Http_Request_Parser::parse_req_header() {

    line_parse_type line_parse_result = __read_one_line();
    cur_line_parse_state = line_parse_result.first;
    switch (cur_line_parse_state) {
        case PLS_OPEN:
            // 1. 还没有读取到完整的一行
            break;
        case PLS_OK: {
            // 2. 读取到完整的一行
            size_t pos = line_parse_result.second;  // '\r'的位置
            std::istringstream iss(req_buffer_to_parse.substr(cur_check_idx, pos - cur_check_idx));
            // 提取请求头 - HTTP/1.1中的形式，HTTP/2.0中的形式不清楚
            iss >> req_method >> req_url >> req_http_version;
            if (!__verify_header(req_method, req_url, req_http_version)) {
                
                cur_woking_stage = PS_PARSE_FAIL;
            }else {
                // 头部字段解析成功
                cur_line_parse_state = PLS_OPEN;
                // cur_check_idx切换到下一行的起点
                cur_check_idx = pos + 2;  
                // 切换到请求行阶段
                cur_woking_stage = PS_REQLINE;
            }
            break;
        }
        case PLS_BAD:

            cur_woking_stage = PS_PARSE_FAIL;
            break;
    }

    return cur_woking_stage;
}

// desc: parse request line
// according to pattern: key: val\r\n
PARSE_STAGE Http_Request_Parser::parse_req_lines() {

    std::pair<PARSE_LINE_STATE, size_t> line_parse_result = __read_one_line();
    cur_line_parse_state = line_parse_result.first;
    switch (cur_line_parse_state) {
        case PLS_OPEN:  
            // 1. 还未读取到一行数据

            break;
        case PLS_OK: {
            // 2. 读取到一行数据
            if (req_buffer_to_parse[cur_check_idx] == '\r' && \
                req_buffer_to_parse[cur_check_idx + 1] == '\n') {
                // 这是请求行的最后一行 - 空行

                cur_check_idx += 2; // move cur_check_idx to next line begin
                cur_woking_stage = PS_BODY;
            }else {
                
                size_t pos = line_parse_result.second;  // '\r' pos
                // 1. get key
                size_t key_end_pos = req_buffer_to_parse.find_first_of(':', \
                cur_check_idx);
                std::string key = req_buffer_to_parse.substr(cur_check_idx, \
                    key_end_pos - cur_check_idx);
                // 2. get val
                key_end_pos += 2;   // move to val start
                std::string val = req_buffer_to_parse.substr(key_end_pos, \
                    pos - key_end_pos);
                key_val.insert({key, val});

                cur_check_idx = pos + 2; // move cur_check_idx to next line begin
            }
            break;
        }
        case PLS_BAD:

            cur_woking_stage = PS_PARSE_FAIL;
            break;
    }

    return cur_woking_stage;
}

// desc: parse request body
PARSE_STAGE Http_Request_Parser::parse_req_body() {

    // 根据Content-Length的长度，
    // 确定请求体的长度，有效字符个数，不包括空字符
    if (key_val.find("Content-Length") == key_val.end() ||
        key_val["Content-Length"] == "0") {
        
        return PS_OK;
    }
    int body_len = stoi(key_val["Content-Length"]);
    if (body_len < 0) {
        // 错误数据

        return PS_PARSE_FAIL;
    }
    // 还没有获取那么多数据
    if (req_buffer_end_idx - cur_check_idx < body_len) {
        
        return PS_BODY;
    }else {
        // 已经获取足够数据，可以填充请求体
        req_body.assign(req_buffer_to_parse.substr(cur_check_idx, \
            body_len));
        cur_check_idx += body_len;  // 移动到下一个请求报文的起始位置

        return PS_OK;
    }
}

std::pair<PARSE_LINE_STATE, size_t> Http_Request_Parser::__read_one_line() {
    size_t pos = req_buffer_to_parse.find_first_of('\r', cur_check_idx);
        
        // 1. 未找到'\r'，只能等新数据到来
        if (pos == std::string::npos || pos + 1 == req_buffer_end_idx) {

            return {PLS_OPEN, -1};
        }

        // 2. 找到"\r\n"
        if (req_buffer_to_parse[pos + 1] == '\n') {

            return {PLS_OK, pos};
        }

        return {PLS_BAD, -1}; // 非以上2种情况，则出现无法处理的错误
}

bool Http_Request_Parser::__verify_header(const std::string &req_method, const std::string &req_url, \
        const std::string &req_http_version) {

    bool is_any_empty = (req_method.empty() || req_url.empty() || \
            req_http_version.empty());
        
    bool is_start_with_http_or_https = 
        (req_url.find("http://") == std::string::npos) || \
        (req_url.find("https://") == std::string::npos);
    
    return !is_any_empty && is_start_with_http_or_https;
}