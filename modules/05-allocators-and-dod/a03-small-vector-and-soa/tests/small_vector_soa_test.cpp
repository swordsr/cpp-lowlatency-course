// The spec for m05a03: SmallVector storage duality + the AoS/SoA pair.
#include "course/small_vector.hpp"
#include "course/soa.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <utility>
#include <vector>

namespace {

using course::from_aos;
using course::OrderAoS;
using course::OrderColumns;
using course::SmallVector;
using course::sum_notional_aos;

// --- SmallVector: inline regime ---------------------------------------------------

TEST(SmallVectorInline, StaysInlineUpToN) {
    SmallVector<std::int64_t, 4> v;
    for (std::int64_t i = 0; i < 4; ++i) v.push_back(i);
    EXPECT_EQ(v.size(), 4u);
    EXPECT_TRUE(v.is_inline()) << "N elements must not touch the heap";
    EXPECT_EQ(v.capacity(), 4u);
    EXPECT_EQ(v[0], 0);
    EXPECT_EQ(v[3], 3);
}

TEST(SmallVectorInline, DataPointsInsideTheObjectWhileInline) {
    SmallVector<std::int64_t, 4> v;
    v.push_back(42);
    const auto* obj_begin = reinterpret_cast<const unsigned char*>(&v);
    const auto* obj_end = obj_begin + sizeof(v);
    const auto* p = reinterpret_cast<const unsigned char*>(v.data());
    EXPECT_TRUE(p >= obj_begin && p < obj_end)
        << "inline storage means INSIDE the object — that's the locality win";
}

// --- SmallVector: the spill --------------------------------------------------------

TEST(SmallVectorSpill, ElementNPlusOneMovesToTheHeap) {
    SmallVector<std::int64_t, 4> v;
    for (std::int64_t i = 0; i < 5; ++i) v.push_back(i);
    EXPECT_EQ(v.size(), 5u);
    EXPECT_FALSE(v.is_inline());
    EXPECT_GE(v.capacity(), 5u);
    for (std::int64_t i = 0; i < 5; ++i) EXPECT_EQ(v[i], i);

    const auto* obj_begin = reinterpret_cast<const unsigned char*>(&v);
    const auto* obj_end = obj_begin + sizeof(v);
    const auto* p = reinterpret_cast<const unsigned char*>(v.data());
    EXPECT_FALSE(p >= obj_begin && p < obj_end) << "spilled data is on the heap";
}

struct Item {
    static inline int constructed = 0;
    static inline int copied = 0;
    static inline int moved = 0;
    static inline int destroyed = 0;
    static inline int alive = 0;
    static void reset_counts() {
        constructed = copied = moved = destroyed = alive = 0;
    }
    int value = 0;
    explicit Item(int v = 0) : value(v) { ++constructed, ++alive; }
    Item(const Item& o) : value(o.value) { ++copied, ++alive; }
    Item(Item&& o) noexcept : value(o.value) { ++moved, ++alive; }
    ~Item() { ++destroyed, --alive; }
};

TEST(SmallVectorSpill, SpillMovesInlineElementsAndDestroysOriginals) {
    Item::reset_counts();
    {
        SmallVector<Item, 2> v;
        v.emplace_back(1);
        v.emplace_back(2);
        Item::reset_counts();

        v.emplace_back(3);  // the spill

        EXPECT_EQ(Item::moved, 2) << "both inline elements move to the heap";
        EXPECT_EQ(Item::copied, 0);
        EXPECT_EQ(Item::destroyed, 2) << "the inline originals must die";
        EXPECT_EQ(v[0].value, 1);
        EXPECT_EQ(v[2].value, 3);
    }
    EXPECT_EQ(Item::alive, 0) << "destructor leaked after a spill";
}

TEST(SmallVectorSpill, GrowsBeyondTheFirstSpill) {
    SmallVector<std::int64_t, 2> v;
    for (std::int64_t i = 0; i < 100; ++i) v.push_back(i);
    EXPECT_EQ(v.size(), 100u);
    for (std::int64_t i = 0; i < 100; ++i) ASSERT_EQ(v[i], i);
}

TEST(SmallVectorLifetime, EveryPathBalancesConstructions) {
    Item::reset_counts();
    {
        SmallVector<Item, 4> v;
        v.emplace_back(1);
        v.emplace_back(2);
        v.pop_back();
        v.emplace_back(3);
        v.clear();
        v.emplace_back(4);  // reusable after clear
        EXPECT_EQ(v.size(), 1u);
        EXPECT_EQ(v[0].value, 4);
    }
    EXPECT_EQ(Item::alive, 0);
    EXPECT_EQ(Item::constructed + Item::copied + Item::moved, Item::destroyed);
}

TEST(SmallVectorLifetime, WorksWithRangeFor) {
    SmallVector<int, 8> v;
    for (int x : {3, 1, 2}) v.push_back(x);
    int total = 0;
    for (int x : v) total += x;
    EXPECT_EQ(total, 6);
}

// --- AoS vs SoA ---------------------------------------------------------------------

std::vector<OrderAoS> fixture_orders() {
    std::vector<OrderAoS> orders;
    for (int i = 1; i <= 100; ++i) {
        orders.push_back(OrderAoS{i * 1'000, i, 0, {}});
    }
    return orders;
}

TEST(Soa, AosSumIsExact) {
    // Σ (i*1000 * i) for i in 1..100 = 1000 * Σ i² = 1000 * 338'350.
    EXPECT_EQ(sum_notional_aos(fixture_orders()), 338'350'000);
    EXPECT_EQ(sum_notional_aos({}), 0);
}

TEST(Soa, ColumnsPushBackMaintainsTheInvariant) {
    OrderColumns cols;
    cols.push_back(1'000, 5);
    cols.push_back(2'000, 7);
    EXPECT_EQ(cols.size(), 2u);
    EXPECT_EQ(cols.price_at(0), 1'000);
    EXPECT_EQ(cols.qty_at(1), 7);
}

TEST(Soa, ColumnsSumMatchesAos) {
    const auto orders = fixture_orders();
    const OrderColumns cols = from_aos(orders);
    EXPECT_EQ(cols.size(), orders.size());
    EXPECT_EQ(cols.sum_notional(), sum_notional_aos(orders))
        << "same orders, same notional — layout must not change the answer";
}

TEST(Soa, TransformPreservesOrderAndValues) {
    const auto cols = from_aos(fixture_orders());
    EXPECT_EQ(cols.price_at(0), 1'000);
    EXPECT_EQ(cols.qty_at(0), 1);
    EXPECT_EQ(cols.price_at(99), 100'000);
    EXPECT_EQ(cols.qty_at(99), 100);
}

}  // namespace
