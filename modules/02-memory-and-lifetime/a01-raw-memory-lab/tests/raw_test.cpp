// The spec for a01-raw-memory-lab. Part 1 (Sum/RawBuffer) tests your
// implementations; Part 2 (Bugs) tests the functions you must FIX —
// several pass in `debug` while the bug is still there. `asan` is the
// preset that tells the truth. Acceptance = green under BOTH.
#include "course/bugs.hpp"
#include "course/raw.hpp"

#include <gtest/gtest.h>

#include <cstring>

namespace {

using course::RawBuffer;
using course::sum;

// --- Part 1: sum -------------------------------------------------------------

TEST(Sum, BasicRange) {
    const int xs[] = {1, 2, 3, 4, 5};
    EXPECT_EQ(sum(xs, xs + 5), 15);
    EXPECT_EQ(sum(xs + 1, xs + 4), 9);  // interior sub-range
}

TEST(Sum, EmptyRangeIsZero) {
    const int xs[] = {42};
    EXPECT_EQ(sum(xs, xs), 0);
}

TEST(Sum, NegativeValuesAndWideResult) {
    const int xs[] = {-1'000'000'000, -1'000'000'000, -1'000'000'000};
    EXPECT_EQ(sum(xs, xs + 3), -3'000'000'000LL);  // wider than int
}

// --- Part 1: RawBuffer ---------------------------------------------------------

TEST(RawBuffer, AllocatesZeroFilled) {
    RawBuffer buf{64};
    ASSERT_NE(buf.data(), nullptr);
    EXPECT_EQ(buf.size(), 64u);
    for (std::size_t i = 0; i < buf.size(); ++i) {
        EXPECT_EQ(buf.data()[i], 0) << "byte " << i << " not zeroed";
    }
}

TEST(RawBuffer, IsWritable) {
    RawBuffer buf{8};
    std::memcpy(buf.data(), "abcdefg", 8);
    EXPECT_STREQ(buf.data(), "abcdefg");
}

TEST(RawBuffer, ResizeGrowPreservesPrefixAndZeroesTail) {
    RawBuffer buf{4};
    std::memcpy(buf.data(), "abc", 4);
    buf.resize(16);
    EXPECT_EQ(buf.size(), 16u);
    EXPECT_STREQ(buf.data(), "abc");
    for (std::size_t i = 4; i < 16; ++i) {
        EXPECT_EQ(buf.data()[i], 0) << "grown byte " << i << " not zeroed";
    }
}

TEST(RawBuffer, ResizeShrinkPreservesPrefix) {
    RawBuffer buf{8};
    std::memcpy(buf.data(), "abcdefg", 8);
    buf.resize(3);
    EXPECT_EQ(buf.size(), 3u);
    EXPECT_EQ(buf.data()[0], 'a');
    EXPECT_EQ(buf.data()[1], 'b');
    EXPECT_EQ(buf.data()[2], 'c');
    // ASan note: touching buf.data()[3] here would now be a heap overflow —
    // and if your resize() forgot to actually reallocate, ASan stays quiet
    // but you've silently kept the old size. The size() check above matters.
}

// --- Part 2: the bugs ----------------------------------------------------------

using namespace course::bugs;

TEST(Bugs, UseCountedPayloadDoesNotLeak) {
    ASSERT_EQ(Counted::alive, 0) << "another test leaked; fix that first";
    EXPECT_EQ(use_counted_payload(), 7);
    EXPECT_EQ(Counted::alive, 0) << "Counted instance leaked";
}

TEST(Bugs, SumOfSquares) {
    // Correct answers even while the bug is present — run under asan.
    EXPECT_EQ(sum_of_squares(1), 1);
    EXPECT_EQ(sum_of_squares(5), 55);
    EXPECT_EQ(sum_of_squares(10), 385);
}

TEST(Bugs, FirstChar) {
    EXPECT_EQ(first_char("hello"), 'h');
    EXPECT_EQ(first_char("x"), 'x');
}

TEST(Bugs, LengthViaBuffer) {
    EXPECT_EQ(length_via_buffer("hello"), 5u);
    EXPECT_EQ(length_via_buffer(""), 0u);
}

TEST(Bugs, RepeatIncludingZeroTimes) {
    EXPECT_EQ(repeat("ab", 3), "ababab");
    EXPECT_EQ(repeat("", 4), "");
    EXPECT_EQ(repeat("x", 0), "");  // this one aborts until you fix it
}

}  // namespace
