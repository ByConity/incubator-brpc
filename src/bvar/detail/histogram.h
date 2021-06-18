#ifndef  BVAR_DETAIL_HISTOGRAM_H
#define  BVAR_DETAIL_HISTOGRAM_H

#include <array>
#include <cstdint>
#include <limits>
#include <ostream>                      // std::ostream
#include <stdint.h>                     // uint32_t
#include "butil/logging.h"                // ARRAY_SIZE
#include "butil/macros.h"
#include "butil/type_traits.h"
#include "bvar/vector.h"

#include <iostream>
namespace bvar {
namespace detail
{

class HistogramBucket {
public:
    HistogramBucket()
        : _num_added(0)
    {}

    uint64_t added_count() const {
        return _num_added;
    }

    void describe(std::ostream &os) const {
        os << "(num_added=" << added_count() << ")";
    }

    void increment (int i = 1)
    {
        _num_added += i;
    }

    void clear() {
        _num_added = 0;
    }
private:
    uint64_t _num_added;

};

static const size_t NUM_BUCKETS = 10;
static constexpr int64_t _latency_buckets_ceilings [NUM_BUCKETS] {1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, std::numeric_limits<int64_t>::max()};

class Histogram
{

public:
    Histogram();
    ~Histogram();
    uint64_t get_value(size_t i) const;

    Histogram& operator<<(int64_t latency);
    // This name is useful for warning negative latencies in operator<<
    void set_debug_name(const butil::StringPiece& name) {
        _debug_name.assign(name.data(), name.size());
    }

private:
    HistogramBucket* _buckets[NUM_BUCKETS];
    std::string _debug_name;


};


} //namespace detail
} //namespace bvar

#endif // BVAR_DETAIL_HISTOGRAM_H_
