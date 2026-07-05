// The spec for m04a01. Scales used throughout: Price = 1e4
// (US-equity style), FxPrice = 1e5.
#include "course/fixed.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

namespace {

using course::Fixed;
using course::midpoint;
using course::round_down_to_tick;

using Price = Fixed<10'000>;
using FxPrice = Fixed<100'000>;

TEST(FixedBasics, FactoriesAndAccessors) {
    const auto p = Price::from_ticks(1'551'000);
    EXPECT_EQ(p.ticks(), 1'551'000);
    EXPECT_EQ(p.units_truncated(), 155);

    const auto u = Price::from_units(155);
    EXPECT_EQ(u.ticks(), 1'550'000);

    EXPECT_EQ(Price{}.ticks(), 0);
}

TEST(FixedBasics, UnitsTruncateTowardZero) {
    EXPECT_EQ(Price::from_ticks(1'559'999).units_truncated(), 155);
    EXPECT_EQ(Price::from_ticks(-1'559'999).units_truncated(), -155)
        << "truncation is toward zero, not toward -inf";
    EXPECT_EQ(Price::from_ticks(-9'999).units_truncated(), 0);
}

TEST(FixedArithmetic, SameScaleAddSub) {
    const auto a = Price::from_ticks(1'000'000);
    const auto b = Price::from_ticks(250'000);
    EXPECT_EQ((a + b).ticks(), 1'250'000);
    EXPECT_EQ((a - b).ticks(), 750'000);
    EXPECT_EQ((b - a).ticks(), -750'000);
}

TEST(FixedArithmetic, ScaleByQuantityAndDivide) {
    const auto p = Price::from_ticks(1'551'000);
    EXPECT_EQ((p * 100).ticks(), 155'100'000) << "price x qty = notional";
    EXPECT_EQ((p / 2).ticks(), 775'500);
    EXPECT_EQ((Price::from_ticks(-15) / 10).ticks(), -1)
        << "integer division truncates toward zero (C++11 rule)";
}

TEST(FixedComparisons, AllSixOperators) {
    const auto lo = Price::from_ticks(1);
    const auto hi = Price::from_ticks(2);
    EXPECT_TRUE(lo == lo);
    EXPECT_TRUE(lo != hi);
    EXPECT_TRUE(lo < hi);
    EXPECT_TRUE(lo <= hi);
    EXPECT_TRUE(lo <= lo);
    EXPECT_TRUE(hi > lo);
    EXPECT_TRUE(hi >= hi);
    EXPECT_FALSE(hi < lo);
}

TEST(FixedComparisons, SortsInContainers) {
    std::vector<Price> v = {Price::from_ticks(3), Price::from_ticks(1),
                            Price::from_ticks(2)};
    std::sort(v.begin(), v.end());  // needs operator< via <=>
    EXPECT_EQ(v[0].ticks(), 1);
    EXPECT_EQ(v[2].ticks(), 3);
}

TEST(FixedRescale, UpscaleIsExact) {
    const auto p = Price::from_ticks(1'551'000);  // 155.1 at 1e4
    const auto fx = p.rescale<100'000>();         // -> 1e5
    EXPECT_EQ(fx.ticks(), 15'510'000);
    EXPECT_TRUE((std::is_same_v<decltype(fx), const FxPrice>));
}

TEST(FixedRescale, DownscaleTruncatesTowardZero) {
    const auto fx = FxPrice::from_ticks(15'510'009);  // 155.10009 at 1e5
    EXPECT_EQ(fx.rescale<10'000>().ticks(), 1'551'000);

    const auto neg = FxPrice::from_ticks(-15'510'009);
    EXPECT_EQ(neg.rescale<10'000>().ticks(), -1'551'000)
        << "toward zero, not toward -inf";
}

TEST(FixedRescale, RoundTripThroughFinerScaleIsLossless) {
    const auto p = Price::from_ticks(1'234'567);
    const auto round_trip = p.rescale<100'000>().rescale<10'000>();
    EXPECT_EQ(round_trip.ticks(), p.ticks());
}

TEST(FreeFunctions, MidpointAvoidsOverflow) {
    const auto lo = Price::from_ticks(1'000'000);
    const auto hi = Price::from_ticks(2'000'000);
    EXPECT_EQ(midpoint(lo, hi).ticks(), 1'500'000);

    // Near-INT64_MAX operands: the naive (a+b)/2 overflows (UB — the asan
    // preset's UBSan will call it out); a + (b-a)/2 must not.
    const auto big = Price::from_ticks(9'000'000'000'000'000'000LL);
    EXPECT_EQ(midpoint(big, big).ticks(), 9'000'000'000'000'000'000LL);

    // Truncation bias is toward the first argument.
    EXPECT_EQ(midpoint(Price::from_ticks(0), Price::from_ticks(3)).ticks(), 1);
    EXPECT_EQ(midpoint(Price::from_ticks(3), Price::from_ticks(0)).ticks(), 2);
}

TEST(FreeFunctions, RoundDownToTick) {
    // Tick size of 25 ticks (a quarter-cent grid at scale 1e4).
    EXPECT_EQ(round_down_to_tick<25>(Price::from_ticks(1'551'013)).ticks(),
              1'551'000);
    EXPECT_EQ(round_down_to_tick<25>(Price::from_ticks(1'551'000)).ticks(),
              1'551'000)
        << "already on the grid: unchanged";
}

TEST(FreeFunctions, RoundDownToTickGoesTowardMinusInfinityForNegatives) {
    // -3 ticks on a 25-tick grid: the largest multiple <= -3 is -25.
    EXPECT_EQ(round_down_to_tick<25>(Price::from_ticks(-3)).ticks(), -25)
        << "truncation toward zero would give 0, which is ABOVE the price";
    EXPECT_EQ(round_down_to_tick<25>(Price::from_ticks(-25)).ticks(), -25);
}

}  // namespace
