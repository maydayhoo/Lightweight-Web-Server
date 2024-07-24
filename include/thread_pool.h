#pragma once

#include <stdio.h>
#include <iostream>
#include <sys/sem.h>
#include <pthread.h>
#include <assert.h>
#include <vector>
#include <memory.h>
#include "thread_task.h"

using work_routine_t = void *(*)(void *);

template<typename ClientData_t>
class ThreadPool {
public:
    ThreadPool(work_routine_t work_routine, ThreadPoolTaskContainer<ClientData_t> *p_thread_task_container, int thread_num = 8): \
        _work_routine(work_routine), _thread_num(thread_num), \
        _p_thread_task_container(p_thread_task_container) {
        
        assert(thread_num > 0);

        assert(work_routine != NULL);

        _threads.assign(_thread_num, -1);
        assert(!_threads.empty());
    }
    void create() {

        // 创建_thread_num个线程
        for (int i = 0; i < _thread_num; ++i) {

            std::cout << "creating " << i + 1 << " thread" << std::endl;

            // 写任务类，其中函数为静态，存储任务的数据类型为双向链表
            assert(pthread_create(&_threads[i], NULL, _work_routine, _p_thread_task_container) == 0);

            // 设置为分离线程
            assert(pthread_detach(_threads[i]) != 0);
        }
    }
    ~ThreadPool() {
        _work_routine = NULL;
    }
private:
    int _thread_num;
    std::vector<pthread_t> _threads;
    work_routine_t _work_routine;
    ThreadPoolTaskContainer<ClientData_t> *_p_thread_task_container;
};