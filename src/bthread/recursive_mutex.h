#pragma once
#include <cassert>
#include <pthread.h>
#include "bthread/bthread.h"
#include "bthread/condition_variable.h"

namespace bthread {

class TaskGroup;

extern BAIDU_THREAD_LOCAL TaskGroup* tls_task_group;

class RecursiveMutex {
public:
    DISALLOW_COPY_AND_ASSIGN(RecursiveMutex);

    RecursiveMutex() = default;

    ~RecursiveMutex() { std::lock_guard<Mutex> lk(mtx); }

    void lock() {
        std::unique_lock<Mutex> lk(mtx);

        cv.wait(lk, [this]() { return available(); });
        setup_ownership();
    }

    void unlock() {
        std::unique_lock<Mutex> lk(mtx);

        if (--counter)
            return;

        assert(counter == 0);
        lk.unlock();
        cv.notify_one();
    }

    bool try_lock() {
        std::unique_lock<Mutex> lk(mtx, std::try_to_lock);

        if (!lk.owns_lock() || !available())
            return false;

        setup_ownership();
        return true;
    }

protected:
    bool available() noexcept {
        if (counter == 0)
            return true;

        if (own_by_bthread != !!tls_task_group)
            return false;

        return get_tid() == tid;
    }

    void setup_ownership() noexcept {
        if (counter++)
            return;

        own_by_bthread = !!tls_task_group;
        tid = get_tid();
    }

    uint64_t get_tid() {
        return own_by_bthread ? bthread_self() : pthread_self();
    }

    ConditionVariable cv;
    Mutex mtx;
    int counter {0};
    uint64_t tid {0};
    bool own_by_bthread {false};
};

}  // namespace bthread
