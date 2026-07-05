// The spec for m06a01. The bucket layout (bit_width buckets) and the
// rank-based percentile semantics are pinned exactly — the capstones
// depend on this histogram behaving precisely this way.
#include "course/latency_histogram.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

namespace {

using course::LatencyHistogram;
using course::TimedSection;

TEST(Histogram, EmptyReportsZeros) {
    const LatencyHistogram h;
    EXPECT_EQ(h.count(), 0);
    EXPECT_EQ(h.min(), 0);
    EXPECT_EQ(h.max(), 0);
    EXPECT_EQ(h.percentile(50), 0);
}

TEST(Histogram, CountsAndExactMinMax) {
    LatencyHistogram h;
    h.record(800);
    h.record(1'200);
    h.record(750);
    EXPECT_EQ(h.count(), 3);
    EXPECT_EQ(h.min(), 750) << "min is EXACT, not a bucket bound";
    EXPECT_EQ(h.max(), 1'200) << "max is EXACT, not a bucket bound";
}

TEST(Histogram, BucketUpperBounds) {
    // One sample per documented bucket edge; p100 of a single-sample
    // histogram is that sample's bucket upper bound.
    struct Case {
        std::int64_t value;
        std::int64_t bucket_upper;
    };
    for (const auto& c : {Case{0, 0}, Case{1, 1}, Case{2, 3}, Case{3, 3},
                          Case{4, 7}, Case{7, 7}, Case{8, 15},
                          Case{1'000, 1'023}, Case{1'024, 2'047}}) {
        LatencyHistogram h;
        h.record(c.value);
        EXPECT_EQ(h.percentile(100), c.bucket_upper)
            << "value " << c.value << " must land in the bucket capped at "
            << c.bucket_upper;
    }
}

TEST(Histogram, NegativeClampsToBucketZero) {
    LatencyHistogram h;
    h.record(-5);
    EXPECT_EQ(h.count(), 1);
    EXPECT_EQ(h.percentile(100), 0) << "negatives count as bucket 0";
}

TEST(Histogram, PercentileRankSemantics) {
    // 100 samples of value 1..100. Rank r sample is the value r.
    LatencyHistogram h;
    for (int i = 1; i <= 100; ++i) h.record(i);

    // p50 -> rank 50 -> value 50 -> bucket [32,63] -> upper bound 63.
    EXPECT_EQ(h.percentile(50), 63);
    // p1 -> rank 1 -> value 1 -> bucket upper 1.
    EXPECT_EQ(h.percentile(1), 1);
    // p99 -> rank 99 -> value 99 -> bucket [64,127] -> 127.
    EXPECT_EQ(h.percentile(99), 127);
    // p100 -> rank 100 -> value 100 -> same bucket -> 127.
    EXPECT_EQ(h.percentile(100), 127);
    // Tiny p must not round rank down to 0: rank = ceil(0.001*100) = 1.
    EXPECT_EQ(h.percentile(0.001), 1);
}

TEST(Histogram, PercentileWithSkewedMass) {
    // 99 fast ops (value 8: bucket upper 15) + 1 disaster (1'000'000:
    // bucket [2^19..2^20-1] upper 1'048'575). The mean would say ~10k ns
    // and everything is fine; the tail says otherwise. This asymmetry is
    // the whole reason histograms exist (README THEORY §3).
    LatencyHistogram h;
    for (int i = 0; i < 99; ++i) h.record(8);
    h.record(1'000'000);

    EXPECT_EQ(h.percentile(50), 15);
    EXPECT_EQ(h.percentile(99), 15);
    EXPECT_EQ(h.percentile(100), 1'048'575);
    EXPECT_EQ(h.max(), 1'000'000);
}

TEST(Histogram, MergeAddsCountersAndFoldsExtremes) {
    LatencyHistogram fast;
    fast.record(10);
    fast.record(20);
    LatencyHistogram slow;
    slow.record(5'000);

    fast.merge(slow);
    EXPECT_EQ(fast.count(), 3);
    EXPECT_EQ(fast.min(), 10);
    EXPECT_EQ(fast.max(), 5'000);
    EXPECT_EQ(fast.percentile(100), 8'191);  // 5000 lives in [4096, 8191]

    // Merging an empty histogram must not disturb min/max.
    fast.merge(LatencyHistogram{});
    EXPECT_EQ(fast.count(), 3);
    EXPECT_EQ(fast.min(), 10);
}

TEST(TimedSectionTest, RecordsPlausibleElapsedOnScopeExit) {
    LatencyHistogram h;
    {
        TimedSection t{h};
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        EXPECT_EQ(h.count(), 0) << "nothing recorded before scope exit";
    }
    ASSERT_EQ(h.count(), 1);
    EXPECT_GE(h.max(), 5'000'000) << "slept >=10ms; recorded ns too small";
    EXPECT_LT(h.max(), 10'000'000'000) << "implausibly large";
}

TEST(TimedSectionTest, EachSectionRecordsOnce) {
    LatencyHistogram h;
    for (int i = 0; i < 5; ++i) {
        TimedSection t{h};
    }
    EXPECT_EQ(h.count(), 5);
}

}  // namespace
