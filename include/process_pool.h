#pragma once

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <assert.h>
#include <vector>
#include "epoll_utils.h"
#include <signal.h>
#include "utils.h"
#include <cerrno>
#include <iostream>
#include <wait.h>
#include "locker.h"
#include "thread_task.h"
#include "thread_pool.h"

#include "http_request_parser.h"
#include "http_response_sender.h"

#include "worker.h"
#include "heap.h"

using namespace std;

#define MAX_CLIENT_NUM 4096


template<typename ClientData_t>
class ProcessPool;

/**
 * desc: represent one process
 */
class Process {
    template<typename ClientData_t>
    friend class ProcessPool;
public:

    Process(pid_t pid = -1): _pid(pid) {}
    Process(const Process &rhs) {
        _pid = rhs._pid;
        _pipefd[0] = rhs._pipefd[0];
        _pipefd[1] = rhs._pipefd[1];
        _serverd_user_count = rhs._serverd_user_count;
    }
    Process &operator=(const Process &rhs) {
        _pid = rhs._pid;
        _pipefd[0] = rhs._pipefd[0];
        _pipefd[1] = rhs._pipefd[1];
        _serverd_user_count = rhs._serverd_user_count;
    }
private:

    pid_t _pid;     // pid
    int _pipefd[2];  // IPC: using pipe to communicate with parent Process
    int _serverd_user_count = 0;        // user playload
};

/**
 * desc: manage all processes
 */
template<typename ClientData_t>
class ProcessPool {
private:
    ProcessPool(int listenfd, work_routine_t work_routine, \
            int process_number = 8): \
        _listen_fd(listenfd), \
        _process_num(process_number), \
        _process_idx(-1) {

        // check valid input
        assert(0 < _process_num && _process_num <= MAX_PROCESS_NUM);
        process_pool.assign(_process_num, Process());

        // 创建process_number个进程
        for (int i = 0; i < _process_num; ++i) {
            
            // create pipe between child process with father process
            assert(socketpair(PF_UNIX, SOCK_STREAM, 0, \
                process_pool[i]._pipefd));

            // fork one process
            process_pool[i]._pid = fork();
            assert(process_pool[i]._pid >= 0);

            // father process
            if (process_pool[i]._pid > 0) {    

                // 将子进程所在pool的下标加入到进程调度堆中（比加pid好）
                _process_heap.insert({i, process_pool[i]._serverd_user_count});

                // 关闭和子进程通信管道的一端，因为不使用
                close(process_pool[i]._pipefd[1]);  // father close write 

                continue;   // father process job done.
            }

            // 以下只有子进程执行
            close(process_pool[i]._pipefd[0]);      // child close read
            _process_idx = i;   // to identify father or child   
            _serverd_user_count = 0; 

            break;
        }
    }
public:
    // 进程池的静态方法，通过这里创建单例线程池
    static ProcessPool* create(int listenfd, \
            work_routine_t work_routine, int process_number = 8) {
        if (!Instance) {
            Instance = new ProcessPool(listenfd, work_routine, \
                process_number);
        }
        return Instance;
    }

    // 进程工作前的准备工作
    void init() {
        setup_epoll();             // 准备内核事件表
        setup_sig_pipe();          // 设置信号管道（用于统一事件源）
        setup_sig();               // 添加进程需要管理的信号
    }

    // 创建每个进程的内核事件表
    void setup_epoll() {
        Epoll_Util::init();
        Epoll_Util::create();
    }

    // 为每个进程统一事件源 (信号源)
    static void setup_sig_pipe() {
        assert(socketpair(PF_UNIX, SOCK_STREAM, 0, _sig_pipefd) != -1);
        Epoll_Util::addfd(_sig_pipefd[0]);
    }

    // 添加每个进程需要监听的信号
    void setup_sig() {
        addsig(SIGCHLD, handler);
        addsig(SIGTERM, handler);
        addsig(SIGINT, handler);
        addsig(SIGPIPE, SIG_IGN);
    }

