#pragma once

#include <list>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <vector>
#include <memory.h>
#include "locker.h"


template<typename ClientData_t>
struct ThreadPoolTask {
    
    // constructor
    ThreadPoolTask(epoll_event event, int clientfd = -1, \
            ClientData_t *p = NULL): \
        _event(event), _clientfd(clientfd), p_client_data(p)  {

        }

    // copy constructor
    ThreadPoolTask(const ThreadPoolTask& rhs) {
        _event = rhs._event;
        _clientfd = rhs._clientfd;
        p_client_data = new ClientData_t(*rhs.p_client_data);
    }

    // destructor
    ~ThreadPoolTask() {

    }

    epoll_event _event; // 任务的类型
    int _clientfd;      // 服务的客户fd
    // 对指向vector中的元素来说, 使用指针是非常危险的，如果vector扩容的话
    ClientData_t *p_client_data;  
};

/**
 * 线程安全的任务容器
 */
template<typename ClientData_t>
class ThreadPoolTaskContainer {
private:

    std::list<ThreadPoolTask<ClientData_t>> task_queue;
public:
    void add(epoll_event event, int clientfd, ClientData_t *p_client_data) {

        _locker.lock();

        task_queue.push_back(ThreadPoolTask(event, clientfd, p_client_data));

        _locker.unlock();
        _sem.post();
    }
    bool try_remove(ThreadPoolTask<ClientData_t> &task) {

        _sem.wait();
        _locker.lock();

        if (task_queue.empty()) {
            _locker.unlock();
            return false;
        }

        // 需要为ThreadPoolTask类写拷贝构造
        task = task_queue.front();
        task_queue.pop_front();

        _locker.unlock();

        return true;
    }
private:
    Sem _sem;
    Locker _locker;
};