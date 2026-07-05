// The spec for m06a03: the pairs must agree everywhere, including the
// extremes where mask tricks go wrong (Hint 2).
#include "course/branches.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <vector>

namespace {

using course::count_above_branchless;
using course::count_above_branchy;
using course::sum_above_branchless;
using course::sum_above_branchy;

constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
constexpr std::int64_t kMin = std::numeric_limits<std::int64_t>::min();

const std::vector<std::int64_t> kData = {5, -3, 12, 0, 7, -8, 12, 3};

TEST(CountAbove, BranchyBasics) {
    EXPECT_EQ(count_above_branchy(kData.data(), kData.size(), 4), 4);
    EXPECT_EQ(count_above_branchy(kData.data(), kData.size(), 12), 0)
        << "strictly above: 12 > 12 is false";
    EXPECT_EQ(count_above_branchy(kData.data(), kData.size(), -100), 8);
    EXPECT_EQ(count_above_branchy(kData.data(), 0, 0), 0);
}

TEST(CountAbove, BranchlessMatchesBranchyEverywhere) {
    const std::int64_t thresholds[] = {-100, -8, 0, 3, 4, 12, 100, kMax, kMin};
    for (std::int64_t t : thresholds) {
        EXPECT_EQ(count_above_branchless(kData.data(), kData.size(), t),
                  count_above_branchy(kData.data(), kData.size(), t))
            << "disagreement at threshold " << t;
    }
}

TEST(SumAbove, BranchyBasics) {
    EXPECT_EQ(sum_above_branchy(kData.data(), kData.size(), 4), 36)
        << "5 + 12 + 7 + 12";
    EXPECT_EQ(sum_above_branchy(kData.data(), kData.size(), -100), 28);
    EXPECT_EQ(sum_above_branchy(kData.data(), kData.size(), kMax), 0);
}

TEST(SumAbove, BranchlessMatchesIncludingNegatives) {
    // Negative elements above a more-negative threshold must be ADDED —
    // the case where an inverted mask silently zeroes them out.
    const std::vector<std::int64_t> negs = {-1, -5, -9, 4};
    EXPECT_EQ(sum_above_branchless(negs.data(), negs.size(), -6),
              sum_above_branchy(negs.data(), negs.size(), -6));
    EXPECT_EQ(sum_above_branchy(negs.data(), negs.size(), -6), -2)
        << "-1 + -5 + 4";

    const std::int64_t thresholds[] = {-100, 0, 4, kMax, kMin};
    for (std::int64_t t : thresholds) {
        EXPECT_EQ(sum_above_branchless(kData.data(), kData.size(), t),
                  sum_above_branchy(kData.data(), kData.size(), t))
            << "disagreement at threshold " << t;
    }
}

TEST(SumAbove, EmptyInput) {
    EXPECT_EQ(sum_above_branchy(kData.data(), 0, 0), 0);
    EXPECT_EQ(sum_above_branchless(kData.data(), 0, 0), 0);
}

}  // namespace
