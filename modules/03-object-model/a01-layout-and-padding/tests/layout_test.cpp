// The spec for m03a01. Every size check here is a RUNTIME expectation on
// purpose: once your code is right, promote the ones you care about to
// static_assert in your own scratch files — that's the production idiom.
#include "course/layout.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

namespace {

using course::aligned_up;
using course::packed_struct_size;
using course::OrderCompact;
using course::OrderNaive;
using course::TaggedValue;

// Reference structs for packed_struct_size. Named, at namespace scope —
// an anonymous struct inside EXPECT_EQ trips over the macro's commas.
// The compiler lays these out; your function must agree with it.
struct CharInt {
    char a;
    int b;
};

struct IntChar {  // tail padding: the char is followed by 3 dead bytes
    int a;
    char b;
};

struct ThreeChars {  // no padding at all: alignof 1
    char a;
    char b;
    char c;
};

struct CharDoubleChar {  // padding at both ends of the double
    char a;
    double b;
    char c;
};

struct EmptyStruct {};

struct PriceTag {};  // empty: exists only to brand the type

TEST(AlignedUp, AlreadyAlignedStays) {
    EXPECT_EQ(aligned_up(0, 8), 0u);
    EXPECT_EQ(aligned_up(8, 8), 8u);
    EXPECT_EQ(aligned_up(16, 4), 16u);
}

TEST(AlignedUp, RoundsUpToNextMultiple) {
    EXPECT_EQ(aligned_up(1, 8), 8u);
    EXPECT_EQ(aligned_up(7, 8), 8u);
    EXPECT_EQ(aligned_up(9, 8), 16u);
    EXPECT_EQ(aligned_up(5, 4), 8u);
    EXPECT_EQ(aligned_up(17, 2), 18u);
}

TEST(AlignedUp, AlignmentOneIsIdentity) {
    EXPECT_EQ(aligned_up(0, 1), 0u);
    EXPECT_EQ(aligned_up(7, 1), 7u);
    EXPECT_EQ(aligned_up(23, 1), 23u);
}

// Each case checks your algorithm against what the compiler actually did.
// Note the extra parentheses: the commas in the template argument list
// would otherwise be parsed as macro-argument separators.
TEST(PackedStructSize, MatchesCompilerLayout) {
    EXPECT_EQ((packed_struct_size<char, int>()), sizeof(CharInt));
    EXPECT_EQ((packed_struct_size<char, char, char>()), sizeof(ThreeChars));
}

TEST(PackedStructSize, TailPaddingCounts) {
    // {int, char} is 5 bytes of data but sizeof is 8: the struct's size
    // must be a multiple of its alignment so arrays of it stay aligned.
    EXPECT_EQ((packed_struct_size<int, char>()), sizeof(IntChar));
}

TEST(PackedStructSize, PaddingBeforeAndAfter) {
    // {char, double, char}: 7 bytes before the double, 7 after the last
    // char — 10 bytes of data, 24 bytes of struct.
    EXPECT_EQ((packed_struct_size<char, double, char>()),
              sizeof(CharDoubleChar));
}

TEST(PackedStructSize, SingleMemberIsJustThatMember) {
    EXPECT_EQ(packed_struct_size<double>(), sizeof(double));
    EXPECT_EQ(packed_struct_size<char>(), sizeof(char));
}

TEST(OrderLayout, NaiveOrderWastes16Bytes) {
    // Not a bug to fix — a fact to internalize. Six members totalling
    // 24 bytes of data cost 40 bytes in declaration order. This test
    // passes from day one; it is here as documentation.
    EXPECT_EQ(sizeof(OrderNaive), 40u);
}

TEST(OrderLayout, CompactOrderReaches24) {
    // Same members, reordered by you. 24 = the data itself, zero padding.
    EXPECT_EQ(sizeof(OrderCompact), 24u);
}

TEST(EmptyClasses, EmptyStructHasSizeOne) {
    // Spec, not accident: every object needs a distinct address, so even
    // a memberless struct occupies one byte (THEORY §4). Arrays and
    // pointer arithmetic would break otherwise.
    EXPECT_EQ(sizeof(EmptyStruct), 1u);
}

TEST(TaggedValue, EmptyTagCostsNothing) {
    // The whole point of a tag type is that it's free. Ships failing at
    // 16; your job is to make the empty member occupy zero bytes.
    EXPECT_EQ(sizeof(TaggedValue<double, PriceTag>), sizeof(double));
}

TEST(TaggedValue, AccessorsStillWork) {
    // Whatever you do to the layout must not break the value semantics.
    TaggedValue<double, PriceTag> price{101.25};
    EXPECT_EQ(price.value(), 101.25);
    price.set_value(99.5);
    EXPECT_EQ(price.value(), 99.5);
}

}  // namespace
