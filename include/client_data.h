#pragma once

#include <vector>
#include <memory.h>
#include <string>
#include "http_request_parser.h"
#include "http_response_sender.h"

#define READ_BUF_SZ 1024
#define WRITE_BUF_SZ 2048

struct ClientData {
    // constructor
    ClientData(int clientfd = -1, const std::vector<std::string> &_support_content_type): _read_buf_end_idx(0),\
             _send_buf_end_idx(0), _clientfd(clientfd), \ 
             _should_close(false),  hrs(_support_content_type){

        memset(_readbuf, 0, sizeof(_readbuf));
    }

    // copy constructor
    ClientData(const ClientData& rhs): hrs(rhs.hrs) {
        memcpy(_readbuf, rhs._readbuf, strlen(rhs._readbuf));
        _read_buf_end_idx = rhs._read_buf_end_idx;
        _send_buf_end_idx = rhs._send_buf_end_idx;
        _should_close = rhs._should_close;
        _clientfd = rhs._clientfd;
        //TODO:
        // Http_Request_Parser的拷贝构造和析构
        // Http_Response_Sender的拷贝构造和析构
    }

    // destructor
    ~ClientData() {
        memset(_readbuf, 0, sizeof(_readbuf));
        _read_buf_end_idx = 0;
        _send_buf_end_idx = 0;
        _should_close = false;
        _clientfd = -1;
    }
    char _readbuf[READ_BUF_SZ]; // 用于接收recv的数据
    int _read_buf_end_idx = 0;  // _readbuf的有效字符个数
    int _send_buf_end_idx = 0;  // 发送response buf的起点
    bool _should_close;         // 当前用户是否需要关闭
    int _clientfd;              // 当前用户的clientfd

    // 将HTTP数据处理和客户数据绑定在一起是比较好的解决方案，解决了很多问题
    Http_Request_Parser hrp;    
    Http_Response_Sender hrs;
};