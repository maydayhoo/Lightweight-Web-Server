#include "../include/http_response_sender.h"

void Http_Response_Sender::response(Http_Request_Parser &http_request_parser) {
        
    
    // 检查文件相关的准备工作 - 作为后续很多工作的起点
    _check_target_resource(_get_file_pos(http_request_parser));

    // 解析时产生的http code - Internal server error，最高级别错误
    http_code = http_request_parser.http_code;

    while (cur_working_stage != RS_OK) {
        switch (cur_working_stage) {
            case RS_HEADER:
                cur_working_stage = response_header(http_request_parser);
                break;
            case RS_LINES:
                cur_working_stage = response_lines(http_request_parser);
                break;
            case RS_BODY:
                cur_working_stage = response_body(http_request_parser);
                break;
            case RS_OK:
                break;
            case RS_FAIL:
                break;
        }
    }
}

const std::string & Http_Response_Sender::_get_file_pos(Http_Request_Parser &http_request_parser) {
    size_t pos = http_request_parser.req_url.rfind(SEP);
    const std::string filename = http_request_parser.req_url.substr(pos, \
        http_request_parser.req_url.size() - pos);   
    // file_pos: if success, it will look like: /var/www/html/hello.html
    return server_root + SEP + filename;
}

const std::string & Http_Response_Sender::_get_file_pos(const std::string &dir, const std::string &file_name) {

    return server_root + SEP + dir + SEP + file_name;
}

RESPONSE_STAGE Http_Response_Sender::response_body(Http_Request_Parser &http_request_parser) {
        
    // 然后根据HTTP CODE生成相应的响应体，如果HTTP CODE是OK的话

    switch (http_code) {
        case HTTPCODE::OK:
            _set_body_ok(http_request_parser);
            break;
        case HTTPCODE::NOT_MODIFIED:
            _set_body_not_modified();
            break;
        case HTTPCODE::NOT_FOUND:
            _set_body_not_found();
            break;
        case HTTPCODE::Not_ACCEPTABLE:
            _set_body_not_acceptable();
            break;
        case HTTPCODE::BAD_REQUEST:
            _set_body_bad_request();
            break;
        case HTTPCODE::INTERNAL_SERVER_ERROR:
            _set_body_internal_server_error();
            break;
        case HTTPCODE::METHOD_NOT_ALLOWED:
            _set_body_method_not_allowed();
            break;
        case HTTPCODE::FORBIDDEN:
            _set_body_forbidden();
            break;
        // TODO: 还有一些状态未列出全部HTTP的状态码
    }

    return RS_HEADER;
}

// - 检查文件（这一步是全部的步骤中最独立的步骤了，可以作为起点的步骤）
void Http_Response_Sender::_check_target_resource(const std::string &file_pos) {

    // check: verify file status
    __set_http_code(__verify_file(file_pos));
}

// 添加检查字段
void Http_Response_Sender::_generate_check_fields(Http_Request_Parser &http_request_parser) {

    for (auto fields: _check_fields) {
        switch (fields) {
            case CF_CONNECTION:         // Connection
                __set_connection(http_request_parser.key_val["Connection"] == "keep-alive");
                break;
            case CF_CONTENT_TYPE:       // Content-Type
                __set_content_type(http_request_parser.key_val["Content-Type"]);
                break;
            case CF_CONTENT_LENGTH:     // Content-Length
                __set_content_length();
                break;
            case CF_CONTENT_ENCODING:   // Content-Encoding
                _set_content_encoding();
                break;
            case CF_CONTENT_LANGUAGE:   // Content-Language
                _set_content_language({LT_EN});
                break;
            case CF_CONTENT_LOCATION:   // Content-Location
                _set_content_location();
                break;
            case CF_TRANSFER_ENCODING:  // Transfer-Encoding
                _set_transfer_encoding();   // only HTTP/1.1
                break;
            default:
                break;
        }
    }
}

// 添加一般字段
void Http_Response_Sender::_generate_general_fields(Http_Request_Parser &http_request_parser) {

    for (auto fields: _general_fields) {
        switch (fields) {
            case GF_DATE:           // Date
                time_t now = time(0);
                _set_date(now);
                break;
            case GF_SERVER:         // Server
                _set_server_info();
                break;
            case GF_LAST_MODIFIED:  // Last modified
                _set_last_modify_time(http_request_parser.key_val["If-Modified-Since"]);
                break;
            case GF_ETAG:           // ETag
                _set_etag(http_request_parser.key_val["If-None-Match"]);
                break;
            case GF_CACHE_CONTROL:  // Cache-control
                _set_cache_control();
                break;
        }
    }
}

