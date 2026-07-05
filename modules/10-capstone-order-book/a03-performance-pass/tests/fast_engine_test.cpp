// The spec for m10a03: behavioral equivalence with YOUR a02, proven by
// replaying identical op streams into both and comparing after every
// op. If a01/a02 aren't green, these results mean nothing — the oracle
// must be trusted first (README THEORY §3).
#include "course/fast_engine.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <random>
#include <vector>

namespace {

using namespace course;

constexpr Price kBandMin = 90;
constexpr Price kBandMax = 110;

TEST(FastEngine, BasicSubmitAndQueries) {
    FastEngine eng{kBandMin, kBandMax};
    EXPECT_TRUE(eng.submit(1, Side::Bid, 100, 10).empty());
    EXPECT_EQ(eng.best_bid(), 100);
    EXPECT_EQ(eng.qty_at(Side::Bid, 100), 10u);
    EXPECT_EQ(eng.order_count(), 1u);
    EXPECT_EQ(eng.quantity_of(1), 10u);
}

TEST(FastEngine, MatchesLikeA02) {
    FastEngine eng{kBandMin, kBandMax};
    eng.submit(1, Side::Ask, 100, 5);
    eng.submit(2, Side::Ask, 100, 5);
    const auto trades = eng.submit(3, Side::Bid, 101, 7);
    ASSERT_EQ(trades.size(), 2u);
    EXPECT_EQ(trades[0].maker_id, 1u) << "FIFO survives the rebuild";
    EXPECT_EQ(trades[0].price, 100) << "maker price survives the rebuild";
    EXPECT_EQ(trades[1].qty, 2u);
    EXPECT_EQ(eng.quantity_of(2), 3u);
}

TEST(FastEngine, OutOfBandPricesAreRejected) {
    FastEngine eng{kBandMin, kBandMax};
    EXPECT_TRUE(eng.submit(1, Side::Bid, kBandMax + 1, 5).empty());
    EXPECT_TRUE(eng.submit(2, Side::Ask, kBandMin - 1, 5).empty());
    EXPECT_EQ(eng.order_count(), 0u);
    EXPECT_TRUE(eng.submit(3, Side::Bid, kBandMin, 5).empty()) ;
    EXPECT_EQ(eng.order_count(), 1u) << "band edges are IN band";
}

TEST(FastEngine, CancelAndReduceMirrorA01Semantics) {
    FastEngine eng{kBandMin, kBandMax};
    eng.submit(1, Side::Ask, 105, 10);
    ASSERT_TRUE(eng.reduce(1, 4));
    EXPECT_EQ(eng.quantity_of(1), 6u);
    EXPECT_FALSE(eng.reduce(1, 0));
    ASSERT_TRUE(eng.reduce(1, 99)) << "over-reduce removes";
    EXPECT_FALSE(eng.cancel(1)) << "already gone";
    EXPECT_EQ(eng.depth(Side::Ask), 0u);
}

// --- the differential suite -------------------------------------------------------

struct Op {
    enum class Kind { Submit, Cancel, Reduce } kind;
    OrderId id;
    Side side;
    Price price;
    Qty qty;
};

std::vector<Op> make_stream(std::uint32_t seed, int n) {
    std::mt19937 rng{seed};
    std::uniform_int_distribution<int> roll(0, 99);
    std::uniform_int_distribution<Price> price(kBandMin + 2, kBandMax - 2);
    std::uniform_int_distribution<Qty> qty(1, 25);
    std::vector<Op> ops;
    OrderId next_id = 1;
    std::vector<OrderId> maybe_live;
    for (int i = 0; i < n; ++i) {
        const int r = roll(rng);
        if (r < 60 || maybe_live.empty()) {
            const OrderId id = next_id++;
            ops.push_back({Op::Kind::Submit, id,
                           (r % 2 == 0) ? Side::Bid : Side::Ask, price(rng),
                           qty(rng)});
            maybe_live.push_back(id);
        } else if (r < 85) {
            std::uniform_int_distribution<std::size_t> pick(
                0, maybe_live.size() - 1);
            ops.push_back({Op::Kind::Cancel, maybe_live[pick(rng)], Side::Bid,
                           0, 0});
        } else {
            std::uniform_int_distribution<std::size_t> pick(
                0, maybe_live.size() - 1);
            ops.push_back({Op::Kind::Reduce, maybe_live[pick(rng)], Side::Bid,
                           0, qty(rng)});
        }
    }
    return ops;
}

void run_differential(std::uint32_t seed) {
    SCOPED_TRACE(::testing::Message() << "seed " << seed);
    MatchingEngine oracle;  // YOUR a02
    FastEngine fast{kBandMin, kBandMax};

    const auto ops = make_stream(seed, 4'000);
    for (std::size_t i = 0; i < ops.size(); ++i) {
        const Op& op = ops[i];
        switch (op.kind) {
            case Op::Kind::Submit: {
                const auto expected =
                    oracle.submit(op.id, op.side, op.price, op.qty);
                const auto actual =
                    fast.submit(op.id, op.side, op.price, op.qty);
                ASSERT_EQ(actual.size(), expected.size()) << "op " << i;
                for (std::size_t t = 0; t < expected.size(); ++t) {
                    ASSERT_EQ(actual[t].maker_id, expected[t].maker_id)
                        << "op " << i << " trade " << t;
                    ASSERT_EQ(actual[t].taker_id, expected[t].taker_id);
                    ASSERT_EQ(actual[t].price, expected[t].price);
                    ASSERT_EQ(actual[t].qty, expected[t].qty);
                }
                break;
            }
            case Op::Kind::Cancel:
                ASSERT_EQ(fast.cancel(op.id), oracle.cancel(op.id))
                    << "op " << i;
                break;
            case Op::Kind::Reduce:
                ASSERT_EQ(fast.reduce(op.id, op.qty),
                          oracle.reduce(op.id, op.qty))
                    << "op " << i;
                break;
        }
        ASSERT_EQ(fast.best_bid(), oracle.book().best_bid()) << "op " << i;
        ASSERT_EQ(fast.best_ask(), oracle.book().best_ask()) << "op " << i;
        ASSERT_EQ(fast.order_count(), oracle.book().order_count())
            << "op " << i;
    }
}

TEST(FastEngineDifferential, Seed1) { run_differential(101); }
TEST(FastEngineDifferential, Seed2) { run_differential(202); }
TEST(FastEngineDifferential, Seed3) { run_differential(303); }

}  // namespace
