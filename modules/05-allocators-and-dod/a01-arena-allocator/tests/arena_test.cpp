// The spec for m05a01.
#include "course/arena.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace {

using course::Arena;
using course::ArenaAllocator;

bool aligned_to(const void* p, std::size_t align) {
    return reinterpret_cast<std::uintptr_t>(p) % align == 0;
}

TEST(Arena, AllocationsAreAlignedAndDisjoint) {
    Arena arena{1024};

    void* a = arena.allocate(1, 1);
    void* b = arena.allocate(8, 8);
    void* c = arena.allocate(16, 16);

    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(aligned_to(b, 8));
    EXPECT_TRUE(aligned_to(c, 16));
    EXPECT_NE(a, b);
    EXPECT_NE(b, c);
    EXPECT_TRUE(arena.owns(a));
    EXPECT_TRUE(arena.owns(b));
    EXPECT_TRUE(arena.owns(c));
}

TEST(Arena, BumpsMonotonically) {
    Arena arena{1024};
    auto* a = static_cast<unsigned char*>(arena.allocate(16, 8));
    auto* b = static_cast<unsigned char*>(arena.allocate(16, 8));
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b, a + 16) << "aligned same-size allocations should be "
                            "exactly adjacent — this is a bump, not a heap";
}

TEST(Arena, UsedTracksConsumption) {
    Arena arena{1024};
    EXPECT_EQ(arena.used(), 0u);
    arena.allocate(10, 1);
    EXPECT_EQ(arena.used(), 10u);
    arena.allocate(6, 8);  // 6 padding bytes to reach alignment 8, then 6
    EXPECT_EQ(arena.used(), 22u) << "alignment gaps count as used";
    EXPECT_EQ(arena.capacity(), 1024u);
}

TEST(Arena, FillsToExactCapacity) {
    Arena arena{64};
    EXPECT_NE(arena.allocate(64, 1), nullptr) << "exact fit must succeed";
    EXPECT_EQ(arena.allocate(1, 1), nullptr) << "and now it's full";
}

TEST(Arena, ExhaustionReturnsNullptrNotUB) {
    Arena arena{64};
    EXPECT_EQ(arena.allocate(65, 1), nullptr);
    // Overflow probe: aligned + size must not wrap (Hint 2).
    EXPECT_EQ(arena.allocate(SIZE_MAX - 8, 8), nullptr);
    // A failed allocation must not consume space.
    EXPECT_EQ(arena.used(), 0u);
    EXPECT_NE(arena.allocate(64, 1), nullptr);
}

TEST(Arena, ResetRewindsAndMemoryIsReused) {
    Arena arena{128};
    void* first = arena.allocate(100, 8);
    ASSERT_NE(first, nullptr);
    arena.reset();
    EXPECT_EQ(arena.used(), 0u);
    void* again = arena.allocate(100, 8);
    EXPECT_EQ(again, first) << "after reset the same bytes come back";
}

struct Widget {
    static inline int constructed = 0;
    static inline int destroyed = 0;
    int a;
    int b;
    Widget(int a_, int b_) : a(a_), b(b_) { ++constructed; }
    ~Widget() { ++destroyed; }
};

TEST(Arena, CreateConstructsInPlace) {
    Widget::constructed = Widget::destroyed = 0;
    Arena arena{1024};

    Widget* w = arena.create<Widget>(4, 2);
    ASSERT_NE(w, nullptr);
    EXPECT_TRUE(arena.owns(w));
    EXPECT_TRUE(aligned_to(w, alignof(Widget)));
    EXPECT_EQ(w->a, 4);
    EXPECT_EQ(w->b, 2);
    EXPECT_EQ(Widget::constructed, 1);
}

TEST(Arena, ResetRunsNoDestructors) {
    // The documented contract (THEORY §2), pinned as spec: reset() must
    // NOT know about or destroy the objects created in the arena.
    Widget::constructed = Widget::destroyed = 0;
    Arena arena{1024};
    arena.create<Widget>(1, 2);
    arena.reset();
    EXPECT_EQ(Widget::destroyed, 0);
}

TEST(Arena, CreateReturnsNullWhenFull) {
    Arena arena{sizeof(Widget)};
    EXPECT_NE(arena.create<Widget>(1, 2), nullptr);
    EXPECT_EQ(arena.create<Widget>(3, 4), nullptr);
}

TEST(Arena, OwnsRejectsForeignPointers) {
    Arena arena{64};
    int on_the_stack = 0;
    EXPECT_FALSE(arena.owns(&on_the_stack));
    EXPECT_FALSE(arena.owns(nullptr));
}

// --- the std::vector adapter -----------------------------------------------------

TEST(ArenaAllocator, VectorDrawsFromTheArena) {
    Arena arena{1 << 16};
    std::vector<std::int64_t, ArenaAllocator<std::int64_t>> v{
        ArenaAllocator<std::int64_t>{arena}};

    for (int i = 0; i < 100; ++i) v.push_back(i);

    EXPECT_EQ(v.size(), 100u);
    EXPECT_EQ(v[99], 99);
    EXPECT_TRUE(arena.owns(v.data()))
        << "the vector's buffer must live in the arena";
    EXPECT_GT(arena.used(), 100 * sizeof(std::int64_t) - 1)
        << "growth consumed arena space (old buffers aren't reclaimed — "
           "that's the arena deal)";
}

TEST(ArenaAllocator, RebindingCompilesAndAllocates) {
    // Node-based containers rebind ArenaAllocator<T> to their node type;
    // the converting constructor makes this line compile at all.
    Arena arena{1 << 16};
    ArenaAllocator<int> as_int{arena};
    ArenaAllocator<double> as_double{as_int};
    double* d = as_double.allocate(4);
    ASSERT_NE(d, nullptr);
    EXPECT_TRUE(arena.owns(d));
    EXPECT_TRUE(aligned_to(d, alignof(double)));
}

TEST(ArenaAllocator, EqualityMeansSameArena) {
    Arena a{64};
    Arena b{64};
    ArenaAllocator<int> a1{a};
    ArenaAllocator<int> a2{a};
    ArenaAllocator<int> b1{b};
    EXPECT_TRUE(a1 == a2);
    EXPECT_FALSE(a1 == b1);
}

TEST(ArenaAllocator, ExhaustionThrowsBadAlloc) {
    Arena arena{16};
    ArenaAllocator<std::int64_t> alloc{arena};
    EXPECT_THROW(alloc.allocate(1000), std::bad_alloc);
}

}  // namespace
