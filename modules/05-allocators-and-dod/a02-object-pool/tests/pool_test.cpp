// The spec for m05a02. Order is an instrumented stand-in for the real
// thing: the counters catch construction/destruction imbalances, the
// pointer-equality tests pin the LIFO recycling contract.
#include "course/pool.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace {

using course::ObjectPool;

struct Order {
    static inline int constructed = 0;
    static inline int destroyed = 0;
    static void reset_counts() { constructed = destroyed = 0; }

    std::int64_t price;
    std::int32_t qty;

    Order(std::int64_t p, std::int32_t q) : price(p), qty(q) { ++constructed; }
    ~Order() { ++destroyed; }
};

class PoolTest : public ::testing::Test {
protected:
    void SetUp() override { Order::reset_counts(); }
};

TEST_F(PoolTest, AcquireConstructsWithArguments) {
    ObjectPool<Order> pool{8};
    Order* o = pool.acquire(1'551'000, 100);
    ASSERT_NE(o, nullptr);
    EXPECT_EQ(o->price, 1'551'000);
    EXPECT_EQ(o->qty, 100);
    EXPECT_EQ(Order::constructed, 1);
    EXPECT_EQ(pool.in_use(), 1u);
    EXPECT_TRUE(pool.owns(o));
    pool.release(o);
}

TEST_F(PoolTest, ReleaseDestroysExactlyOnce) {
    ObjectPool<Order> pool{8};
    Order* o = pool.acquire(1, 1);
    pool.release(o);
    EXPECT_EQ(Order::destroyed, 1);
    EXPECT_EQ(pool.in_use(), 0u);
}

TEST_F(PoolTest, ExhaustionReturnsNullptr) {
    ObjectPool<Order> pool{2};
    Order* a = pool.acquire(1, 1);
    Order* b = pool.acquire(2, 2);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(pool.acquire(3, 3), nullptr) << "capacity 2 means 2";
    EXPECT_EQ(pool.in_use(), 2u);

    pool.release(a);
    Order* c = pool.acquire(4, 4);
    EXPECT_NE(c, nullptr) << "released capacity comes back";
    pool.release(b);
    pool.release(c);
}

TEST_F(PoolTest, ReuseIsLIFO) {
    ObjectPool<Order> pool{4};
    Order* a = pool.acquire(1, 1);
    Order* b = pool.acquire(2, 2);

    pool.release(a);
    pool.release(b);  // b released LAST...

    Order* c = pool.acquire(3, 3);
    EXPECT_EQ(static_cast<void*>(c), static_cast<void*>(b))
        << "...so b's slot must come back FIRST (LIFO, THEORY §3)";
    Order* d = pool.acquire(4, 4);
    EXPECT_EQ(static_cast<void*>(d), static_cast<void*>(a));

    pool.release(c);
    pool.release(d);
}

TEST_F(PoolTest, DrainAndRefillFullCapacity) {
    constexpr std::size_t kCap = 64;
    ObjectPool<Order> pool{kCap};
    std::vector<Order*> live;

    for (std::size_t round = 0; round < 3; ++round) {
        for (std::size_t i = 0; i < kCap; ++i) {
            Order* o = pool.acquire(static_cast<std::int64_t>(i), 1);
            ASSERT_NE(o, nullptr) << "round " << round << " slot " << i;
            live.push_back(o);
        }
        EXPECT_EQ(pool.acquire(0, 0), nullptr);
        EXPECT_EQ(pool.in_use(), kCap);
        for (Order* o : live) pool.release(o);
        live.clear();
        EXPECT_EQ(pool.in_use(), 0u);
    }
    EXPECT_EQ(Order::constructed, Order::destroyed);
    EXPECT_EQ(Order::constructed, 3 * static_cast<int>(kCap));
}

TEST_F(PoolTest, InterleavedChurnKeepsTheBooksBalanced) {
    ObjectPool<Order> pool{8};
    std::vector<Order*> live;

    // Deterministic churn: acquire two, release one, 100 times.
    for (int i = 0; i < 100; ++i) {
        if (Order* o = pool.acquire(i, i)) live.push_back(o);
        if (Order* o = pool.acquire(i, i)) live.push_back(o);
        if (!live.empty()) {
            pool.release(live.front());
            live.erase(live.begin());
        }
    }
    for (Order* o : live) pool.release(o);
    EXPECT_EQ(pool.in_use(), 0u);
    EXPECT_EQ(Order::constructed, Order::destroyed)
        << "every acquire must eventually meet exactly one destroy";
}

TEST_F(PoolTest, SlotsAreProperlyAligned) {
    struct alignas(32) Wide {
        double xs[4];
        Wide() {}
    };
    ObjectPool<Wide> pool{4};
    Wide* w = pool.acquire();
    ASSERT_NE(w, nullptr);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(w) % 32, 0u);
    pool.release(w);
}

TEST_F(PoolTest, OwnsRejectsForeignPointers) {
    ObjectPool<Order> pool{4};
    Order stack_order{1, 1};
    EXPECT_FALSE(pool.owns(&stack_order));
    EXPECT_FALSE(pool.owns(nullptr));
    Order* o = pool.acquire(2, 2);
    EXPECT_TRUE(pool.owns(o));
    pool.release(o);
}

}  // namespace
