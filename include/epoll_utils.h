#pragma once
#include <sys/socket.h>
#include <sys/types.h>
#include "utils.h"
#include <assert.h>
#include <utility>

#define MAX_EVENT_NUMBER 10000

class Epoll_Util final{
private:
    static int _epoll_fd;
    static int _max_fd;
    static epoll_event *events;
public:
    static void init();

    static std::pair<epoll_event*, int> wait_for_events();

    // 创建内核事件表
    static void create();

    // 向内核事件表添加fd 及 监听事件
    static void addfd(int fd, bool oneshot = false);

    // 从内核事件表中删除fd
    static void removefd(int fd);

    // 修改fd的事件
    static void modifyfd(int fd, int ev);
};