// 验证请求接受的content-type，服务器端是否支持
void Http_Response_Sender::__verify_if_support_accept_content_type(std::string accept_content_type) {
    
    bool support = true;
    for (const auto &_s_p_t: support_content_type) {
        if (accept_content_type != _s_p_t) {
            support = false;
            break;
        }
    }

    // 编写这部分代码
    if (!support && \
            http_code != HTTPCODE::INTERNAL_SERVER_ERROR && \
            http_code != HTTPCODE::BAD_REQUEST && \
            http_code != HTTPCODE::NOT_FOUND && \
            http_code != HTTPCODE::FORBIDDEN) {

        // 如果产生了上述的问题，则上述问题的优先级更大
        __set_http_code(CS_NOT_ACCEPTABLE);
    }
}

char * Http_Response_Sender::_get_file(const std::string &file_pos) {

    int __file_fd = open(file_pos.c_str(), O_RDONLY);

    char * file_address = (char *)mmap(0, __file_stat.st_size, PROT_READ, \
        MAP_PRIVATE, __file_fd, 0);
        
    close(__file_fd);

    return file_address;
}

CHECK_STATE Http_Response_Sender::__verify_file(const std::string &file_pos) {

    memset(&__file_stat, 0, sizeof(__file_stat));

    if (stat(file_pos.c_str(), &__file_stat) < 0) {
        return CS_NORESOURCE;
    }

    if (!(__file_stat.st_mode & S_IROTH)) {
        return CS_NOAUTHORITY;
    }

    if (S_ISDIR(__file_stat.st_mode)) {
        return CS_BADREQUEST;
    }

    return CS_OK;
}

// 统一在此设置HTTP CODE：问题设置是有优先级的，小问题不能覆盖大问题
void Http_Response_Sender::__set_http_code(CHECK_STATE cs) {

    switch (cs) {
        case CS_NORESOURCE:
            http_code = HTTP_UTILS::HTTPCODE::NOT_FOUND;
            break;
        case CS_NOAUTHORITY:
            http_code = HTTP_UTILS::HTTPCODE::FORBIDDEN;
            break;
        case CS_BADREQUEST:
            http_code = HTTP_UTILS::HTTPCODE::BAD_REQUEST;
        case CS_NOT_ACCEPTABLE:
            http_code = HTTP_UTILS::HTTPCODE::Not_ACCEPTABLE;
        case CS_NOT_MODIFIED:
            http_code = HTTP_UTILS::HTTPCODE::NOT_MODIFIED;
        case CS_OK:
            http_code = HTTP_UTILS::HTTPCODE::OK;
            break;
    }
}

void Http_Response_Sender::_set_content_language(const std::vector<LANGUAGE_TYPE> &language_type) {

    resp_lines += "Content-Language: ";
    for (auto choice: language_type) {
        switch (choice) {
            case LT_EN:
                resp_lines += std::string(EN) + ",";
                break;
            case LT_FR:
                resp_lines += std::string(FR) + ",";
                break;
        }
    }
    resp_lines.pop_back();  // 弹出最后一个 “,”
    resp_lines += "\r\n";
}

// 这里也可以检查请求中是否有 If-Modified-Since 字段，
    // 从而提高客户端缓存效率
void Http_Response_Sender::_set_last_modify_time(const std::string &client_since) {
    //  只要文件存在，不管是否文件可读，都可以通过stat获取last modify time
    // TODO：优先级设置法
    if (http_code == HTTPCODE::NOT_FOUND || 
            http_code == HTTPCODE::BAD_REQUEST) {
        // 文件不存在 或 目标资源为目录
        return;
    }
    time_t last_modify_time = __file_stat.st_mtim.tv_sec + __file_stat.st_mtim.tv_nsec / 1e9;
    struct tm *gmt = gmtime(&last_modify_time);
    char buffer[30];
    memset(buffer, 0, sizeof(buffer));
    // strftime函数会在格式化字符串的末尾添加一个'\0'
    strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", gmt);

    std::string s_last_modify_time = std::string(buffer);
    if (s_last_modify_time == client_since) {
        __set_http_code(CS_NOT_MODIFIED);
    }

    resp_lines += "Last-Modified: " + std::string(buffer) + "\r\n";
}

