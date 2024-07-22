#pragma once

#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <signal.h>
#include <memory.h>
#include <assert.h>
#include <string>
#include <stdlib.h>
#include <memory>

using sig_hander = void (*) (int);

// 设置非阻塞IO
extern int setnonblocking(int fd);

extern void addsig(int sig, sig_hander handler, bool restart = true);

// 用于执行在命令行执行命令，并将命令执行的结果返回到程序
extern std::string _exec_command(const char *cmd);
