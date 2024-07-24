#pragma once

#include <iostream>
#include <string>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <stdio.h>
#include "http_request_parser.h"
#include <unistd.h>
#include "http_utils.h"
#include <ctime>
#include <sstream>
#include <fstream>
#include <memory>
#include <array>
#include "utils.h"
#include <algorithm>
#include <vector>
#include <numeric>

using namespace HTTP_UTILS;

#ifdef _WIN32
    #define SEP "\"
#elif __linux__
    #define SEP "/"
#endif

// webserver root: store all html resouce
extern const std::string server_root = "/var/www/html";

// TODO
// 对于全部的错误，都应该准备相应的HTML页面，并将内容作为响应体返回给客户端
#define NOT_MODIFIED_RESPONSE "Resource is not modified"
#define FORBIDDEN_RESPONSE ""
#define METHOD_NOT_ALLOWED_RESOPONSE ""
#define INTERNAL_SERVER_ERROR_RESPONSE ""
#define BAD_REQUEST_RESPONSE ""
#define Not_ACCEPTABLE_RESPONSE ""
#define NOT_FOUND_RESPONSE ""

// 一般字段
enum GENERAL_FIELDS {
    GF_DATE = 0,
    GF_SERVER,
    GF_LAST_MODIFIED,
    GF_ETAG,    // 唯一的标识资源，类似于身份证，若资源被修改，则ETAG更新
    GF_CACHE_CONTROL,   // 指示请求端应该如何缓存接收到的数据，包括缓存时间等
                        // 静态，动态，敏感内容，策略不同
    // 因为不涉及到帐号、密码，因此不使用cookie
};

// 检查请求报文中的对应字段，若有，则添加相应的回复
enum CHECK_FIELDS {
    CF_CONNECTION = 0,
    CF_CONTENT_TYPE,
    CF_CONTENT_LENGTH,
    CF_CONTENT_ENCODING,
    CF_CONTENT_LANGUAGE,
    CF_CONTENT_LOCATION,
    CF_TRANSFER_ENCODING,
    CF_ALLOW
};

// 生成响应报文的阶段
enum RESPONSE_STAGE{
    RS_HEADER = 0,
    RS_LINES,
    RS_BODY,
    RS_OK,
    RS_FAIL
};

// 响应过程中的问题类型，用于生成对应的HTTP CODE
enum CHECK_STATE {
    CS_NORESOURCE = 0,
    CS_NOAUTHORITY,
    CS_BADREQUEST,
    CS_NOT_ACCEPTABLE,
    CS_NOT_MODIFIED,
    CS_OK
};

class Http_Response_Sender {
private:
// response structre:
    std::string resp_header;
    std::string resp_lines;
    std::string resp_body;
    
// 一般字段（不需检查请求报文中的对应字段从而给出答案）- 按需生成
    // string resp_date;           // Date
    // string resp_server_info;    // Server
    // string resp_last_modified;  // Last-Modified - 资源最后修改时间
    // string resp_eTag;           // ETag - 资源的实体标签，用于缓存验证
    // string resp_cache_control;  // Cache-Control - 缓存策略
    // string resp_expires;        // Expires - 响应的过期时间
    // string resp_set_cookie;     // Set-Cookie - 设置HTTP Cookie
    // string resp_location;       // Location - 用于重定位响应，指示客户端访问新的URL
    
// 相关字段（需检查请求报文中的对应字段从而给出答案）
    // string resp_connecton;      // 是否长连接 -  Connection
    // string resp_content_type;   // 是否支持客户端要求的响应类型 - Accept
    // string resp_content_length; // 响应体的长度 - 
    // string resp_content_encoding;   // 响应体的编码方式 - Accept-Encoding
    // string resp_content_language;   // 响应体的语言 - Accept-Language
    // string resp_content_location;   // 响应体的位置？ - Content-Location
    // string resp_transfer_encoding;  // 传输编码的方式 - Transfer-Encoding
    // string resp_allow;              // 服务器支持的请求方法 - Allow
    
// cur working stage:
    RESPONSE_STAGE cur_working_stage;
    
// server support content-type: provided by server;
    static std::vector<std::string> support_content_type;
    
// general fields
    std::vector<GENERAL_FIELDS> _general_fields = {
        GF_DATE, GF_SERVER, GF_LAST_MODIFIED, GF_ETAG, GF_CACHE_CONTROL
    };

// check fields
    std::vector<CHECK_FIELDS> _check_fields = {
        CF_CONNECTION, CF_CONTENT_TYPE, CF_CONTENT_LENGTH, \
        CF_CONTENT_ENCODING, CF_CONTENT_LANGUAGE, \
        CF_CONTENT_LOCATION, CF_TRANSFER_ENCODING
    };

/*客户请求的目标文件被mmap到内存中的起始位置*/
    char *__file_address;

    // 目标文件的状态。通过它我们可以判断文件是否存在、
    // 是否为目录、是否可读，并获取文件大小等信息
    struct stat __file_stat;

