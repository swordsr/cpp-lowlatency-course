// The spec for m10a01. Correctness-heavy on purpose: a02 and a03 both
// stand on this book, so every invariant is pinned here. The finale is
// a 10k-op randomized differential test with a fixed seed.
#include "course/order_book.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <random>
#include <vector>

namespace {

using namespace course;

TEST(Book, EmptyBook) {
    OrderBook book;
    EXPECT_EQ(book.best_bid(), std::nullopt);
    EXPECT_EQ(book.best_ask(), std::nullopt);
    EXPECT_EQ(book.depth(Side::Bid), 0u);
    EXPECT_EQ(book.order_count(), 0u);
    EXPECT_EQ(book.qty_at(Side::Bid, 100), 0u);
}

TEST(Book, AddSetsBestPerSide) {
    OrderBook book;
    EXPECT_TRUE(book.add(1, Side::Bid, 100, 10));
    EXPECT_TRUE(book.add(2, Side::Ask, 105, 5));
    EXPECT_EQ(book.best_bid(), 100);
    EXPECT_EQ(book.best_ask(), 105);
    EXPECT_EQ(book.order_count(), 2u);
}

TEST(Book, BestIsHighestBidAndLowestAsk) {
    OrderBook book;
    book.add(1, Side::Bid, 100, 1);
    book.add(2, Side::Bid, 102, 1);
    book.add(3, Side::Bid, 101, 1);
    book.add(4, Side::Ask, 110, 1);
    book.add(5, Side::Ask, 108, 1);
    book.add(6, Side::Ask, 109, 1);
    EXPECT_EQ(book.best_bid(), 102) << "bids ladder: best = HIGHEST";
    EXPECT_EQ(book.best_ask(), 108) << "asks ladder: best = LOWEST";
    EXPECT_EQ(book.depth(Side::Bid), 3u);
    EXPECT_EQ(book.depth(Side::Ask), 3u);
}

TEST(Book, LevelAggregatesQuantity) {
    OrderBook book;
    book.add(1, Side::Bid, 100, 10);
    book.add(2, Side::Bid, 100, 25);
    book.add(3, Side::Bid, 99, 7);
    EXPECT_EQ(book.qty_at(Side::Bid, 100), 35u);
    EXPECT_EQ(book.qty_at(Side::Bid, 99), 7u);
    EXPECT_EQ(book.depth(Side::Bid), 2u) << "two orders, one price = one level";
}

TEST(Book, RejectsDuplicateIdAndZeroQty) {
    OrderBook book;
    EXPECT_TRUE(book.add(1, Side::Bid, 100, 10));
    EXPECT_FALSE(book.add(1, Side::Ask, 105, 5)) << "id 1 is live";
    EXPECT_FALSE(book.add(2, Side::Bid, 100, 0)) << "qty 0 is not an order";
    EXPECT_EQ(book.order_count(), 1u);
    EXPECT_EQ(book.qty_at(Side::Bid, 100), 10u) << "failed add must not touch";
}

TEST(Book, CancelRemovesAndEmptyLevelsDisappear) {
    OrderBook book;
    book.add(1, Side::Ask, 105, 10);
    book.add(2, Side::Ask, 105, 5);
    ASSERT_TRUE(book.cancel(1));
    EXPECT_EQ(book.qty_at(Side::Ask, 105), 5u);
    EXPECT_EQ(book.depth(Side::Ask), 1u);
    ASSERT_TRUE(book.cancel(2));
    EXPECT_EQ(book.qty_at(Side::Ask, 105), 0u);
    EXPECT_EQ(book.depth(Side::Ask), 0u) << "empty level must disappear";
    EXPECT_EQ(book.best_ask(), std::nullopt);
}

TEST(Book, CancelUnknownAndDoubleCancel) {
    OrderBook book;
    book.add(1, Side::Bid, 100, 10);
    EXPECT_FALSE(book.cancel(999));
    ASSERT_TRUE(book.cancel(1));
    EXPECT_FALSE(book.cancel(1)) << "an order dies once";
}

TEST(Book, BestMovesWhenBestLevelEmpties) {
    OrderBook book;
    book.add(1, Side::Bid, 102, 5);
    book.add(2, Side::Bid, 100, 5);
    ASSERT_EQ(book.best_bid(), 102);
    book.cancel(1);
    EXPECT_EQ(book.best_bid(), 100) << "best must fall back to next level";
}

TEST(Book, ReducePartialKeepsPosition) {
    OrderBook book;
    book.add(1, Side::Ask, 105, 10);
    book.add(2, Side::Ask, 105, 20);
    ASSERT_TRUE(book.reduce(1, 4));
    EXPECT_EQ(book.quantity_of(1), 6u);
    EXPECT_EQ(book.qty_at(Side::Ask, 105), 26u);
    EXPECT_EQ(book.front_order(Side::Ask, 105), 1u)
        << "reduce keeps queue position — order 1 is still first";
}

TEST(Book, ReduceToZeroRemoves) {
    OrderBook book;
    book.add(1, Side::Bid, 100, 10);
    ASSERT_TRUE(book.reduce(1, 10));
    EXPECT_EQ(book.quantity_of(1), std::nullopt);
    EXPECT_EQ(book.order_count(), 0u);
    EXPECT_EQ(book.depth(Side::Bid), 0u);

    book.add(2, Side::Bid, 100, 5);
    ASSERT_TRUE(book.reduce(2, 99)) << "by > remaining also removes";
    EXPECT_EQ(book.order_count(), 0u);
}

TEST(Book, ReduceRejectsUnknownAndZero) {
    OrderBook book;
    book.add(1, Side::Bid, 100, 10);
    EXPECT_FALSE(book.reduce(999, 1));
    EXPECT_FALSE(book.reduce(1, 0));
    EXPECT_EQ(book.quantity_of(1), 10u);
}

TEST(Book, FrontOrderIsFifo) {
    OrderBook book;
    book.add(10, Side::Bid, 100, 1);
    book.add(11, Side::Bid, 100, 1);
    book.add(12, Side::Bid, 100, 1);
    EXPECT_EQ(book.front_order(Side::Bid, 100), 10u);
    book.cancel(10);
    EXPECT_EQ(book.front_order(Side::Bid, 100), 11u)
        << "front moves to the NEXT oldest, not the newest";
    EXPECT_EQ(book.front_order(Side::Bid, 999), std::nullopt);
}

TEST(Book, CrossedStateIsLegalHere) {
    // a01 stores; it does not match (README THEORY §3). a02 owns
    // uncrossing.
    OrderBook book;
    EXPECT_TRUE(book.add(1, Side::Ask, 100, 5));
    EXPECT_TRUE(book.add(2, Side::Bid, 102, 5));
    EXPECT_EQ(book.best_bid(), 102);
    EXPECT_EQ(book.best_ask(), 100);
    EXPECT_EQ(book.order_count(), 2u);
}

TEST(Book, RandomizedDifferentialAgainstAggregateModel) {
    // 10k random ops mirrored into a naive model: per-side
    // map<price, qty> + map<id, {side, price, qty}>. Any divergence in
    // best/qty_at/depth/count names the op that caused it. Fixed seed:
    // failures reproduce exactly.
    OrderBook book;
    struct ModelOrder {
        Side side;
        Price price;
        Qty qty;
    };
    std::map<Price, Qty> model_bids, model_asks;
    std::map<OrderId, ModelOrder> model_orders;
    std::mt19937 rng{20260705};
    std::uniform_int_distribution<int> op_dist(0, 99);
    std::uniform_int_distribution<Price> price_dist(95, 105);
    std::uniform_int_distribution<Qty> qty_dist(1, 50);
    OrderId next_id = 1;
    std::vector<OrderId> live;

    for (int op = 0; op < 10'000; ++op) {
        const int roll = op_dist(rng);
        if (roll < 55 || live.empty()) {  // add
            const Side side = (roll % 2 == 0) ? Side::Bid : Side::Ask;
            const Price price = price_dist(rng);
            const Qty qty = qty_dist(rng);
            const OrderId id = next_id++;
            ASSERT_TRUE(book.add(id, side, price, qty)) << "op " << op;
            auto& ladder = (side == Side::Bid) ? model_bids : model_asks;
            ladder[price] += qty;
            model_orders[id] = {side, price, qty};
            live.push_back(id);
        } else if (roll < 85) {  // cancel
            std::uniform_int_distribution<std::size_t> pick(0, live.size() - 1);
            const std::size_t at = pick(rng);
            const OrderId id = live[at];
            ASSERT_TRUE(book.cancel(id)) << "op " << op;
            const auto& mo = model_orders.at(id);
            auto& ladder = (mo.side == Side::Bid) ? model_bids : model_asks;
            if ((ladder[mo.price] -= mo.qty) == 0) ladder.erase(mo.price);
            model_orders.erase(id);
            live.erase(live.begin() + static_cast<std::ptrdiff_t>(at));
        } else {  // reduce
            std::uniform_int_distribution<std::size_t> pick(0, live.size() - 1);
            const std::size_t at = pick(rng);
            const OrderId id = live[at];
            auto& mo = model_orders.at(id);
            const Qty by = std::min<Qty>(qty_dist(rng), mo.qty);
            ASSERT_TRUE(book.reduce(id, by)) << "op " << op;
            auto& ladder = (mo.side == Side::Bid) ? model_bids : model_asks;
            if ((ladder[mo.price] -= by) == 0) ladder.erase(mo.price);
            mo.qty -= by;
            if (mo.qty == 0) {
                model_orders.erase(id);
                live.erase(live.begin() + static_cast<std::ptrdiff_t>(at));
            }
        }

        // Invariants after EVERY op.
        const auto best_bid = model_bids.empty()
                                  ? std::nullopt
                                  : std::optional<Price>{model_bids.rbegin()->first};
        const auto best_ask = model_asks.empty()
                                  ? std::nullopt
                                  : std::optional<Price>{model_asks.begin()->first};
        ASSERT_EQ(book.best_bid(), best_bid) << "op " << op;
        ASSERT_EQ(book.best_ask(), best_ask) << "op " << op;
        ASSERT_EQ(book.order_count(), model_orders.size()) << "op " << op;
        ASSERT_EQ(book.depth(Side::Bid), model_bids.size()) << "op " << op;
        ASSERT_EQ(book.depth(Side::Ask), model_asks.size()) << "op " << op;
    }

    // Full ladder audit at the end.
    for (const auto& [price, qty] : model_bids) {
        EXPECT_EQ(book.qty_at(Side::Bid, price), qty) << "bid level " << price;
    }
    for (const auto& [price, qty] : model_asks) {
        EXPECT_EQ(book.qty_at(Side::Ask, price), qty) << "ask level " << price;
    }
}

}  // namespace
