#include "bthread/recursive_mutex.h"

namespace bthread {

// The bthread equivalent of std::recursive_timed_mutex.
// This is also a higher level construct not directly supported by native bthread APIs.
class RecursiveTimedMutex : public RecursiveMutex {
public:
    template<class Rep, class Period>
    bool try_lock_for(const std::chrono::duration<Rep, Period>& rel_time) {
        return try_lock_until(std::chrono::steady_clock::now() + rel_time);
    }

    template<typename Clock, typename Duration>
    bool try_lock_until(const std::chrono::time_point<Clock, Duration>& timeout_time) {
        std::unique_lock<Mutex> lk(mtx);
        while(!available()) {
            if(Clock::now() >= timeout_time)
                break;

            cv.wait_until(lk, timeout_time);
        }

        if (!available())
            return false;

        setup_ownership();
        return true;
    }
};

// The bthread equivalent of std::timed_mutex.
class TimedMutex {

public:
    using native_handle_type = bthread_mutex_t*;

    DISALLOW_COPY_AND_ASSIGN(TimedMutex);

    TimedMutex();

    ~TimedMutex() { CHECK_EQ(0, bthread_mutex_destroy(&_mutex)); }

    void lock();

    void unlock() { bthread_mutex_unlock(&_mutex); }

    bool try_lock() { return !bthread_mutex_trylock(&_mutex); }

    template<typename Rep, typename Period>
    bool try_lock_for(const std::chrono::duration<Rep, Period>& rel_time) {
        return try_lock_until(std::chrono::steady_clock::now() + rel_time);
    }

    template<typename Clock, typename Duration>
    bool try_lock_until(const std::chrono::time_point<Clock, Duration>& timeout_time) {
        auto dur = timeout_time - Clock::now();
        auto sys_timeout = std::chrono::time_point_cast<std::chrono::nanoseconds>(
                               std::chrono::system_clock::now() + dur);
        auto nanos_since_epoch = sys_timeout.time_since_epoch();
        auto secs_since_epoch = std::chrono::duration_cast<std::chrono::seconds>(nanos_since_epoch);
        auto max_timespec_secs = std::numeric_limits<decltype(timespec::tv_sec)>::max();
        timespec sp{};
        if (secs_since_epoch.count() < max_timespec_secs) {
            sp.tv_sec = secs_since_epoch.count();
            sp.tv_nsec = static_cast<decltype(sp.tv_nsec)>(
                 (nanos_since_epoch - secs_since_epoch).count());
        } else {
            sp.tv_sec = max_timespec_secs;
            sp.tv_nsec = 999999999;
        }
        return !bthread_mutex_timedlock(&_mutex, &sp);
    }

private:
    bthread_mutex_t _mutex;
};

}  // namespace bthread
