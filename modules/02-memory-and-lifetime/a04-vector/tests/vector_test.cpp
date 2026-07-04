// The spec for a04-vector. Item counts every construction, copy, move,
// and destruction — the tests assert exact balances, so leaks, double
// destroys, and copy-where-move-required all fail loudly.
#include "course/vector.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <type_traits>
#include <utility>

namespace {

using course::Vector;

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

    explicit Item(int v = 0) : value(v) {
        ++constructed;
        ++alive;
    }
    Item(const Item& o) : value(o.value) {
        ++copied;
        ++alive;
    }
    Item(Item&& o) noexcept : value(o.value) {
        ++moved;
        ++alive;
        o.value = -1;
    }
    Item& operator=(const Item& o) {
        value = o.value;
        ++copied;
        return *this;
    }
    Item& operator=(Item&& o) noexcept {
        value = o.value;
        o.value = -1;
        ++moved;
        return *this;
    }
    ~Item() {
        ++destroyed;
        --alive;
    }
};

class VectorTest : public ::testing::Test {
protected:
    void SetUp() override { Item::reset_counts(); }
};

// --- basics ------------------------------------------------------------------

TEST_F(VectorTest, StartsEmpty) {
    const Vector<int> v;
    EXPECT_EQ(v.size(), 0u);
    EXPECT_EQ(v.capacity(), 0u);
    EXPECT_TRUE(v.empty());
}

TEST_F(VectorTest, PushBackStoresValues) {
    Vector<int> v;
    v.push_back(10);
    v.push_back(20);
    v.push_back(30);
    ASSERT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0], 10);
    EXPECT_EQ(v[1], 20);
    EXPECT_EQ(v[2], 30);
    EXPECT_EQ(v.front(), 10);
    EXPECT_EQ(v.back(), 30);
}

TEST_F(VectorTest, GrowthPolicyIsExactlyDoubling) {
    Vector<int> v;
    std::size_t expected_caps[] = {1, 2, 4, 4, 8, 8, 8, 8, 16};
    for (int i = 0; i < 9; ++i) {
        v.push_back(i);
        EXPECT_EQ(v.capacity(), expected_caps[i]) << "after push #" << i + 1;
    }
}

TEST_F(VectorTest, ReserveIsExactAndNeverShrinks) {
    Vector<int> v;
    v.reserve(100);
    EXPECT_EQ(v.capacity(), 100u);
    EXPECT_EQ(v.size(), 0u);
    v.reserve(10);  // never shrinks
    EXPECT_EQ(v.capacity(), 100u);
}

TEST_F(VectorTest, ReserveMakesDataStable) {
    Vector<int> v;
    v.reserve(10);
    const int* p = v.data();
    for (int i = 0; i < 10; ++i) v.push_back(i);
    EXPECT_EQ(v.data(), p) << "no reallocation may happen within capacity";
}

// --- object lifetime ----------------------------------------------------------

TEST_F(VectorTest, DestructorDestroysExactlyTheElements) {
    {
        Vector<Item> v;
        for (int i = 0; i < 10; ++i) v.push_back(Item{i});
        EXPECT_EQ(Item::alive, 10);
    }
    EXPECT_EQ(Item::alive, 0) << "leak or double-destroy";
}

TEST_F(VectorTest, ClearDestroysButKeepsCapacity) {
    Vector<Item> v;
    for (int i = 0; i < 5; ++i) v.push_back(Item{i});
    const std::size_t cap = v.capacity();
    v.clear();
    EXPECT_EQ(v.size(), 0u);
    EXPECT_EQ(v.capacity(), cap);
    EXPECT_EQ(Item::alive, 0);
    v.push_back(Item{42});  // must be reusable after clear
    EXPECT_EQ(v[0].value, 42);
}

TEST_F(VectorTest, PopBackDestroysTheLastElement) {
    Vector<Item> v;
    v.push_back(Item{1});
    v.push_back(Item{2});
    const int destroyed_before = Item::destroyed;
    v.pop_back();
    EXPECT_EQ(v.size(), 1u);
    EXPECT_EQ(Item::destroyed, destroyed_before + 1);
    EXPECT_EQ(v[0].value, 1);
}

// --- move discipline ------------------------------------------------------------

