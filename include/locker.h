#pragma once

#include <semaphore.h>
#include <pthread.h>
#include <exception>
#include <assert.h>

class Sem {
private:
    sem_t _sem;
public:
    Sem() {
        assert(sem_init(&_sem, 0, 0) != 0);
    }
    ~Sem() {
        sem_destroy(&_sem);
    }
    bool wait() {
        return sem_wait(&_sem) == 0;
    }
    bool post() {
        return sem_post(&_sem) == 0;
    }
};

class Locker {
private:
    pthread_mutex_t _mutex;
public:
    Locker() {
        assert(pthread_mutex_init(&_mutex, NULL) == 0);
    }
    ~Locker() {
        pthread_mutex_destroy(&_mutex);
    }
    bool lock() {
        return pthread_mutex_lock(&_mutex) == 0;
    }
    bool unlock() {
        return pthread_mutex_unlock(&_mutex) == 0;
    }
};