void Http_Response_Sender::_set_date(time_t now) {

    // 将时间转换为UTC时间
    struct tm* p_gmt = gmtime(&now);
    // 格式化时间字段
    char buf[100];
    memset(buf, 0, sizeof(buf));
    strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", p_gmt);

    resp_lines += ("Date: " + std::string(buf) + "\r\n");
    // resp_date = "Date: " + string(buf) + "\r\n";
}

void Http_Response_Sender::__set_content_length() {

    int length = -1;
    if (http_code == HTTP_UTILS::HTTPCODE::BAD_REQUEST || http_code == HTTP_UTILS::HTTPCODE::NOT_FOUND) {

        length = 0;
    }else {
        
        length = __file_stat.st_size;
    }

    resp_lines +=  "Content-Length: " + std::to_string(length) + \
        "\r\n";
    // resp_content_length = "Content-Length: " + to_string(length) + \
    //     "\r\n";
}

// 这里只需将服务器端支持的content-type全部填写进去即可，
void Http_Response_Sender::__set_content_type(std::string accept_content_type) {

    // accpet_content_type分割 , 空格
    // like: text/html; charset=UTF-8, text/plain
    std::istringstream iss(accept_content_type);
    std::string type;
    while (std::getline(iss, type, ',')) {
        __verify_if_support_accept_content_type(type[0] == ' ' ? type.substr(1, type.size()): type);
    }

    // 即使服务器端无法满足客户端接受的内容类型，
    // 服务器端也应该返回一个 Content-Type：内容为服务器端支持的类型
    // 但响应体可能需要按需修改，这个错误等级不是很高
    resp_lines +="Content-type: ";
    std::for_each(support_content_type.begin(), \
        support_content_type.end(), [this](std::string &_content_type) {
            this->resp_lines += _content_type + ", ";
        });
    
    resp_lines.pop_back();   // 弹出最后一个 ","
    resp_lines += "\r\n";
}

void Http_Response_Sender::_set_server_info() {
    // eg. Server: Apache/2.4.1 (Unix) OpenSSL/1.0.2g
    std::string cmd_output = _exec_command("lsb_release -a");
        std::string target_beg = "Description:";
    auto pos = cmd_output.find(target_beg, 0);
    pos += target_beg.size();
    auto iter_server_info_beg = std::find_if(cmd_output.begin() + pos, cmd_output.end(), [](char c) {return isalpha(c);});
    auto iter_server_info_end = std::find_if(iter_server_info_beg, cmd_output.end(), [](char c) {return c == '\n';});

    resp_lines += std::string("Server: RocketStar/1.0 (Linux) ") + cmd_output.substr(iter_server_info_beg - cmd_output.begin(), iter_server_info_end - iter_server_info_beg) \ 
        + "\r\n";
    // resp_server_info = string("Server: RocketStar/1.0(Unix) ") + cmd_output.substr(iter_server_info_beg - cmd_output.begin(), iter_server_info_end - iter_server_info_beg);
}

// 不管文件是否可读，只要文件存在，都可以做
void Http_Response_Sender::_set_etag(const std::string &client_etag) {
    // 这里我不根据文件内容生成，而是根据文件大小和last_modifiedtime生成
    // 因为如果文件不可读，那么是无法根据文件内容生成的
    // 另外一点在于我没有安装OpenSSL库
    if (http_code == HTTP_UTILS::HTTPCODE::NOT_FOUND || 
            http_code == HTTP_UTILS::HTTPCODE::BAD_REQUEST) {

        // 没有目标资源，或目标资源为目录
        return;
    }

    std::string server_etag = std::to_string(__file_stat.st_size) + 
        std::to_string(__file_stat.st_mtim.tv_sec);
    if (client_etag == server_etag) {
        // 我这里是这样想的，哪怕现在文件是不可读的，
        // 但客户端仍然可以继续使用缓存中的目标资源

        
        // TODO: HTTP code 优先级设置法，
        // 只有优先级大于当前的HTTP CODE才可以设置
        __set_http_code(CS_NOT_MODIFIED);
    }
    resp_lines += "ETag: " + server_etag + "\r\n";
}