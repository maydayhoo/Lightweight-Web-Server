#include "../include/epoll_utils.h"

void Epoll_Util::init() {
    events = new epoll_event[MAX_EVENT_NUMBER];
}

std::pair<epoll_event*, int> Epoll_Util::wait_for_events() {

    int number = epoll_wait(_epoll_fd, events, MAX_EVENT_NUMBER, -1);
    return {events, number};
}

// 创建内核事件表
void Epoll_Util::create() {
    _epoll_fd = epoll_create(_max_fd);
    assert(_epoll_fd != -1);
}

// 向内核事件表添加fd 及 监听事件
void Epoll_Util::addfd(int fd, bool oneshot = false) {

    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP | (oneshot ? EPOLLONESHOT: 0);
    epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, fd, &event);

    setnonblocking(fd);
}

// 从内核事件表中删除fd
void Epoll_Util::removefd(int fd) {

    epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);
}

// 修改fd的事件
void Epoll_Util::modifyfd(int fd, int ev) {
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &event);
}