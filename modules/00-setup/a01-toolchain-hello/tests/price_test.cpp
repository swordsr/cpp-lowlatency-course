// The spec for a01-toolchain-hello. Read these before implementing;
// each test documents one required behavior of parse_price/format_price.
#include "course/price.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string>

namespace {

using course::format_price;
using course::kPriceScale;
using course::parse_price;

constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();

// --- parse_price: valid inputs ---------------------------------------------

TEST(ParsePrice, ZeroVariants) {
    EXPECT_EQ(parse_price("0"), 0);
    EXPECT_EQ(parse_price("0.0"), 0);
    EXPECT_EQ(parse_price("-0"), 0);
    EXPECT_EQ(parse_price("000"), 0);
}

TEST(ParsePrice, WholeNumbers) {
    EXPECT_EQ(parse_price("1"), 10'000);
    EXPECT_EQ(parse_price("123"), 1'230'000);
    EXPECT_EQ(parse_price("007"), 70'000);  // leading zeros are fine
}

TEST(ParsePrice, FractionDigitsScaleCorrectly) {
    // 1 to 4 fraction digits; missing trailing digits are implicit zeros.
    EXPECT_EQ(parse_price("123.4"), 1'234'000);
    EXPECT_EQ(parse_price("123.45"), 1'234'500);
    EXPECT_EQ(parse_price("123.456"), 1'234'560);
    EXPECT_EQ(parse_price("123.4567"), 1'234'567);
    EXPECT_EQ(parse_price("0.0001"), 1);  // one tick
}

TEST(ParsePrice, NegativeValues) {
    EXPECT_EQ(parse_price("-155.1"), -1'551'000);
    EXPECT_EQ(parse_price("-0.0001"), -1);
    EXPECT_EQ(parse_price("-42"), -420'000);
}

TEST(ParsePrice, LargestRepresentablePrice) {
    // INT64_MAX ticks == 922'337'203'685'477.5807. Exactly representable.
    EXPECT_EQ(parse_price("922337203685477.5807"), kMax);
    EXPECT_EQ(parse_price("-922337203685477.5807"), -kMax);
}

// --- parse_price: rejected inputs -------------------------------------------

TEST(ParsePrice, RejectsEmptyAndBareSigns) {
    EXPECT_EQ(parse_price(""), std::nullopt);
    EXPECT_EQ(parse_price("-"), std::nullopt);
    EXPECT_EQ(parse_price("+1"), std::nullopt);  // '+' is not in the grammar
}

TEST(ParsePrice, RejectsMalformedDots) {
    EXPECT_EQ(parse_price("."), std::nullopt);
    EXPECT_EQ(parse_price("1."), std::nullopt);    // digits required after '.'
    EXPECT_EQ(parse_price(".5"), std::nullopt);    // digits required before '.'
    EXPECT_EQ(parse_price("-.5"), std::nullopt);
    EXPECT_EQ(parse_price("1..2"), std::nullopt);
    EXPECT_EQ(parse_price("1.2.3"), std::nullopt);
}

TEST(ParsePrice, RejectsStrayCharacters) {
    EXPECT_EQ(parse_price("abc"), std::nullopt);
    EXPECT_EQ(parse_price("12a"), std::nullopt);
    EXPECT_EQ(parse_price("a12"), std::nullopt);
    EXPECT_EQ(parse_price(" 1"), std::nullopt);   // no whitespace trimming
    EXPECT_EQ(parse_price("1 "), std::nullopt);
    EXPECT_EQ(parse_price("12,5"), std::nullopt);
    EXPECT_EQ(parse_price("1e3"), std::nullopt);
}

TEST(ParsePrice, RejectsTooManyFractionDigits) {
    EXPECT_EQ(parse_price("1.00001"), std::nullopt);
    EXPECT_EQ(parse_price("0.12345"), std::nullopt);
}

TEST(ParsePrice, RejectsOverflow) {
    // One tick past INT64_MAX...
    EXPECT_EQ(parse_price("922337203685477.5808"), std::nullopt);
    EXPECT_EQ(parse_price("-922337203685477.5808"), std::nullopt);
    // ...one whole unit past...
    EXPECT_EQ(parse_price("922337203685478"), std::nullopt);
    // ...and far past: this must not invoke signed-integer overflow (UB)
    // while accumulating. Run the asan preset — UBSan will flag a naive
    // multiply-then-check implementation here.
    EXPECT_EQ(parse_price("99999999999999999999"), std::nullopt);
}

// --- format_price ------------------------------------------------------------

TEST(FormatPrice, WholeValuesHaveNoDot) {
    EXPECT_EQ(format_price(0), "0");
    EXPECT_EQ(format_price(10'000), "1");
    EXPECT_EQ(format_price(1'230'000), "123");
    EXPECT_EQ(format_price(-420'000), "-42");
}

TEST(FormatPrice, TrailingFractionZerosAreTrimmed) {
    EXPECT_EQ(format_price(1'234'000), "123.4");
    EXPECT_EQ(format_price(1'234'500), "123.45");
    EXPECT_EQ(format_price(1'234'560), "123.456");
    EXPECT_EQ(format_price(1'234'567), "123.4567");
    EXPECT_EQ(format_price(1'000), "0.1");
}

TEST(FormatPrice, SubUnitNegativesKeepTheirSign) {
    // The classic bug: -1 ticks is "-0.0001", but whole part -0 == 0 loses
    // the sign if you format parts naively.
    EXPECT_EQ(format_price(-1), "-0.0001");
    EXPECT_EQ(format_price(-1'000), "-0.1");
    EXPECT_EQ(format_price(-1'551'000), "-155.1");
}

TEST(FormatPrice, RoundTripsCanonicalStrings) {
    for (const std::string s : {"0", "1", "123", "-42", "155.1", "0.0001",
                                "-0.0001", "123.4567", "922337203685477.5807",
                                "-922337203685477.5807"}) {
        const auto ticks = parse_price(s);
        ASSERT_TRUE(ticks.has_value()) << "failed to parse: " << s;
        EXPECT_EQ(format_price(*ticks), s);
    }
}

}  // namespace
