#include "bvar/detail/histogram.h"
#include "butil/logging.h"
#include <string.h>                     // memset memcmp

#include <iostream>

namespace bvar {
namespace detail {

Histogram::Histogram()
{
    memset(&_buckets, 0, sizeof(_buckets));
}

Histogram::~Histogram()
{
    for (size_t i = 0; i < NUM_BUCKETS; ++i)
    {
        if (_buckets[i])
            delete _buckets[i];
    }
}

uint64_t Histogram::get_value(size_t i) const
{
    if (_buckets[i] != NULL)
        return _buckets[i]->added_count();
    else
        return 0;
}

Histogram& Histogram::operator<<(int64_t latency)
{
    if (latency < 0) {
        if (!_debug_name.empty()) {
            LOG(WARNING) << "Input=" << latency << " to `" << _debug_name
                       << "' is negative, drop";
        } else {
            LOG(WARNING) << "Input=" << latency << " to Histogram("
                       << (void*)this << ") is negative, drop";
        }
        return *this;
    }
    else {
        for (size_t i = 0; i < NUM_BUCKETS; ++i)
        {
            if (latency <= _latency_buckets_ceilings[i])
            {
                if (_buckets[i] == NULL)
                    _buckets[i] = new HistogramBucket();
                _buckets[i]->increment();
                break;
            }
            if (i == NUM_BUCKETS - 1)
            {
                LOG(WARNING) << "Input=" << latency << " to Histogram("
                       << (void*)this << ") is invalid, drop";

            }
        }
    }

    return *this;
}

}
}