    // 线程工作函数 - 拦路虎
    void run(int thread_num) {

        if (_process_idx == -1) {   // 父进程的工作内容

            run_father();
        }else {                     // 子进程的工作内容

            run_child(thread_num);   
        }
    }

    // 子进程真正的工作逻辑
    void run_child(int thread_num = 8) {

        // 进程开始工作前的准备工作
        init();

        // 每个进程都有自己的一个任务容器以及线程池为任务服务
        ThreadPoolTaskContainer<ClientData_t> thread_task_container;  
        ThreadPool<ClientData_t> _thread_pool(work_routine, &thread_task_container, thread_num);
        _thread_pool.create();  // 在进入子进程以后，创建thread_num个线程

        // 和父进程之间的管道 - 父进程通过管道，来告诉子进程可以accept
        // 因为通过socketpair生成，所以任何一端，可读可写
        // 因此使用 _pipefd[1] 还是 _pipefd[0]无所谓
        int pipefd = process_pool[_process_idx]._pipefd[1];
        Epoll_Util::addfd(pipefd);  // 监听和父进程通信的管道

        // 客户信息表 
        // - 下标为clientfd 
        // - 用于存储每个客户读缓冲区/HTTP_Parser/HTTP_Sender
        // - use ClientData_t default constructor
        vector<ClientData_t> client_data(MAX_CLIENT_NUM);

        // 子进程开始工作
        while (is_working) {

            // 开始监听事件
            auto ret = Epoll_Util::wait_for_events();
            epoll_event *events = ret.first;
            int len = ret.second;
            
            // check return val status
            if (len < 0 && errno != EINTR) {    
                cout << "epoll() system call failed" << endl;
                break;
            }

            // process every event
            for (int i = 0; i < len; ++i) {

                int sockfd = events[i].data.fd;

                if (sockfd == pipefd && (events[i].events & EPOLLIN)) {
                   // 1. 父进程告诉子进程listenfd有新的连接, 子进程直接accept

                    bool have_new_conn = false;
                    int ret = recv(pipefd, &have_new_conn, sizeof(have_new_conn), 0);
                    if (ret <= 0 || !have_new_conn) {
                        // 这里本应该更细节的处理一下
                        continue;
                    }else { 
                        // has new connection
                        int client_fd;
                        if (_try_accept_client_connection(&client_fd)) {
                            // 进行新连接用户数据的添加
                            
                            // 1. 将client_fd添加到内核事件表中
                            Epoll_Util::addfd(client_fd, true);

                            // 2. 更新客户表对应项
                            client_data[client_fd]._clientfd = client_fd;

                            // 3. 告诉父进程，该子进程服务人数 + 1
                            int conn_info[2] = {_process_idx, 1};
                            send(pipefd, conn_info, sizeof(conn_info), 0);
                        }
                    }
                }else if (sockfd == _sig_pipefd[0] && (events[i].events & EPOLLIN)) {
                    // 2. 处理信号
                    char signals[1024];
                    int ret = recv(_sig_pipefd[0], signals, sizeof(signals), 0);
                    if (ret <= 0) continue;

                    for (int i = 0; i < ret; ++i) {
                        switch (signals[i]) {
                            case SIGCHLD: { 
                                // 子进程的子进程工作结束(应该不会发生，因为没有)
                                
                                pid_t child_pid;
                                int stat;
                                while ((child_pid) = waitpid(-1, &stat, WNOHANG) > 0) {

                                    continue;
                                }
                                break;
                            }
                            case SIGTERM: case SIGINT:  
                                // 该子进程结束工作

                                // 服务人数置0，并停止工作
                                // 父进程会收到SIGCHILD信号
                                _serverd_user_count = 0;
                                is_working = false;
                                break;
                            default:    
                                // other signal will not be processed.
                                break;
                        }
                    }
                }else {     
                    // 3. 处理客户请求，根据用户到来的请求，
                    //      将其封装为任务，添加到任务容器中去

                    epoll_event event;
                    if (events[i].events & EPOLLIN) {

                        event.events |= EPOLLIN;
                    }else if (events[i].events & EPOLLOUT) {

                        if (client_data[sockfd]._should_close) {

                            // 出现异常：当前用户需要关闭
                            close(sockfd);

                            // 告诉父进程，当前子进程服务人数 - 1
                            int conn_info[2] = {_process_idx, -1};
                            send(pipefd, conn_info, sizeof(conn_info), 0);
                            continue;
                        }

                        event.events |= EPOLLOUT;
                    }else if (events[i].events & EPOLLRDHUP) {

                        // 客户端发起断开连接
                        close(sockfd);

                        // 告诉父进程，当前子进程服务人数 - 1
                        int conn_info[2] = {_process_idx, -1};
                        send(pipefd, conn_info, sizeof(conn_info), 0);
                        continue;
                    }
                    
                    // 通过互斥的方式向任务容器中添加数据
                    thread_task_container.add(event, sockfd, &client_data[sockfd]);
                }
            }
        }
    }

