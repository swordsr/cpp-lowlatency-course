#pragma once

#include <array>
#include <chrono>
#include <cstdint>

namespace course {

/// 64-bucket log2 latency histogram (HdrHistogram's little cousin).
/// Bucket i counts samples whose bit_width is i: bucket 0 holds value 0,
/// bucket 1 holds 1, bucket 2 holds 2-3, bucket 3 holds 4-7, ...
/// Relative error <= 2x; record() is O(1) and must stay under ~5ns.
/// min()/max() are tracked exactly. Used by both capstones — build it well.
class LatencyHistogram {
public:
    /// O(1). Negative inputs clamp to bucket 0.
    void record(std::int64_t ns) {
        // TODO (Hint 1).
        (void)ns;
    }

    std::int64_t count() const {
        // TODO
        return 0;
    }

    /// Exact. 0 when empty.
    std::int64_t min() const {
        // TODO
        return 0;
    }

    /// Exact. 0 when empty.
    std::int64_t max() const {
        // TODO
        return 0;
    }

    /// Upper bound of the bucket holding the sample of rank
    /// ceil(p/100 * count), rank 1 = smallest. p in (0, 100]. 0 when
    /// empty. (README THEORY §3, Hint 2.)
    std::int64_t percentile(double p) const {
        // TODO
        (void)p;
        return 0;
    }

    /// Fold another histogram in: counters add, min/max combine.
    void merge(const LatencyHistogram& other) {
        // TODO
        (void)other;
    }

private:
    std::array<std::int64_t, 64> buckets_{};
    std::int64_t count_ = 0;
    std::int64_t min_ = 0;
    std::int64_t max_ = 0;
};

/// RAII: records the elapsed wall time (steady_clock, ns) of a scope into
/// a histogram — m01a03's ScopeTimer, grown up.
class TimedSection {
public:
    explicit TimedSection(LatencyHistogram& hist) : hist_(hist) {
        // TODO: stamp the start.
    }

    ~TimedSection() {
        // TODO: record the elapsed nanoseconds.
    }

    TimedSection(const TimedSection&) = delete;
    TimedSection& operator=(const TimedSection&) = delete;

private:
    LatencyHistogram& hist_;
    std::chrono::steady_clock::time_point start_;
};

}  // namespace course