    // 全局应该只有唯一一个http_code 发生任何错误，应该修改之
    HTTP_UTILS::HTTPCODE http_code;     
public:
// constructor
    Http_Response_Sender():
        cur_working_stage(RS_LINES), __file_address(NULL) {

    }

// copy constructor
    Http_Response_Sender(const Http_Response_Sender& rhs):resp_header(rhs.resp_header), \
         resp_lines(rhs.resp_lines), resp_body(rhs.resp_body), \
         cur_working_stage(rhs.cur_working_stage), \
         _general_fields(rhs._general_fields), \
         _check_fields(rhs._check_fields), \
         http_code(rhs.http_code) {
        
        memcpy(__file_address, rhs.__file_address, sizeof(rhs.__file_address));

        memcpy(&__file_stat, &rhs.__file_stat, sizeof(__file_stat));   
    }

// destructor
    ~Http_Response_Sender() {

        delete __file_address;
        __file_address = NULL;
        memset(&__file_stat, 0, sizeof(__file_stat));
        cur_working_stage = RS_LINES;
        clear_data();
    }

    static void set_support_content_type(const std::vector<std::string> &_support_content_type) {
        support_content_type = std::move(_support_content_type);
    }

    // 整个响应报文，用于给线程向客户端发送响应报文
    const char * get_response_data() {
        std::string resp_data = resp_header + resp_lines + resp_body;
        
        return resp_data.c_str();
    }

    // 整个响应报文的长度
    int get_response_data_len() {
        return resp_header.size() + resp_lines.size() + resp_body.size();
    }

    // 清空数据
    void clear_data() {

        resp_header.clear();
        resp_lines.clear();
        resp_body.clear();
        
        resp_header.shrink_to_fit();
        resp_lines.shrink_to_fit();
        resp_body.shrink_to_fit();
    }

    // 生成响应报文，根据请求报文
    void response(Http_Request_Parser &http_request_parser);

    // 根据HTTP CODE，生成相应的header
    RESPONSE_STAGE response_header(Http_Request_Parser &http_request_parser) {

        resp_header += http_request_parser.req_http_version + " " + HTTP_UTILS::http_header_response[http_code];

        return RS_OK;
    }

    RESPONSE_STAGE response_lines(Http_Request_Parser &http_request_parser) {

        // 添加一般字段的值
        _generate_general_fields(http_request_parser);
        
        // 添加检查字段的值
        _generate_check_fields(http_request_parser);

        // 添加空行
        resp_lines += "\r\n";

        return RS_BODY;
    }

    // TODO：这一部分可能有问题，因为 URL的形式似乎有很多
    // 这里目前处理的是类似于： /index.html 这种形式
    RESPONSE_STAGE response_body(Http_Request_Parser &http_request_parser);

private:

    // - 检查文件（这一步是全部的步骤中最独立的步骤了，可以作为起点的步骤）
    void _check_target_resource(const std::string &file_pos);

    const std::string & _get_file_pos(Http_Request_Parser &http_request_parser);
    const std::string & _get_file_pos(const std::string &dir, const std::string &file_name);

    // 403 FORBIDDEN
    void _set_body_forbidden() {

        const std::string &file_pos = _get_file_pos("error-4xx", "forbidden.html");
        _check_target_resource(file_pos);
        
        __file_address = _get_file(file_pos);
        // finally: get resp_body
        resp_body.assign(__file_address);
    }

    // 405 METHOD_NOT_ALLOWED
    void _set_body_method_not_allowed() {

        const std::string &file_pos = _get_file_pos("error-4xx", "method_not_allowed.html");
        _check_target_resource(file_pos);
        
        __file_address = _get_file(file_pos);
        // finally: get resp_body
        resp_body.assign(__file_address);
    }

    // 500 INTERNAL_SERVER_ERROR
    void _set_body_internal_server_error() {

        const std::string &file_pos = _get_file_pos("error-5xx", "internal_server_error.html");
        _check_target_resource(file_pos);
        
        __file_address = _get_file(file_pos);
        // finally: get resp_body
        resp_body.assign(__file_address);
    }

    // 400 BAD_REQUEST
    void _set_body_bad_request() {

        const std::string &file_pos = _get_file_pos("error-4xx", "bad_request.html");
        _check_target_resource(file_pos);
        
        __file_address = _get_file(file_pos);
        // finally: get resp_body
        resp_body.assign(__file_address);
    }

    // 406 Not_ACCEPTABLE
    void _set_body_not_acceptable() {

        const std::string &file_pos = _get_file_pos("error-4xx", "not_acceptable.html");
        _check_target_resource(file_pos);
        
        __file_address = _get_file(file_pos);
        // finally: get resp_body
        resp_body.assign(__file_address);
    }

    // 404 Not Found
    void _set_body_not_found() {

        const std::string &file_pos = _get_file_pos("error-4xx", "not_found.html");
        _check_target_resource(file_pos);
        
        __file_address = _get_file(file_pos);
        // finally: get resp_body
        resp_body.assign(__file_address);
    }

    // 304 NOT_MODIFIED
    void _set_body_not_modified() {

        const std::string &file_pos = _get_file_pos("error-3xx", "not_modified.html");
        _check_target_resource(file_pos);
        
        __file_address = _get_file(file_pos);
        // finally: get resp_body
        resp_body.assign(__file_address);
    }

