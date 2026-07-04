// The spec for a01-value-semantics.
#include "course/running_stats.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace {

using course::RunningStats;

RunningStats stats_of(const std::vector<double>& xs) {
    RunningStats s;
    for (double x : xs) s.add(x);
    return s;
}

TEST(RunningStats, EmptyHasZeroCount) {
    const RunningStats s;  // note: usable through const — accessors must be const
    EXPECT_EQ(s.count(), 0);
}

TEST(RunningStats, SingleObservation) {
    const auto s = stats_of({42.5});
    EXPECT_EQ(s.count(), 1);
    EXPECT_DOUBLE_EQ(s.mean(), 42.5);
    EXPECT_DOUBLE_EQ(s.min(), 42.5);
    EXPECT_DOUBLE_EQ(s.max(), 42.5);
    EXPECT_DOUBLE_EQ(s.variance(), 0.0);
}

TEST(RunningStats, BasicAggregates) {
    const auto s = stats_of({2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0});
    EXPECT_EQ(s.count(), 8);
    EXPECT_DOUBLE_EQ(s.mean(), 5.0);
    EXPECT_DOUBLE_EQ(s.min(), 2.0);
    EXPECT_DOUBLE_EQ(s.max(), 9.0);
    EXPECT_DOUBLE_EQ(s.variance(), 4.0);  // the textbook example
}

TEST(RunningStats, NegativeValuesHandled) {
    const auto s = stats_of({-5.0, 5.0});
    EXPECT_DOUBLE_EQ(s.mean(), 0.0);
    EXPECT_DOUBLE_EQ(s.min(), -5.0);
    EXPECT_DOUBLE_EQ(s.max(), 5.0);
    EXPECT_DOUBLE_EQ(s.variance(), 25.0);
}

// Kills the naive sum-of-squares implementation: same spread, huge offset.
// Welford passes with tiny tolerance; sum-of-squares cancels catastrophically.
TEST(RunningStats, ShiftedDataHasSameVariance) {
    const double offset = 1e9;
    const auto s = stats_of({offset + 4.0, offset + 7.0, offset + 13.0,
                             offset + 16.0});
    EXPECT_NEAR(s.variance(), 22.5, 1e-6);
    EXPECT_NEAR(s.mean(), offset + 10.0, 1e-3);
}

TEST(RunningStats, CopiesAreIndependent) {
    auto a = stats_of({1.0, 2.0, 3.0});
    RunningStats b = a;  // value semantics: this is a copy
    b.add(100.0);
    EXPECT_EQ(a.count(), 3);
    EXPECT_EQ(b.count(), 4);
    EXPECT_DOUBLE_EQ(a.mean(), 2.0);
    EXPECT_DOUBLE_EQ(a.max(), 3.0);
    EXPECT_DOUBLE_EQ(b.max(), 100.0);
}

TEST(RunningStats, MergeEqualsConcatenation) {
    const std::vector<double> xs = {1.0, 5.5, -2.0, 8.25};
    const std::vector<double> ys = {3.0, 3.0, 100.0};

    auto merged = stats_of(xs);
    const auto rhs = stats_of(ys);
    merged.merge(rhs);

    std::vector<double> all = xs;
    all.insert(all.end(), ys.begin(), ys.end());
    const auto expected = stats_of(all);

    EXPECT_EQ(merged.count(), expected.count());
    EXPECT_NEAR(merged.mean(), expected.mean(), 1e-9);
    EXPECT_NEAR(merged.variance(), expected.variance(), 1e-9);
    EXPECT_DOUBLE_EQ(merged.min(), expected.min());
    EXPECT_DOUBLE_EQ(merged.max(), expected.max());

    // merge() must not touch its argument.
    EXPECT_EQ(rhs.count(), 3);
    EXPECT_DOUBLE_EQ(rhs.max(), 100.0);
}

TEST(RunningStats, MergeWithEmptyIsIdentity) {
    auto s = stats_of({1.0, 2.0});
    s.merge(RunningStats{});  // empty rhs
    EXPECT_EQ(s.count(), 2);
    EXPECT_DOUBLE_EQ(s.mean(), 1.5);

    RunningStats empty;
    empty.merge(s);  // empty lhs
    EXPECT_EQ(empty.count(), 2);
    EXPECT_DOUBLE_EQ(empty.mean(), 1.5);
    EXPECT_DOUBLE_EQ(empty.min(), 1.0);
}

}  // namespace
