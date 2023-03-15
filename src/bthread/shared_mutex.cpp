#include "shared_mutex.h"

namespace bthread {
// Shared Mutex Base
__shared_mutex_base::__shared_mutex_base()
    : __state_(0)
{
}

// Exclusive ownership

void
__shared_mutex_base::lock()
{
    std::unique_lock<Mutex> lk(__mut_);
    while (__state_ & __write_entered_)
        __read_cv.wait(lk);
    __state_ |= __write_entered_;
    while (__state_ & __n_readers_)
        __write_cv.wait(lk);
}

bool
__shared_mutex_base::try_lock()
{
    std::unique_lock<Mutex> lk(__mut_);
    if (__state_ == 0)
    {
        __state_ = __write_entered_;
        return true;
    }
    return false;
}

void
__shared_mutex_base::unlock()
{
    std::lock_guard<Mutex> _(__mut_);
    __state_ = 0;
    __read_cv.notify_all();
}

// Shared ownership

void
__shared_mutex_base::lock_shared()
{
    std::unique_lock<Mutex> lk(__mut_);
    while ((__state_ & __write_entered_) || (__state_ & __n_readers_) == __n_readers_)
        __read_cv.wait(lk);
    unsigned num_readers = (__state_ & __n_readers_) + 1;
    __state_ &= ~__n_readers_;
    __state_ |= num_readers;
}

bool
__shared_mutex_base::try_lock_shared()
{
    std::unique_lock<Mutex> lk(__mut_);
    unsigned num_readers = __state_ & __n_readers_;
    if (!(__state_ & __write_entered_) && num_readers != __n_readers_)
    {
        ++num_readers;
        __state_ &= ~__n_readers_;
        __state_ |= num_readers;
        return true;
    }
    return false;
}

void
__shared_mutex_base::unlock_shared()
{
    std::lock_guard<Mutex> _(__mut_);
    unsigned num_readers = (__state_ & __n_readers_) - 1;
    __state_ &= ~__n_readers_;
    __state_ |= num_readers;
    if (__state_ & __write_entered_)
    {
        if (num_readers == 0)
            __write_cv.notify_one();
    }
    else
    {
        if (num_readers == __n_readers_ - 1)
            __read_cv.notify_one();
    }
}

}
