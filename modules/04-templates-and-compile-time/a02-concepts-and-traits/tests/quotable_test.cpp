// The spec for m04a02. The fixture types below draw the exact boundary
// lines your trait and concepts must reproduce. Concept checks are runtime
// EXPECT_* (not static_assert) so the stub state still builds — the
// course-wide rule.
#include "course/quotable.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

namespace {

using course::has_bid_v;
using course::Quotable;
using course::spread;
using course::TightlyQuotable;
using course::weighted_mid;

// --- fixtures: the ducks ------------------------------------------------------

struct SimpleQuote {  // Quotable, nothing more
    std::int64_t b, a;
    std::int64_t bid() const { return b; }
    std::int64_t ask() const { return a; }
};

struct WideQuote {  // TightlyQuotable
    std::int64_t b, a;
    std::int32_t qb, qa;
    std::int64_t bid() const { return b; }
    std::int64_t ask() const { return a; }
    std::int32_t qty_at_bid() const { return qb; }
    std::int32_t qty_at_ask() const { return qa; }
};

// --- fixtures: the near-misses --------------------------------------------------

struct NoAsk {  // half a quote is not a quote
    std::int64_t bid() const { return 0; }
};

struct NonConstBid {  // right name, wrong const-ness: unusable via const&
    std::int64_t bid() { return 0; }
    std::int64_t ask() const { return 0; }
};

struct WrongReturn {  // right shape, hopeless types
    std::string bid() const { return "155.10"; }
    std::string ask() const { return "155.20"; }
};

// --- Part 1: the void_t detector -------------------------------------------------

TEST(HasBid, DetectsConstCallableBid) {
    EXPECT_TRUE(has_bid_v<SimpleQuote>);
    EXPECT_TRUE(has_bid_v<WideQuote>);
    EXPECT_TRUE(has_bid_v<WrongReturn>) << "the detector asks 'is there a "
                                           "bid()?', not what it returns";
}

TEST(HasBid, RejectsTheDucklessAndTheNonConst) {
    EXPECT_FALSE(has_bid_v<int>);
    EXPECT_FALSE(has_bid_v<NoAsk*>) << "a pointer has no members";
    EXPECT_FALSE(has_bid_v<NonConstBid>)
        << "detection must go through const T (Hint 1)";
}

// --- Part 2: the concepts ---------------------------------------------------------

TEST(QuotableConcept, AcceptsRealQuotes) {
    EXPECT_TRUE(Quotable<SimpleQuote>);
    EXPECT_TRUE(Quotable<WideQuote>);
}

TEST(QuotableConcept, RejectsNearMisses) {
    EXPECT_FALSE(Quotable<int>);
    EXPECT_FALSE(Quotable<NoAsk>);
    EXPECT_FALSE(Quotable<NonConstBid>);
    EXPECT_FALSE(Quotable<WrongReturn>)
        << "compound requirement must constrain the return type";
}

TEST(TightlyQuotableConcept, RefinesQuotable) {
    EXPECT_TRUE(TightlyQuotable<WideQuote>);
    EXPECT_FALSE(TightlyQuotable<SimpleQuote>) << "no quantities, not tight";
    EXPECT_FALSE(TightlyQuotable<NoAsk>);
}

// --- Part 3: constrained functions and subsumption --------------------------------

TEST(Spread, AskMinusBid) {
    EXPECT_EQ(spread(SimpleQuote{1'551'000, 1'551'500}), 500);
    EXPECT_EQ(spread(WideQuote{1'551'000, 1'551'500, 10, 20}), 500);
}

TEST(WeightedMid, PlainQuoteGetsPlainMidpoint) {
    EXPECT_EQ(weighted_mid(SimpleQuote{1'000, 2'001}), 1'500)
        << "(bid+ask)/2, truncating";
}

TEST(WeightedMid, TightQuoteGetsQuantityWeighting) {
    // bid 1000 x 30, ask 2000 x 10: mid pulled toward the heavy side's
    // OPPOSITE price: (1000*10 + 2000*30)/40 = 1750. A plain midpoint
    // (1500) here means subsumption did not select the tight overload.
    EXPECT_EQ(weighted_mid(WideQuote{1'000, 2'000, 30, 10}), 1'750);
}

TEST(WeightedMid, EqualQuantitiesMatchThePlainMidpoint) {
    EXPECT_EQ(weighted_mid(WideQuote{1'000, 2'000, 5, 5}), 1'500);
}

}  // namespace