    // 父进程真正的工作逻辑
    void run_father() {

        // 进程工作前的准备工作
        init();

        // 和父进程之间的管道 - 父进程通过管道，来告诉子进程可以accept
        // 子进程也可以将消息通过这里告诉父进程
        // 将每个进程的管道加入到内核事件表中
        for (int i = 0; i < _process_num; ++i) {
            if (process_pool[i]._pid != -1) {
                Epoll_Util::addfd(process_pool[i]._pipefd[0]);
            }
        }

        // 父进程的工作逻辑
        // 1. 监听listenfd
        // 2. 根据一定逻辑选择一个子进程进行工作
        // 3. 处理父进程收到的信号

        // 监听listenfd
        Epoll_Util::addfd(_listen_fd);

        // 父进程开始工作
        while (is_working) {

            // 开始监听事件
            auto ret = Epoll_Util::wait_for_events();
            epoll_event *events = ret.first;
            int len = ret.second;

            if (len < 0 && errno != EINTR) {

                cout << "father process epoll() failed: errno = " 
                        << errno << endl;
                // 如果父进程的epoll失败，那listen是没有意义的
                // 因为子进程不会得到新的task，应该关闭每个子进程
                _kill_child_process();
                is_working = false;
                continue;
            }

            // 处理每个事件
            for (int i = 0; i < len; ++i) {

                int sockfd = events[i].data.fd;

                if (sockfd == _listen_fd && (events[i].events & EPOLLIN)) {
                    // 1. 处理新客户到来

                    // 根据策略，选择当前工作负载最小的子进程
                    _choose_min_load_child_process();

                }else if (_check_if_pipefd(sockfd) && (events[i].events & EPOLLIN)) {
                    // 2. 处理子进程给父进程发送的消息
                    // （目前我只发送用户人数改变消息）也可以发送其他消息

                    int conn_info[2];   // 其中包含子进程在进程池中的idx与用户增减信息
                    int ret = recv(pipefd, conn_info, sizeof(conn_info), 0);
                    if (ret <= 0) {
                        // 接收管道信息出现问题，则处理下一个epoll_event
                        continue;   
                    } 

                    // 更新子进程当前服务的客户数量
                    process_pool[conn_info[0]]._serverd_user_count += \ 
                            conn_info[1];
                    
                    // 更新_process_heap中的顺序
                    int idx = conn_info[0];
                    int user_count = process_pool[conn_info[0]]._serverd_user_count;
                    _refresh_process_heap(idx, user_count);
                    
                }else if (sockfd == _sig_pipefd[0] && (events[i].events & EPOLLIN)) {

                    char signals[1024];
                    int ret = recv(_sig_pipefd[0], signals, sizeof(signals), 0);
                    if (ret <= 0) {
                        // 接收管道信息出现问题
                        continue;
                    }

                    // 父进程信号处理
                    for (int i = 0; i < ret; ++i) {
                        switch (signals[i]) {
                            case SIGCHLD:
                                // 子进程结束
                                __withdraw_child_process();
                                break;
                            case SIGINT: case SIGTERM:
                                // 我觉得这里需要将子进程全部关闭
                                // 书中也是这样做的
                                cout << "kill all child process" << endl;
                                is_working = false;
                                _kill_child_process();
                                break;
                        }
                    }
                }
            }
        }
    }

private:    // 全部子进程都会有一份下列的拷贝，不管是否是静态
    static ProcessPool *Instance;   // ProcessPool: Singleton Instance
    static const int MAX_PROCESS_NUM = 16;  
    bool is_working = true;             // 进程是否工作
    int _listen_fd;                     // 由每个子进程accpet客户连接
    int _process_num;                   // 进程池中的进程数量
    vector<Process> process_pool;
    int _process_idx;                   // 区分子进程和父进程的一个标志
    static int _sig_pipefd[2];          // 每个进程内实现统一信号事件源的管道
    Heap<std::pair<int, int>> _process_heap;  // 给主进程使用，虽然每个进程都会有一份，但其他进程不使用 
private:

