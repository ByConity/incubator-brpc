#ifndef _BTHREAD_SHARED_MUTEX
#define _BTHREAD_SHARED_MUTEX

#include "bthread/bthread.h"
#include "bthread/condition_variable.h"

namespace bthread
{

struct __shared_mutex_base
{
    Mutex              __mut_;
    ConditionVariable  __read_cv;
    ConditionVariable  __write_cv;
    unsigned           __state_;

    static const unsigned __write_entered_ = 1U << (sizeof(unsigned)*__CHAR_BIT__ - 1);
    static const unsigned __n_readers_ = ~__write_entered_;

    __shared_mutex_base();
    ~__shared_mutex_base() = default;

    __shared_mutex_base(const __shared_mutex_base&) = delete;
    __shared_mutex_base& operator=(const __shared_mutex_base&) = delete;

    // Exclusive ownership
    void lock(); // blocking
    bool try_lock();
    void unlock();

    // Shared ownership
    void lock_shared(); // blocking
    bool try_lock_shared();
    void unlock_shared();

};

class SharedMutex
{
    __shared_mutex_base __base;
public:
    SharedMutex() : __base() {}
    ~SharedMutex() = default;

    SharedMutex(const SharedMutex&) = delete;
    SharedMutex& operator=(const SharedMutex&) = delete;

    // Exclusive ownership
    void lock()     { return __base.lock(); }
    bool try_lock() { return __base.try_lock(); }
    void unlock()   { return __base.unlock(); }

    // Shared ownership
    void lock_shared()     { return __base.lock_shared(); }
    bool try_lock_shared() { return __base.try_lock_shared(); }
    void unlock_shared()   { return __base.unlock_shared(); }
};

class SharedTimedMutex
{
    __shared_mutex_base __base;
public:
    SharedTimedMutex() : __base() {};
    ~SharedTimedMutex() = default;

    SharedTimedMutex(const SharedTimedMutex&) = delete;
    SharedTimedMutex& operator=(const SharedTimedMutex&) = delete;

    // Exclusive ownership
    void lock()            { return __base.lock(); }
    bool try_lock()        { return __base.try_lock(); }
    void unlock()          { return __base.unlock(); }

    template <class _Rep, class _Period>
    bool
    try_lock_for(const std::chrono::duration<_Rep, _Period>& __rel_time)
    {
        return try_lock_until(std::chrono::steady_clock::now() + __rel_time);
    }

    template <class _Clock, class _Duration>
    bool
    try_lock_until(const std::chrono::time_point<_Clock, _Duration>& __abs_time);

    // Shared ownership
    void lock_shared()     { return __base.lock_shared(); }
    bool try_lock_shared() { return __base.try_lock_shared(); }
    void unlock_shared()   { return __base.unlock_shared(); }

    template <class _Rep, class _Period>
    bool
    try_lock_shared_for(const std::chrono::duration<_Rep, _Period>& __rel_time)
    {
        return try_lock_shared_until(std::chrono::steady_clock::now() + __rel_time);
    }

    template <class _Clock, class _Duration>
    bool
    try_lock_shared_until(const std::chrono::time_point<_Clock, _Duration>& __abs_time);

};

template <class _Clock, class _Duration>
bool
SharedTimedMutex::try_lock_until(
                        const std::chrono::time_point<_Clock, _Duration>& __abs_time)
{
    std::unique_lock<bthread::Mutex> __lk(__base.__mut_);
    if (__base.__state_ & __base.__write_entered_)
    {
        while (true)
        {
            std::cv_status __status = __base.__read_cv.wait_until(__lk, __abs_time);
            if ((__base.__state_ & __base.__write_entered_) == 0)
                break;
            if (__status == std::cv_status::timeout)
                return false;
        }
    }
    __base.__state_ |= __base.__write_entered_;
    if (__base.__state_ & __base.__n_readers_)
    {
        while (true)
        {
            std::cv_status __status = __base.__write_cv.wait_until(__lk, __abs_time);
            if ((__base.__state_ & __base.__n_readers_) == 0)
                break;
            if (__status == std::cv_status::timeout)
            {
                __base.__state_ &= ~__base.__write_entered_;
                __base.__read_cv.notify_all();
                return false;
            }
        }
    }
    return true;
}

template <class _Clock, class _Duration>
bool
SharedTimedMutex::try_lock_shared_until(
                        const std::chrono::time_point<_Clock, _Duration>& __abs_time)
{
    std::unique_lock<bthread::Mutex> __lk(__base.__mut_);
    if ((__base.__state_ & __base.__write_entered_) || (__base.__state_ & __base.__n_readers_) == __base.__n_readers_)
    {
        while (true)
        {
            std::cv_status status = __base.__read_cv.wait_until(__lk, __abs_time);
            if ((__base.__state_ & __base.__write_entered_) == 0 &&
                                       (__base.__state_ & __base.__n_readers_) < __base.__n_readers_)
                break;
            if (status == std::cv_status::timeout)
                return false;
        }
    }
    unsigned __num_readers = (__base.__state_ & __base.__n_readers_) + 1;
    __base.__state_ &= ~__base.__n_readers_;
    __base.__state_ |= __num_readers;
    return true;
}
}

#endif
