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

#ifndef  BVAR_LATENCY_HISTOGRAM_RECORDER_H
#define  BVAR_LATENCY_HISTOGRAM_RECORDER_H

#include "bvar/recorder.h"
#include "bvar/reducer.h"
#include "bvar/passive_status.h"
#include "bvar/detail/histogram.h"

namespace bvar {
namespace detail {

typedef Window<IntRecorder, SERIES_IN_SECOND> RecorderWindow;
typedef Window<Maxer<int64_t>, SERIES_IN_SECOND> MaxWindow;

// NOTE: Always use int64_t in the interfaces no matter what the impl. is.

// For mimic constructor inheritance.
class LatencyHistogramRecorderBase {
public:
    explicit LatencyHistogramRecorderBase(time_t window_size);
    time_t window_size() const { return _latency_window.window_size(); }
protected:
    IntRecorder _latency;
    Maxer<int64_t> _max_latency;
    Histogram _histogram;

    RecorderWindow _latency_window;
    MaxWindow _max_latency_window;
    PassiveStatus<int64_t> _count;
    PassiveStatus<int64_t> _qps;
    PassiveStatus<uint64_t> _latency_bucket_0;
    PassiveStatus<uint64_t> _latency_bucket_1;
    PassiveStatus<uint64_t> _latency_bucket_2;
    PassiveStatus<uint64_t> _latency_bucket_3;
    PassiveStatus<uint64_t> _latency_bucket_4;
    PassiveStatus<uint64_t> _latency_bucket_5;
    PassiveStatus<uint64_t> _latency_bucket_6;
    PassiveStatus<uint64_t> _latency_bucket_7;
    PassiveStatus<uint64_t> _latency_bucket_8;
    PassiveStatus<uint64_t> _latency_bucket_9;
};
} // namespace detail

// Specialized structure to record latency.
// It's not a Variable, but it contains multiple bvar inside.
class LatencyHistogramRecorder : public detail::LatencyHistogramRecorderBase {
    typedef detail::LatencyHistogramRecorderBase Base;
public:
    LatencyHistogramRecorder() : Base(-1) {}
    explicit LatencyHistogramRecorder(time_t window_size) : Base(window_size) {}
    explicit LatencyHistogramRecorder(const butil::StringPiece& prefix) : Base(-1) {
        expose(prefix);
    }
    LatencyHistogramRecorder(const butil::StringPiece& prefix,
                    time_t window_size) : Base(window_size) {
        expose(prefix);
    }
    LatencyHistogramRecorder(const butil::StringPiece& prefix1,
                    const butil::StringPiece& prefix2) : Base(-1) {
        expose(prefix1, prefix2);
    }
    LatencyHistogramRecorder(const butil::StringPiece& prefix1,
                    const butil::StringPiece& prefix2,
                    time_t window_size) : Base(window_size) {
        expose(prefix1, prefix2);
    }

    ~LatencyHistogramRecorder() { hide(); }

    // Record the latency.
    LatencyHistogramRecorder& operator<<(int64_t latency);
        
    // Expose all internal variables using `prefix' as prefix.
    // Returns 0 on success, -1 otherwise.
    // Example:
    //   LatencyHistogramRecorder rec;
    //   rec.expose("foo_bar_write");     // foo_bar_write_latency
    //                                    // foo_bar_write_max_latency
    //                                    // foo_bar_write_count
    //                                    // foo_bar_write_qps
    //   rec.expose("foo_bar", "read");   // foo_bar_read_latency
    //                                    // foo_bar_read_max_latency
    //                                    // foo_bar_read_count
    //                                    // foo_bar_read_qps
    int expose(const butil::StringPiece& prefix) {
        return expose(butil::StringPiece(), prefix);
    }
    int expose(const butil::StringPiece& prefix1,
               const butil::StringPiece& prefix2);
    
    // Hide all internal variables, called in dtor as well.
    void hide();

    // Get the average latency in recent |window_size| seconds
    // If |window_size| is absent, use the window_size to ctor.
    int64_t latency(time_t window_size) const
    { return _latency_window.get_value(window_size).get_average_int(); }
    int64_t latency() const
    { return _latency_window.get_value().get_average_int(); }


    // Get the max latency in recent window_size-to-ctor seconds.
    int64_t max_latency() const { return _max_latency_window.get_value(); }

    // Get the total number of recorded latencies.
    int64_t count() const { return _latency.get_value().num; }

    // Get qps in recent |window_size| seconds. The `q' means latencies
    // recorded by operator<<().
    // If |window_size| is absent, use the window_size to ctor.
    int64_t qps(time_t window_size) const;
    int64_t qps() const { return _qps.get_value(); }

    // Get name of a sub-bvar.
    const std::string& latency_name() const { return _latency_window.name(); }
    const std::string& max_latency_name() const
    { return _max_latency_window.name(); }
    const std::string& count_name() const { return _count.name(); }
    const std::string& qps_name() const { return _qps.name(); }
    const std::string& latency_bucket_0_name() const
    { return _latency_bucket_0.name(); }
    const std::string& latency_bucket_1_name() const
    { return _latency_bucket_1.name(); }
    const std::string& latency_bucket_2_name() const
    { return _latency_bucket_2.name(); }
    const std::string& latency_bucket_3_name() const
    { return _latency_bucket_3.name(); }
    const std::string& latency_bucket_4_name() const
    { return _latency_bucket_4.name(); }
    const std::string& latency_bucket_5_name() const
    { return _latency_bucket_5.name(); }
    const std::string& latency_bucket_6_name() const
    { return _latency_bucket_6.name(); }
    const std::string& latency_bucket_7_name() const
    { return _latency_bucket_7.name(); }
    const std::string& latency_bucket_8_name() const
    { return _latency_bucket_8.name(); }
    const std::string& latency_bucket_9_name() const
    { return _latency_bucket_9.name(); }
};

std::ostream& operator<<(std::ostream& os, const LatencyHistogramRecorder&);

}  // namespace bvar

#endif  //BVAR_LATENCY_HISTOGRAM_RECORDER_H
