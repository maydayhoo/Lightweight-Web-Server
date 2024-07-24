#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include <sstream>
#include "http_utils.h"

#define REQ_BUFFER_SIZE 4096

// 当前所处的解析阶段
enum PARSE_STAGE{
    PS_REQLINE = 0,
    PS_HEADER,
    PS_BODY,
    PS_OK,
    PS_PARSE_FAIL
};

// 当前解析一行所处的阶段
enum PARSE_LINE_STATE {
    PLS_OPEN = 0,
    PLS_OK,
    PLS_BAD
};

class Http_Response_Sender;

// HTTP请求报文解析类：
// 目标：支持解析HTTP/1.1和HTTP/2.0报文
class Http_Request_Parser {

    friend class Http_Response_Sender;
    using line_parse_type = std::pair<PARSE_LINE_STATE, size_t>;
private:
    int cur_check_idx;      // 当前解析的字节
    int req_buffer_end_idx; // buffer的最后一个字节的下一个位置 

// request message stat
    std::string req_buffer_to_parse; 
    std::string req_method;
    std::string req_url;
    std::string req_http_version;
    std::unordered_map<std::string, std::string> key_val;
    std::string req_body;

    PARSE_STAGE cur_woking_stage;
    PARSE_LINE_STATE cur_line_parse_state;

// final parse state.
    HTTP_UTILS::HTTPCODE http_code;
public:
// constructor
    Http_Request_Parser(): cur_check_idx(0), req_buffer_end_idx(0), \
        cur_woking_stage(PS_HEADER) {

    }

// copy constructor
    Http_Request_Parser(const Http_Request_Parser& rhs): req_method(rhs.req_method), \
         req_url(rhs.req_url), req_http_version(rhs.req_http_version), \
         key_val(rhs.key_val), req_body(rhs.req_body), \
         cur_woking_stage(rhs.cur_woking_stage), \
         cur_line_parse_state(rhs.cur_line_parse_state), \
         http_code(rhs.http_code) {
            
        cur_check_idx = rhs.cur_check_idx;
        req_buffer_end_idx = rhs.req_buffer_end_idx;
        std::string req_buffer_to_parse; 
    }

// destructor
    ~Http_Request_Parser() {
        cur_check_idx = 0;
        req_buffer_end_idx = 0;
        cur_woking_stage = PS_HEADER;
    }

    /**
     * read_buf:   should end with '\0'
     * read_bytes: useful bytes num
     */
    PARSE_STAGE parse(char *read_buf, int read_bytes);

    // desc: parse request header
    PARSE_STAGE parse_req_header();  

    // desc: parse request line
    // according to pattern: key: val\r\n
    PARSE_STAGE parse_req_lines();

    // desc: parse request body
    PARSE_STAGE parse_req_body();

    // 是否长连接
    inline bool is_keep_alive() {

        // HTTP/1.1 default set
        return key_val["Connection"] == "keep-alive";
    }

private:
    /*
        desc: 保证从每一行的第一个字符开始读起
     */
    std::pair<PARSE_LINE_STATE, size_t> __read_one_line();

    /**
     * req_method: 
     * req_url:
     * req_http_version:
     */
    bool __verify_header(const std::string &req_method, const std::string &req_url, \
        const std::string &req_http_version);
};