    // 200 ok
    void _set_body_ok(Http_Request_Parser &http_request_parser) {
        
        // file: with successful status ： OK
        const std::string &file_pos = _get_file_pos(http_request_parser);
        __file_address = _get_file(file_pos);
        // finally: get resp_body
        resp_body.assign(__file_address);
    }

    // get file by mmap
    char * _get_file(const std::string &file_pos);

    // 添加检查字段
    void _generate_check_fields(Http_Request_Parser &http_request_parser);

    // 添加一般字段
    void _generate_general_fields(Http_Request_Parser &http_request_parser);

    // 验证请求接受的content-type，服务器端是否支持
    void __verify_if_support_accept_content_type(std::string accept_content_type);

    // 检查文件合理性
    CHECK_STATE __verify_file(const std::string &file_pos);

    // release mmap
    void __file_ummap() {
        if (__file_address) {

            munmap(__file_address, __file_stat.st_size);
            __file_address = 0;
        }
    }

    // 统一在此设置HTTP CODE：问题设置是有优先级的，小问题不能覆盖大问题
    void __set_http_code(CHECK_STATE cs);

    /**
     * transfer_encoding字段：指定数据是如何传输的，而不是如何编码的
     *      如分块传输、
     *      也就是告诉客户端，响应数据不是一次性完整发送给你，而是逐步发送
     * 这在处理大数据或动态生成内容时，非常管用，提高了效率，
     * 因为显示结果可以慢慢全部得到，而不是一直在等着，最后一次性显示出来
     * 例子：
     *  Transfer-Encoding: chunked
     *  分块传输编码（Chunked Transfer Encoding）是一种在 HTTP 传输过程中, 
     *  将响应体分块传输的方式。每个块有一个大小字段，后跟实际数据。
     *  最后一个块的大小为零，表示数据结束。
     *  对应的响应体：
     *  4
        Wiki
        5
        pedia
        0
        在 HTTP/2 中，Transfer-Encoding 头字段的支持被移除，
        使用了更为高效的流和帧机制。
     */
    void _set_transfer_encoding() {

    }

    /**
     * content_location 字段：非强制要求
     *      指示的是响应内容所在的原始位置，以URL形式给出
     *      可以帮助客户端了解内容的位置
     *      常用于资源重定向的情况，以及通过代理服务器缓存资源的时候
     * 若301 Moved Permanently，则强制要求 Location字段：
     *      以URL形式给出：如Location: http://www.new-url.com/new-page
     *      指示资源最新的位置，客户端将被重定向到该URL
     */
    void _set_content_location() {

    }

    // content_language：告诉客户端响应内容是什么自然语言
    // 在多语言网站中，通过该字段指定使用的语言。
    // 客户端可以根据用户所在的地域，来选择合适的版本提供给用户
    // eg. （比如经常在右上角出现的翻译选项）
    void _set_content_language(const std::vector<LANGUAGE_TYPE> &language_type);

    // Content-Encoding 指示内容的编码方式（如压缩算法）
    // 设置服务器端对响应内容的编码方式：
    // 可以依据请求报文中的content_encoding字段中要求的值
    // 在此通过硬编码的方式：即服务器端对传输内容不进行编码
    void _set_content_encoding(const std::string &encoding_method = "identity") {

        resp_lines += std::string("Content-Encoding: ") + encoding_method + \
            "\r\n";
    }

    // 缓存控制字段是指导客户端以及中间缓存服务器（或客户端的代理服务器）
    // 如何存储服务器端响应的内容，在此我采用硬编码的方式
    void _set_cache_control() {
        /**
         * private: 指示响应只能被单个用户缓存，
         *      通常是浏览器缓存，不能由共享缓存（如代理服务器）存储。
         * public: 指示响应可以被任何缓存（包括中间代理缓存）存储。
         * no-cache: 客户端必须重新验证资源，即客户端每次必须重新请求。
         * no-store: 指示客户端和中间缓存服务器不得存储任何版本的响应。
         * max-age=<seconds>：指示响应在指定的时间（以秒为单位）内是新鲜的，
         *      客户端和中间缓存服务器可以在此时间内使用缓存的响应而不去重新验证。
         * immutable：
         *      指示响应的内容不会改变，客户端可以长时间缓存此响应，无需检查其新鲜度。
         *  */ 
        resp_lines += std::string("Cache-Control: ") + \
            "private, max-age=3600" + "\r\n";
    }

    // 不管文件是否可读，只要文件存在，都可以做
    void _set_etag(const std::string &client_etag);

    // 这里也可以检查请求中是否有 If-Modified-Since 字段，
    // 从而提高客户端缓存效率
    void _set_last_modify_time(const std::string &client_since);

    void _set_date(time_t now);

    void __set_content_length();

    void __set_connection(bool linger) {

        resp_lines += std::string("Connection: ") + \
            (linger ? "keep-alive": "close") + "\r\n";
    }

    // 这里只需将服务器端支持的content-type全部填写进去即可，
    void __set_content_type(std::string accept_content_type);

    void _set_server_info();
};