TEST_F(VectorTest, GrowthMovesElementsBecauseMoveIsNoexcept) {
    static_assert(std::is_nothrow_move_constructible_v<Item>);
    Vector<Item> v;
    v.reserve(4);
    for (int i = 0; i < 4; ++i) v.push_back(Item{i});
    Item::reset_counts();

    v.push_back(Item{4});  // forces reallocation of the 4 existing elements

    EXPECT_EQ(Item::copied, 0)
        << "relocation copied noexcept-movable elements (move_if_noexcept?)";
    EXPECT_GE(Item::moved, 4) << "the 4 old elements must be moved over";
}

TEST_F(VectorTest, EmplaceBackConstructsInPlace) {
    Vector<Item> v;
    v.reserve(2);
    Item::reset_counts();
    Item& ref = v.emplace_back(7);
    EXPECT_EQ(Item::constructed, 1);
    EXPECT_EQ(Item::copied, 0) << "emplace_back must not copy";
    EXPECT_EQ(Item::moved, 0) << "emplace_back must not move";
    EXPECT_EQ(ref.value, 7);
    EXPECT_EQ(&ref, &v[0]);
}

TEST_F(VectorTest, PushBackOwnElementWhileFullIsSafe) {
    // THE trap (THEORY §4): v[0] points into the buffer being replaced.
    Vector<Item> v;
    v.push_back(Item{11});
    v.push_back(Item{22});  // size == capacity == 2 now
    ASSERT_EQ(v.size(), v.capacity()) << "test premise: vector is full";
    v.push_back(v[0]);
    ASSERT_EQ(v.size(), 3u);
    EXPECT_EQ(v[2].value, 11) << "aliased push_back read freed memory";
    EXPECT_EQ(v[0].value, 11);
}

// --- rule of five ------------------------------------------------------------------

TEST_F(VectorTest, CopyIsDeep) {
    Vector<Item> a;
    a.push_back(Item{1});
    a.push_back(Item{2});
    Vector<Item> b = a;
    ASSERT_EQ(b.size(), 2u);
    EXPECT_NE(b.data(), a.data());
    b[0].value = 99;
    EXPECT_EQ(a[0].value, 1);
}

TEST_F(VectorTest, CopyAssignmentIsSelfSafe) {
    Vector<Item> a;
    a.push_back(Item{5});
    Vector<Item>& alias = a;
    a = alias;
    ASSERT_EQ(a.size(), 1u);
    EXPECT_EQ(a[0].value, 5);
}

TEST_F(VectorTest, MoveConstructorSteals) {
    Vector<Item> a;
    a.push_back(Item{1});
    const Item* buf = a.data();
    Item::reset_counts();
    Vector<Item> b{std::move(a)};
    EXPECT_EQ(b.data(), buf) << "move ctor must steal the buffer";
    EXPECT_EQ(Item::moved, 0) << "moving the VECTOR must not move ELEMENTS";
    EXPECT_EQ(a.size(), 0u);
    EXPECT_EQ(b[0].value, 1);
}

TEST_F(VectorTest, MoveAssignmentDestroysOwnContents) {
    Vector<Item> a;
    a.push_back(Item{1});
    Vector<Item> b;
    b.push_back(Item{2});
    b.push_back(Item{3});
    a = std::move(b);
    ASSERT_EQ(a.size(), 2u);
    EXPECT_EQ(a[1].value, 3);
    EXPECT_EQ(Item::alive, 2) << "a's old element must be destroyed";
}

// --- element-type requirements ---------------------------------------------------

TEST_F(VectorTest, SupportsMoveOnlyElementTypes) {
    // Instantiating this at all proves push_back(T&&)/emplace_back don't
    // secretly require copyability. (Why does the copy ctor of Vector not
    // break this? Lazy template instantiation — question-bank material.)
    Vector<std::unique_ptr<int>> v;
    v.push_back(std::make_unique<int>(1));
    v.emplace_back(std::make_unique<int>(2));
    for (int i = 0; i < 20; ++i) v.push_back(std::make_unique<int>(i));  // growth
    ASSERT_EQ(v.size(), 22u);
    EXPECT_EQ(*v[1], 2);
}

TEST_F(VectorTest, WorksWithAlgorithms) {
    Vector<int> v;
    for (int x : {3, 1, 2}) v.push_back(x);
    std::sort(v.begin(), v.end());
    EXPECT_TRUE(std::is_sorted(v.begin(), v.end()));
    int total = 0;
    for (int x : v) total += x;
    EXPECT_EQ(total, 6);
}

}  // namespace
