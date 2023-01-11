// Copyright 2016-2023 Bytedance Ltd. and/or its affiliates
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


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

__attribute__((no_sanitize("thread")))
Histogram::~Histogram()
{
    for (size_t i = 0; i < NUM_BUCKETS; ++i)
    {
        if (_buckets[i])
            delete _buckets[i];
    }
}

__attribute__((no_sanitize("thread")))
uint64_t Histogram::get_value(size_t i) const
{
    if (_buckets[i] != NULL)
        return _buckets[i]->added_count();
    else
        return 0;
}

__attribute__((no_sanitize("thread")))
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
