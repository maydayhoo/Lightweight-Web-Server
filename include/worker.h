#pragma once

#include <stdio.h>
#include "thread_task.h"
#include <unistd.h>
#include <sys/types.h>
#include "locker.h"
#include <iostream>
#include <sys/socket.h>
#include "http_request_parser.h"
#include "http_response_sender.h"
#include <memory.h>
#include "epoll_utils.h"

#define MAX_READ_NUM 1024

// 提供给线程池的工作函数
/*
    最关键的地方在此，
            1. 线程尝试不断的获取任务容器中的数据（通过互斥的方式访问）
                在获取之后，才能进行下一步的动作，根据event类型，
                从sockfd中读，或者是写响应。
                在读取到响应之后，通过 http_request_parse类进行处理。
*/ 
//    关于数据的保存、修改、细节非常关键

/**
 * the work routine for thread
 */
template<typename ClientData_t>
class Worker {
public:
    static void* work(void *args) {
        
        ThreadPoolTaskContainer<ClientData_t> *task_container = \
            (ThreadPoolTaskContainer<ClientData_t> *)args;

        // 线程的工作就是不断的尝试从任务容器中取下任务
        while (true) {

            ThreadPoolTask<ClientData_t> task;

            if (!task_container->try_remove(task)) { // 没有成功获取任务
                continue;
            }

            // 根据分析：不需要将自己设置为当前工作客户的服务者，
            //      因为要使用EPOLLONESHOT，只有当前线程是其服务者，
            //      在处理未完成之前，clientfd的EPOLLIN不会再次触发

            if (task._event & EPOLLIN) {
                // 线程读取客户端数据
                int read_bytes = -1;
                while (true) {
                    read_bytes = recv(task._clientfd, \
                        task.p_client_data->_readbuf + task.p_client_data->_read_buf_end_idx, \
                            MAX_READ_NUM - 1, 0);
                    if (read_bytes == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            // 本次EPOLLIN数据已经读完
                            break;
                        }else {
                            // 说明出问题了
                            // 解决方式：为FD注册写事件为FD注册写事件，让主线程关闭连接
                            // 主线程查看EPOLLOUT事件时，先查看该fd的_should_close标记
                            task.p_client_data->_should_close = true;
                            Epoll_Util::modifyfd(task._clientfd, EPOLLOUT);
                        }
                    }else if (read_bytes == 0) {

                        // 说明客户端断开了连接，发送了FIN报文
                        // 方式：为FD注册写事件为FD注册写事件，
                        // 让主线程响应EPOLLOUT事件，从而关闭连接
                        task.p_client_data->_should_close = true;
                        Epoll_Util::modifyfd(task._clientfd, EPOLLOUT);
                        break;
                    }else {
                        
                        // 成功读到数据
                        task.p_client_data->_read_buf_end_idx += read_bytes;
                        
                        // 数据量超过了buffer数组的长度，认为此次连接有误
                        if (task.p_client_data->_read_buf_end_idx > task.p_client_data->READ_BUF_SZ) {
                            // 解决方式：为FD注册写事件，让主线程关闭连接
                            task.p_client_data->_should_close = true;
                            Epoll_Util::modifyfd(task._clientfd, EPOLLOUT);
                            break;
                        }
                    }
                }
                
                // 若should_close，则线程本次处理task已完成
                // 尝试获取下一个task
                if (task.p_client_data->_should_close) {
                    continue;
                }

                // 开始处理recv得到的数据
                PARSE_STAGE state = task.p_client_data->hrp.parse(\
                        task.p_client_data->_readbuf, \
                        task.p_client_data->_read_buf_end_idx
                    );

                // 因为上面将task.p_client_data->_readbuf中的数据交给了parser
                // 因此可以还原read_buf了
                memset(task.p_client_data->_readbuf, 0, task.p_client_data->READ_BUF_SZ);
                task.p_client_data->_read_buf_end_idx = 0;

                if (state == PARSE_STAGE::PS_OK || state == PARSE_STAGE::PS_PARSE_FAIL) {

                    // 请求报文解析完成后，让sender来生成响应
                    //   之后其他线程处理EPOLLOUT事件时，
                    //   只需要拿到task.p_client_data->hrs中的响应数据即可
                    // task.p_client_data->hrs.response(task.p_client_data->hrp);
                    task.p_client_data->_should_close = false;
                    Epoll_Util::modifyfd(task._clientfd, EPOLLOUT);
                }else {

                    // 本次操作完成后，由于没有读到一个完整报文，
                    // 因此需要继续EPOLLIN
                    // 因为使用了EPOLLONESHOT，因此需要修改fd的内核事件表
                    // 重新将fd，注册到epollfd中
                    task.p_client_data->_should_close = false;
                    Epoll_Util::modifyfd(task._clientfd, EPOLLIN);
                }
            }else if (task._event & EPOLLOUT) {
                // 线程给客户端发送数据

                task.p_client_data->hrs.response(task.p_client_data->hrp);

                int send_bytes = 0;
                // 将sender中的数据，发送给clientfd
                // 可能无法一次性将数据全部发送到TCP发送缓冲区
                while (true) {

                    send_bytes = send(task._clientfd, \ 
                        task.p_client_data->hrs.get_response_data() + task.p_client_data->_send_buf_end_idx, \
                            task.p_client_data->hrs.get_response_data_len, 0);

                    if (send_bytes == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            // 说明TCP发送缓冲区没有空间了，
                            // 需要等待下一次TCP发送缓冲区有足够空间
                            // 即EPOLLOUT触发，但可能是由其他线程接着干了
                            std::cout << "发送缓冲区已满" << std::endl;
                            task.p_client_data->_should_close = false;
                            Epoll_Util::modifyfd(task._clientfd, EPOLLOUT);
                            break;
                        }else {
                            // 数据发送出了问题，该如何处理?断开连接吗
                            // 书中是断开连接
                            task.p_client_data->_should_close = true;
                            Epoll_Util::modifyfd(task._clientfd, EPOLLOUT);
                            break;
                        }
                    }
                    
                    task.p_client_data->_send_buf_end_idx += send_bytes;
                    if (task.p_client_data->_send_buf_end_idx >= task.p_client_data->hrs.get_response_data_len) {
                        // 说明一个响应报文的数据已全部发送完成
                        // 根据请求报文中的Connection字段，
                        //      告诉主线程是断开连接还是继续连接
                        // 若持续连接，则继续clientfd的EPOLLIN事件
                        // 若断开连接，则修改为EPOLLOUT事件，并设置should_close标志
                        task.p_client_data->_send_buf_end_idx = 0;
                        bool is_keep_alive = task.p_client_data->hrp.is_keep_alive();
                        if (is_keep_alive) {
                            task.p_client_data->_should_close = false;
                            Epoll_Util::modifyfd(task._clientfd, EPOLLIN);
                        }else {

                            task.p_client_data->_should_close = true;
                            Epoll_Util::modifyfd(task._clientfd, EPOLLOUT);
                        }
                    }
                }
            }else {
                // event中的事件类型是当前线程不支持处理的，
                // 因此不处理当前task，并重新尝试拿到新的task

                continue;
            }
        }
    }
};