    // 检查sockfd是不是和子进程通信的管道fd
    bool _check_if_pipefd(int sockfd) {
        for (int i = 0; i < _process_num; ++i) {
            if (process_pool[i]._pid != -1 && \
                process_pool[i]._pipefd[0] == sockfd) {
                
                return true;
            }   
        }
        return false;
    }

    // 信号处理函数
    static void handler(int sig) {
        int old_errno = errno;
        send(_sig_pipefd[1], (char *)&sig, 1, 0);   // 只传输sig的第一个字节
        errno = old_errno;
    }

    // 子进程尝试接受客户连接请求
    bool _try_accept_client_connection(int *p_client_fd) {
        
        sockaddr_in client_addr;
        socklen_t client_addr_sz;
        int client_fd = accept(_listen_fd, \
            (sockaddr *)&client_addr, &client_addr_sz);

        if (client_fd < 0) {

            cout << "In Child Process accept() system call failed, errno: " 
                << errno << endl;
            return false;
        }

        *p_client_fd = client_fd;
        return true;
    }

    // 跟新小根堆
    void _refresh_process_heap(int idx, int user_count) {

        std::pair<int, int> pval = {idx, user_count};

        _process_heap.modify(pval, [&pval](const std::pair<int, int> &p) -> bool {
            return pval.first == p.first;
        });
    }

    // 选择最小负载子进程工作：通过小根堆
    void _choose_min_load_child_process() {

        bool has_new_conn = true;
        std::pair<int, int> _min_load_process = _process_heap.top();
        int _min_load_process_pid_idx = _min_load_process.first;

        send(process_pool[_min_load_process_pid_idx]._pipefd[0], \
            &has_new_conn, sizeof(has_new_conn), 0);
    }

    // 给子进程发送SIGTERM信号
    void _kill_child_process(int c_pid = -1) {
        if (c_pid != -1) {
            
            kill(c_pid, SIGTERM);
        }else {
            for (int i = 0; i < _process_num; ++i) {

                if (process_pool[i]._pid != -1) {

                    kill(process_pool[i]._pid, SIGTERM);
                }
            }

            // 清空_process_heap
            _process_heap.clear();
        }
    }

    // 回收子进程
    void __withdraw_child_process() {

        int c_stat_loc = -1;
        pid_t c_pid;
        while ((c_pid = waitpid(-1, &c_stat_loc, WNOHANG)) > 0) {

            for (int i = 0; i < _process_num; ++i) {

                if (process_pool[i]._pid == c_pid) {

                    process_pool[i]._pid = -1;
                    process_pool[i]._serverd_user_count = 0;
                    close(process_pool[i]._pipefd[0]);

                    // 从_process_heap中删除该元素
                    std::pair<int, int> pval = {i, 0};
                    _process_heap.delete_from_heap(pval, [&pval](const std::pair<int, int> &p) {
                            return p.first == pval.first;
                    })
                }
            }
        }
    }
};