// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

// Date: 2014/09/22 11:57:43

#include <cstdint>
#include <gflags/gflags.h>
#include "butil/unique_ptr.h"
#include "bvar/latency_histogram_recorder.h"
#include "bvar/passive_status.h"

namespace bvar {

namespace detail {

static int64_t get_window_recorder_qps(void* arg) {
    detail::Sample<Stat> s;
    static_cast<RecorderWindow*>(arg)->get_span(1, &s);
    // Use floating point to avoid overflow.
    if (s.time_us <= 0) {
        return 0;
    }
    return static_cast<int64_t>(round(s.data.num * 1000000.0 / s.time_us));
}

static int64_t get_recorder_count(void* arg) {
    return static_cast<IntRecorder*>(arg)->get_value().num;
}

template <size_t index>
static uint64_t get_histogram(void *arg)
{
        Histogram* histogram = static_cast<Histogram*>(arg);
        return histogram->get_value(index);
}

LatencyHistogramRecorderBase::LatencyHistogramRecorderBase(time_t window_size)
    : _max_latency(0)
    , _latency_window(&_latency, window_size)
    , _max_latency_window(&_max_latency, window_size)
    , _count(get_recorder_count, &_latency)
    , _qps(get_window_recorder_qps, &_latency_window)
    , _latency_bucket_0(get_histogram<0>, &_histogram)
    , _latency_bucket_1(get_histogram<1>, &_histogram)
    , _latency_bucket_2(get_histogram<2>, &_histogram)
    , _latency_bucket_3(get_histogram<3>, &_histogram)
    , _latency_bucket_4(get_histogram<4>, &_histogram)
    , _latency_bucket_5(get_histogram<5>, &_histogram)
    , _latency_bucket_6(get_histogram<6>, &_histogram)
    , _latency_bucket_7(get_histogram<7>, &_histogram)
    , _latency_bucket_8(get_histogram<8>, &_histogram)
    , _latency_bucket_9(get_histogram<9>, &_histogram)
{}

}  // namespace detail

int64_t LatencyHistogramRecorder::qps(time_t window_size) const {
    detail::Sample<Stat> s;
    _latency_window.get_span(window_size, &s);
    // Use floating point to avoid overflow.
    if (s.time_us <= 0) {
        return 0;
    }
    return static_cast<int64_t>(round(s.data.num * 1000000.0 / s.time_us));
}

int LatencyHistogramRecorder::expose(const butil::StringPiece& prefix1,
                            const butil::StringPiece& prefix2) {
    if (prefix2.empty()) {
        LOG(ERROR) << "Parameter[prefix2] is empty";
        return -1;
    }
    butil::StringPiece prefix = prefix2;
    // User may add "_latency" as the suffix, remove it.
    if (prefix.ends_with("latency") || prefix.ends_with("Latency")) {
        prefix.remove_suffix(7);
        if (prefix.empty()) {
            LOG(ERROR) << "Invalid prefix2=" << prefix2;
            return -1;
        }
    }
    std::string tmp;
    if (!prefix1.empty()) {
        tmp.reserve(prefix1.size() + prefix.size() + 1);
        tmp.append(prefix1.data(), prefix1.size());
        tmp.push_back('_'); // prefix1 ending with _ is good.
        tmp.append(prefix.data(), prefix.size());
        prefix = tmp;
    }

    // set debug names for printing helpful error log.
    _latency.set_debug_name(prefix);

    if (_latency_window.expose_as(prefix, "latency") != 0) {
        return -1;
    }
    if (_max_latency_window.expose_as(prefix, "max_latency") != 0) {
        return -1;
    }
    if (_count.expose_as(prefix, "count") != 0) {
        return -1;
    }
    if (_qps.expose_as(prefix, "qps") != 0) {
        return -1;
    }
    _histogram.set_debug_name(prefix);
    if (_latency_bucket_0.expose_as(prefix, "latency_bucket_0", DISPLAY_ON_PLAIN_TEXT) != 0)
        return -1;
    if (_latency_bucket_1.expose_as(prefix, "latency_bucket_1", DISPLAY_ON_PLAIN_TEXT) != 0)
        return -1;
    if (_latency_bucket_2.expose_as(prefix, "latency_bucket_2", DISPLAY_ON_PLAIN_TEXT) != 0)
        return -1;
    if (_latency_bucket_3.expose_as(prefix, "latency_bucket_3", DISPLAY_ON_PLAIN_TEXT) != 0)
        return -1;
    if (_latency_bucket_4.expose_as(prefix, "latency_bucket_4", DISPLAY_ON_PLAIN_TEXT) != 0)
        return -1;
    if (_latency_bucket_5.expose_as(prefix, "latency_bucket_5", DISPLAY_ON_PLAIN_TEXT) != 0)
        return -1;
    if (_latency_bucket_6.expose_as(prefix, "latency_bucket_6", DISPLAY_ON_PLAIN_TEXT) != 0)
        return -1;
    if (_latency_bucket_7.expose_as(prefix, "latency_bucket_7", DISPLAY_ON_PLAIN_TEXT) != 0)
        return -1;
    if (_latency_bucket_8.expose_as(prefix, "latency_bucket_8", DISPLAY_ON_PLAIN_TEXT) != 0)
        return -1;
    if (_latency_bucket_9.expose_as(prefix, "latency_bucket_9", DISPLAY_ON_PLAIN_TEXT) != 0)
        return -1;
    return 0;
}

void LatencyHistogramRecorder::hide() {
    _latency_window.hide();
    _max_latency_window.hide();
    _count.hide();
    _qps.hide();
    _latency_bucket_0.hide();
    _latency_bucket_1.hide();
    _latency_bucket_2.hide();
    _latency_bucket_3.hide();
    _latency_bucket_4.hide();
    _latency_bucket_5.hide();
    _latency_bucket_6.hide();
    _latency_bucket_7.hide();
    _latency_bucket_8.hide();
    _latency_bucket_9.hide();
}

LatencyHistogramRecorder& LatencyHistogramRecorder::operator<<(int64_t latency) {
    _latency << latency;
    _max_latency << latency;
    _histogram << latency;
    return *this;
}

std::ostream& operator<<(std::ostream& os, const LatencyHistogramRecorder& rec) {
    return os << "{latency=" << rec.latency()
              << " max" << rec.window_size() << '=' << rec.max_latency()
              << " qps=" << rec.qps()
              << " count=" << rec.count() << '}';
}

}  // namespace bvar
