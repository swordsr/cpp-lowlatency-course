// The spec for m10a02. Every scenario also asserts the engine's
// postcondition: the book is never left crossed. NOTE: these tests
// exercise your a01 book underneath — a failure here with a green a01
// is an engine bug; a failure that smells like bookkeeping is a01's.
#include "course/matching_engine.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <random>
#include <vector>

namespace {

using namespace course;

void expect_uncrossed(const OrderBook& book, const char* where) {
    const auto bid = book.best_bid();
    const auto ask = book.best_ask();
    if (bid && ask) {
        EXPECT_LT(*bid, *ask) << "book left CROSSED after " << where;
    }
}

TEST(Matching, NonCrossingOrderRests) {
    MatchingEngine eng;
    EXPECT_TRUE(eng.submit(1, Side::Bid, 100, 10).empty());
    EXPECT_TRUE(eng.submit(2, Side::Ask, 105, 5).empty());
    EXPECT_EQ(eng.book().best_bid(), 100);
    EXPECT_EQ(eng.book().best_ask(), 105);
    EXPECT_EQ(eng.book().order_count(), 2u);
    expect_uncrossed(eng.book(), "two resting orders");
}

TEST(Matching, ExactCrossFullyFillsBoth) {
    MatchingEngine eng;
    eng.submit(1, Side::Ask, 100, 10);
    const auto trades = eng.submit(2, Side::Bid, 100, 10);

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].maker_id, 1u);
    EXPECT_EQ(trades[0].taker_id, 2u);
    EXPECT_EQ(trades[0].price, 100);
    EXPECT_EQ(trades[0].qty, 10u);
    EXPECT_EQ(eng.book().order_count(), 0u) << "both sides fully filled";
    expect_uncrossed(eng.book(), "exact cross");
}

TEST(Matching, TradesPrintAtTheMakersPrice) {
    MatchingEngine eng;
    eng.submit(1, Side::Ask, 100, 5);
    const auto trades = eng.submit(2, Side::Bid, 103, 5);  // aggressive bid
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].price, 100)
        << "price improvement: the maker's level, never the taker's limit";
}

TEST(Matching, PartialFillRestsTheRemainder) {
    MatchingEngine eng;
    eng.submit(1, Side::Ask, 100, 4);
    const auto trades = eng.submit(2, Side::Bid, 100, 10);
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].qty, 4u);
    EXPECT_EQ(eng.book().best_bid(), 100) << "remainder (6) must rest";
    EXPECT_EQ(eng.book().qty_at(Side::Bid, 100), 6u);
    EXPECT_EQ(eng.book().best_ask(), std::nullopt);
    expect_uncrossed(eng.book(), "partial fill");
}

TEST(Matching, FifoTimePriorityWithinALevel) {
    MatchingEngine eng;
    eng.submit(1, Side::Ask, 100, 5);  // older
    eng.submit(2, Side::Ask, 100, 5);  // newer
    const auto trades = eng.submit(3, Side::Bid, 100, 7);

    ASSERT_EQ(trades.size(), 2u);
    EXPECT_EQ(trades[0].maker_id, 1u) << "older order fills FIRST";
    EXPECT_EQ(trades[0].qty, 5u);
    EXPECT_EQ(trades[1].maker_id, 2u);
    EXPECT_EQ(trades[1].qty, 2u);
    EXPECT_EQ(eng.book().quantity_of(2), 3u) << "newer keeps the remainder";
}

TEST(Matching, SweepsMultipleLevelsInPriceOrder) {
    MatchingEngine eng;
    eng.submit(1, Side::Ask, 100, 5);
    eng.submit(2, Side::Ask, 101, 5);
    eng.submit(3, Side::Ask, 103, 5);
    const auto trades = eng.submit(4, Side::Bid, 101, 12);

    ASSERT_EQ(trades.size(), 2u) << "103 does not cross a 101 bid";
    EXPECT_EQ(trades[0].price, 100) << "best level first";
    EXPECT_EQ(trades[1].price, 101);
    EXPECT_EQ(trades[0].qty + trades[1].qty, 10u);
    EXPECT_EQ(eng.book().qty_at(Side::Bid, 101), 2u) << "remainder rests";
    EXPECT_EQ(eng.book().best_ask(), 103);
    expect_uncrossed(eng.book(), "multi-level sweep");
}

TEST(Matching, AskAggressorMirrors) {
    MatchingEngine eng;
    eng.submit(1, Side::Bid, 102, 5);
    eng.submit(2, Side::Bid, 101, 5);
    const auto trades = eng.submit(3, Side::Ask, 101, 8);

    ASSERT_EQ(trades.size(), 2u);
    EXPECT_EQ(trades[0].maker_id, 1u);
    EXPECT_EQ(trades[0].price, 102) << "ask hits the HIGHEST bid first";
    EXPECT_EQ(trades[1].price, 101);
    EXPECT_EQ(trades[1].qty, 3u);
    EXPECT_EQ(eng.book().quantity_of(2), 2u);
    expect_uncrossed(eng.book(), "ask sweep");
}

