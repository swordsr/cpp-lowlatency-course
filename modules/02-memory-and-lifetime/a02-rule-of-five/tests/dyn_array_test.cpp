// The spec for a02-rule-of-five. Run under BOTH debug and asan — the
// interesting failures (double free, use-after-free, leak) only speak
// under asan.
#include "course/dyn_array.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <numeric>
#include <utility>

namespace {

using course::DynArray;

TEST(DynArray, DefaultIsEmpty) {
    const DynArray a;
    EXPECT_EQ(a.size(), 0u);
    EXPECT_EQ(a.data(), nullptr);
}

TEST(DynArray, SizedConstructorZeroFills) {
    const DynArray a(5);
    ASSERT_EQ(a.size(), 5u);
    for (std::size_t i = 0; i < a.size(); ++i) EXPECT_EQ(a[i], 0.0);
}

TEST(DynArray, InitializerList) {
    const DynArray a{1.5, 2.5, 3.5};
    ASSERT_EQ(a.size(), 3u);
    EXPECT_EQ(a[0], 1.5);
    EXPECT_EQ(a[2], 3.5);
}

TEST(DynArray, BracesAndParensAreDifferentConstructors) {
    // The classic C++ gotcha, pinned as spec:
    const DynArray braces{3};  // initializer_list: ONE element, value 3.0
    const DynArray parens(3);  // sized ctor: THREE zeros
    ASSERT_EQ(braces.size(), 1u);
    EXPECT_EQ(braces[0], 3.0);
    ASSERT_EQ(parens.size(), 3u);
    EXPECT_EQ(parens[0], 0.0);
}

TEST(DynArray, CopyIsDeep) {
    DynArray a{1.0, 2.0, 3.0};
    DynArray b = a;
    ASSERT_EQ(b.size(), 3u);
    EXPECT_NE(b.data(), a.data()) << "copy must have its own buffer";
    b[0] = 99.0;
    EXPECT_EQ(a[0], 1.0) << "copies must be independent";
}

TEST(DynArray, CopyAssignmentReplacesContents) {
    DynArray a{1.0, 2.0};
    const DynArray b{7.0, 8.0, 9.0};
    a = b;
    ASSERT_EQ(a.size(), 3u);
    EXPECT_EQ(a[2], 9.0);
    EXPECT_NE(a.data(), b.data());
}

TEST(DynArray, SelfAssignmentIsSafe) {
    DynArray a{4.0, 5.0};
    DynArray& alias = a;  // launder the self-assignment past compiler warnings
    a = alias;
    ASSERT_EQ(a.size(), 2u);
    EXPECT_EQ(a[0], 4.0);  // asan flags use-after-free if you freed first
    EXPECT_EQ(a[1], 5.0);
}

TEST(DynArray, MoveConstructorStealsTheBuffer) {
    DynArray a{1.0, 2.0, 3.0};
    const double* buf = a.data();
    DynArray b{std::move(a)};
    EXPECT_EQ(b.data(), buf) << "move must steal, not reallocate";
    ASSERT_EQ(b.size(), 3u);
    EXPECT_EQ(b[1], 2.0);
    EXPECT_EQ(a.size(), 0u) << "moved-from must be empty";
    EXPECT_EQ(a.data(), nullptr);
}

TEST(DynArray, MoveAssignmentFreesOldAndSteals) {
    DynArray a{1.0};
    DynArray b{7.0, 8.0};
    const double* buf = b.data();
    a = std::move(b);
    EXPECT_EQ(a.data(), buf);
    ASSERT_EQ(a.size(), 2u);
    EXPECT_EQ(a[1], 8.0);
    EXPECT_EQ(b.data(), nullptr);
    // The old 1-element buffer must have been freed — a leak here is
    // invisible to the assertions but not to your conscience; the a04
    // Tracker tests close this hole properly.
}

TEST(DynArray, SelfMoveDoesNotExplode) {
    DynArray a{1.0, 2.0};
    DynArray& alias = a;
    a = std::move(alias);
    // Required: no crash, no double free (asan checks), still destructible.
    // Contents after self-move are unspecified; size/data must be coherent.
    SUCCEED();
}

TEST(DynArray, MovedFromIsReusable) {
    DynArray a{1.0, 2.0};
    DynArray b{std::move(a)};
    a = DynArray{9.0};  // assigning into moved-from must work
    ASSERT_EQ(a.size(), 1u);
    EXPECT_EQ(a[0], 9.0);
}

TEST(DynArray, WorksWithRangeForAndAlgorithms) {
    DynArray a{3.0, 1.0, 2.0};
    std::sort(a.begin(), a.end());
    double total = 0.0;
    for (double x : a) total += x;
    EXPECT_EQ(total, 6.0);
    EXPECT_EQ(a[0], 1.0);
    EXPECT_TRUE(std::is_sorted(a.begin(), a.end()));
}

DynArray make_array() {
    DynArray local{1.0, 2.0, 3.0};
    return local;  // NRVO — no copy, no move (see THEORY §4)
}

TEST(DynArray, ReturningByValueWorks) {
    const DynArray a = make_array();
    ASSERT_EQ(a.size(), 3u);
    EXPECT_EQ(a[2], 3.0);
}

}  // namespace
