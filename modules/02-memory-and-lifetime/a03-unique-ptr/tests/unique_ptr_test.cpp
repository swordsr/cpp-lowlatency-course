// The spec for a03-unique-ptr. The Tracker type counts constructions and
// destructions — any ownership imbalance (leak, double-delete, missed
// deleter call) shows up as a wrong count. asan cross-checks the rest.
#include "course/unique_ptr.hpp"

#include <gtest/gtest.h>

#include <type_traits>
#include <utility>
#include <vector>

namespace {

using course::MakeUnique;
using course::UniquePtr;

struct Tracker {
    static inline int alive = 0;
    static inline int destroyed = 0;
    static void reset_counts() { alive = 0; destroyed = 0; }

    int value = 0;
    explicit Tracker(int v = 0) : value(v) { ++alive; }
    ~Tracker() {
        --alive;
        ++destroyed;
    }
    Tracker(const Tracker&) = delete;
    Tracker& operator=(const Tracker&) = delete;
};

class UniquePtrTest : public ::testing::Test {
protected:
    void SetUp() override { Tracker::reset_counts(); }
};

// --- compile-time contract -----------------------------------------------------

static_assert(!std::is_copy_constructible_v<UniquePtr<int>>,
              "UniquePtr must not be copyable");
static_assert(!std::is_copy_assignable_v<UniquePtr<int>>,
              "UniquePtr must not be copy-assignable");
static_assert(std::is_nothrow_move_constructible_v<UniquePtr<int>>,
              "moves must be noexcept (a04 shows why it matters)");
static_assert(std::is_nothrow_move_assignable_v<UniquePtr<int>>);

TEST_F(UniquePtrTest, SizeIsOnePointer) {
    // Empty deleter must not take space (THEORY §3 / Hint 3).
    EXPECT_EQ(sizeof(UniquePtr<int>), sizeof(int*));
}

// --- basic ownership -------------------------------------------------------------

TEST_F(UniquePtrTest, DefaultIsEmpty) {
    const UniquePtr<Tracker> p;
    EXPECT_EQ(p.get(), nullptr);
    EXPECT_FALSE(p);
}

TEST_F(UniquePtrTest, ConstructorTakesOwnership) {
    Tracker* raw = new Tracker{42};
    UniquePtr<Tracker> p{raw};
    EXPECT_EQ(p.get(), raw);
    EXPECT_TRUE(p);
    EXPECT_EQ((*p).value, 42);
    EXPECT_EQ(p->value, 42);
}

TEST_F(UniquePtrTest, DestructorDestroysPointee) {
    {
        UniquePtr<Tracker> p{new Tracker{1}};
        EXPECT_EQ(Tracker::alive, 1);
    }
    EXPECT_EQ(Tracker::alive, 0) << "destructor did not destroy the pointee";
    EXPECT_EQ(Tracker::destroyed, 1);
}

TEST_F(UniquePtrTest, EmptyDestructorDoesNothing) {
    { UniquePtr<Tracker> p; }
    EXPECT_EQ(Tracker::destroyed, 0);
}

// --- moves -----------------------------------------------------------------------

TEST_F(UniquePtrTest, MoveConstructorTransfersWithoutDestroying) {
    UniquePtr<Tracker> a{new Tracker{7}};
    Tracker* raw = a.get();

    UniquePtr<Tracker> b{std::move(a)};
    EXPECT_EQ(b.get(), raw);
    EXPECT_EQ(a.get(), nullptr) << "moved-from must be empty";
    EXPECT_EQ(Tracker::alive, 1) << "moving must not destroy";
    EXPECT_EQ(b->value, 7);
}

TEST_F(UniquePtrTest, MoveAssignmentDestroysTheOldPointee) {
    UniquePtr<Tracker> a{new Tracker{1}};
    UniquePtr<Tracker> b{new Tracker{2}};
    Tracker* raw_b = b.get();

    a = std::move(b);
    EXPECT_EQ(Tracker::destroyed, 1) << "old pointee of a must be destroyed";
    EXPECT_EQ(a.get(), raw_b);
    EXPECT_EQ(a->value, 2);
    EXPECT_EQ(b.get(), nullptr);
    EXPECT_EQ(Tracker::alive, 1);
}

TEST_F(UniquePtrTest, SelfMoveIsSafe) {
    UniquePtr<Tracker> a{new Tracker{5}};
    UniquePtr<Tracker>& alias = a;
    a = std::move(alias);
    EXPECT_EQ(Tracker::alive, 1) << "self-move must not destroy the pointee";
}

// --- release / reset / swap --------------------------------------------------------

TEST_F(UniquePtrTest, ReleaseDisownsWithoutDestroying) {
    UniquePtr<Tracker> p{new Tracker{9}};
    Tracker* raw = p.release();
    EXPECT_EQ(p.get(), nullptr);
    EXPECT_EQ(Tracker::alive, 1) << "release must not destroy";
    ASSERT_NE(raw, nullptr);
    EXPECT_EQ(raw->value, 9);
    delete raw;  // we own it now
}

TEST_F(UniquePtrTest, ResetDestroysOldAndOwnsNew) {
    UniquePtr<Tracker> p{new Tracker{1}};
    p.reset(new Tracker{2});
    EXPECT_EQ(Tracker::destroyed, 1);
    EXPECT_EQ(p->value, 2);

    p.reset();
    EXPECT_EQ(Tracker::destroyed, 2);
    EXPECT_EQ(p.get(), nullptr);
    EXPECT_EQ(Tracker::alive, 0);
}

TEST_F(UniquePtrTest, SwapExchangesPointees) {
    UniquePtr<Tracker> a{new Tracker{1}};
    UniquePtr<Tracker> b{new Tracker{2}};
    a.swap(b);
    EXPECT_EQ(a->value, 2);
    EXPECT_EQ(b->value, 1);
    EXPECT_EQ(Tracker::destroyed, 0) << "swap must not destroy anything";
}

// --- custom deleters ------------------------------------------------------------------

struct CountingDeleter {
    int* count;
    void operator()(Tracker* p) const {
        ++*count;
        delete p;
    }
};

TEST_F(UniquePtrTest, CustomDeleterIsUsed) {
    int deletions = 0;
    {
        UniquePtr<Tracker, CountingDeleter> p{new Tracker{3},
                                              CountingDeleter{&deletions}};
        EXPECT_EQ(deletions, 0);
    }
    EXPECT_EQ(deletions, 1) << "custom deleter was not invoked";
    EXPECT_EQ(Tracker::alive, 0);
}

TEST_F(UniquePtrTest, StatefulDeleterGrowsTheFootprint) {
    // Opposite of SizeIsOnePointer: a deleter WITH state must be stored.
    EXPECT_EQ(sizeof(UniquePtr<Tracker, CountingDeleter>),
              sizeof(Tracker*) + sizeof(int*));
}

// --- MakeUnique -----------------------------------------------------------------------

TEST_F(UniquePtrTest, MakeUniqueForwardsArguments) {
    auto p = MakeUnique<Tracker>(31);
    ASSERT_TRUE(p);
    EXPECT_EQ(p->value, 31);
    EXPECT_EQ(Tracker::alive, 1);
}

struct TwoArg {
    int a;
    double b;
    TwoArg(int a_, double b_) : a(a_), b(b_) {}
};

TEST_F(UniquePtrTest, MakeUniqueWithMultipleArguments) {
    auto p = MakeUnique<TwoArg>(4, 2.5);
    ASSERT_TRUE(p);
    EXPECT_EQ(p->a, 4);
    EXPECT_EQ(p->b, 2.5);
}

// --- plays well with containers -------------------------------------------------------

TEST_F(UniquePtrTest, WorksInsideVector) {
    std::vector<UniquePtr<Tracker>> v;
    for (int i = 0; i < 100; ++i) v.push_back(MakeUnique<Tracker>(i));
    EXPECT_EQ(Tracker::alive, 100);
    EXPECT_EQ(v[99]->value, 99);
    v.clear();
    EXPECT_EQ(Tracker::alive, 0);
}

}  // namespace