TEST(Matching, EqualityBoundariesCross) {
    // Both boundary directions (Hint 3).
    {
        MatchingEngine eng;
        eng.submit(1, Side::Ask, 100, 1);
        EXPECT_EQ(eng.submit(2, Side::Bid, 100, 1).size(), 1u)
            << "bid AT the ask crosses";
    }
    {
        MatchingEngine eng;
        eng.submit(1, Side::Bid, 100, 1);
        EXPECT_EQ(eng.submit(2, Side::Ask, 100, 1).size(), 1u)
            << "ask AT the bid crosses";
    }
    {
        MatchingEngine eng;
        eng.submit(1, Side::Ask, 100, 1);
        EXPECT_TRUE(eng.submit(2, Side::Bid, 99, 1).empty())
            << "one tick under does NOT cross";
    }
}

TEST(Matching, CancelRemovesLiquidityBeforeItTrades) {
    MatchingEngine eng;
    eng.submit(1, Side::Ask, 100, 5);
    ASSERT_TRUE(eng.cancel(1));
    const auto trades = eng.submit(2, Side::Bid, 100, 5);
    EXPECT_TRUE(trades.empty()) << "cancelled maker must not trade";
    EXPECT_EQ(eng.book().qty_at(Side::Bid, 100), 5u);
}

TEST(Matching, ReduceKeepsQueuePriority) {
    MatchingEngine eng;
    eng.submit(1, Side::Ask, 100, 10);
    eng.submit(2, Side::Ask, 100, 10);
    ASSERT_TRUE(eng.reduce(1, 5));  // shrink the OLDER order
    const auto trades = eng.submit(3, Side::Bid, 100, 6);
    ASSERT_EQ(trades.size(), 2u);
    EXPECT_EQ(trades[0].maker_id, 1u)
        << "reduce must not cost queue position (cancel+resubmit would)";
    EXPECT_EQ(trades[0].qty, 5u);
    EXPECT_EQ(trades[1].maker_id, 2u);
    EXPECT_EQ(trades[1].qty, 1u);
}

TEST(Matching, RejectsDuplicateAndZeroQty) {
    MatchingEngine eng;
    eng.submit(1, Side::Bid, 100, 5);
    EXPECT_TRUE(eng.submit(1, Side::Ask, 105, 5).empty()) << "id 1 is live";
    EXPECT_EQ(eng.book().best_ask(), std::nullopt);
    EXPECT_TRUE(eng.submit(2, Side::Bid, 100, 0).empty());
    EXPECT_EQ(eng.book().order_count(), 1u);
}

TEST(Matching, RandomizedConservationAudit) {
    // 5k random submits/cancels. After every op: book uncrossed; at the
    // end: every submitted share is accounted for as filled (twice — a
    // maker share and a taker share per trade), resting, or cancelled.
    MatchingEngine eng;
    std::mt19937 rng{20260710};
    std::uniform_int_distribution<int> op_dist(0, 99);
    std::uniform_int_distribution<Price> price_dist(95, 105);
    std::uniform_int_distribution<Qty> qty_dist(1, 20);

    OrderId next_id = 1;
    std::vector<OrderId> live;
    std::uint64_t submitted = 0, filled_x2 = 0, cancelled = 0;

    for (int op = 0; op < 5'000; ++op) {
        if (op_dist(rng) < 75 || live.empty()) {
            const OrderId id = next_id++;
            const Side side = (op_dist(rng) % 2 == 0) ? Side::Bid : Side::Ask;
            const Qty qty = qty_dist(rng);
            submitted += qty;
            const auto trades = eng.submit(id, side, price_dist(rng), qty);
            for (const auto& t : trades) filled_x2 += 2ull * t.qty;
            if (eng.book().quantity_of(id).has_value()) live.push_back(id);
            // Makers may have died; refresh lazily below.
        } else {
            std::uniform_int_distribution<std::size_t> pick(0, live.size() - 1);
            const OrderId id = live[pick(rng)];
            if (const auto q = eng.book().quantity_of(id)) {
                ASSERT_TRUE(eng.cancel(id));
                cancelled += *q;
            }
        }
        // Compact the live list of anything matching killed.
        std::erase_if(live, [&](OrderId id) {
            return !eng.book().quantity_of(id).has_value();
        });

        const auto bid = eng.book().best_bid();
        const auto ask = eng.book().best_ask();
        if (bid && ask) {
            ASSERT_LT(*bid, *ask) << "crossed book after op " << op;
        }
    }

    std::uint64_t resting = 0;
    for (OrderId id : live) resting += *eng.book().quantity_of(id);
    EXPECT_EQ(submitted, filled_x2 + resting + cancelled)
        << "shares leaked: every share is filled (counted on both sides), "
           "resting, or cancelled — no fourth fate";
}

}  // namespace
