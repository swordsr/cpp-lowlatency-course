// The spec for m04a03. Several tests evaluate your functions in constexpr
// variables — that's the point: they must WORK at compile time, and the
// UB-strictness of compile-time evaluation is part of the spec.
#include "course/ctdispatch.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>

namespace {

using course::Dispatcher;
using course::kDigitTable;
using course::kPow10;
using course::parse_uint;
using course::sum_all;

// --- tables -------------------------------------------------------------------

TEST(DigitTable, DigitsMapToTheirValues) {
    EXPECT_EQ(kDigitTable['0'], 0);
    EXPECT_EQ(kDigitTable['5'], 5);
    EXPECT_EQ(kDigitTable['9'], 9);
}

TEST(DigitTable, EverythingElseMapsTo255) {
    EXPECT_EQ(kDigitTable['a'], 255);
    EXPECT_EQ(kDigitTable[' '], 255);
    EXPECT_EQ(kDigitTable['.'], 255);
    EXPECT_EQ(kDigitTable[0], 255);
    EXPECT_EQ(kDigitTable[255], 255);
}

TEST(Pow10Table, PowersAreExact) {
    EXPECT_EQ(kPow10[0], 1);
    EXPECT_EQ(kPow10[1], 10);
    EXPECT_EQ(kPow10[4], 10'000);
    EXPECT_EQ(kPow10[18], 1'000'000'000'000'000'000LL);
}

// --- parse_uint ------------------------------------------------------------------

TEST(ParseUint, ParsesDecimals) {
    EXPECT_EQ(parse_uint("0").value_or(-1), 0);
    EXPECT_EQ(parse_uint("42").value_or(-1), 42);
    EXPECT_EQ(parse_uint("007").value_or(-1), 7);
    EXPECT_EQ(parse_uint("9223372036854775807").value_or(-1),
              9'223'372'036'854'775'807LL);
}

TEST(ParseUint, RejectsJunkAndSigns) {
    EXPECT_EQ(parse_uint(""), std::nullopt);
    EXPECT_EQ(parse_uint("12a"), std::nullopt);
    EXPECT_EQ(parse_uint("-5"), std::nullopt) << "unsigned parser: no '-'";
    EXPECT_EQ(parse_uint("+5"), std::nullopt);
    EXPECT_EQ(parse_uint("1.5"), std::nullopt);
    EXPECT_EQ(parse_uint(" 1"), std::nullopt);
}

TEST(ParseUint, RejectsOverflowWithoutUB) {
    // One past INT64_MAX, and far past. If your overflow check multiplies
    // first and tests later, the CONSTEXPR test below won't compile — and
    // here, UBSan (asan preset) flags it.
    EXPECT_EQ(parse_uint("9223372036854775808"), std::nullopt);
    EXPECT_EQ(parse_uint("99999999999999999999"), std::nullopt);
}

TEST(ParseUint, RunsAtCompileTime) {
    // `constexpr` on the variable FORCES compile-time evaluation: by the
    // time this test runs, these were already computed by the compiler.
    constexpr auto ok = parse_uint("1234567");
    constexpr auto overflow = parse_uint("9223372036854775808");
    EXPECT_EQ(ok.value_or(-1), 1'234'567);
    EXPECT_EQ(overflow.value_or(-1), -1);
}

// --- sum_all ---------------------------------------------------------------------

TEST(SumAll, FoldsAnyArity) {
    EXPECT_EQ(sum_all(), 0) << "empty pack: the fold needs an init value";
    EXPECT_EQ(sum_all(42), 42);
    EXPECT_EQ(sum_all(1, 2, 3, 4), 10);
    constexpr auto ct = sum_all(10, 20, 30);  // must fold at compile time
    EXPECT_EQ(ct, 60);
}

// --- Dispatcher --------------------------------------------------------------------

struct AddHandler {
    static constexpr char kType = 'A';
    int calls = 0;
    std::string last_payload;
    void operator()(std::string_view payload) {
        ++calls;
        last_payload = std::string(payload);
    }
};

struct ExecHandler {
    static constexpr char kType = 'E';
    int calls = 0;
    void operator()(std::string_view) { ++calls; }
};

// Same tag as AddHandler — used by the first-match-wins test.
struct ShadowAddHandler {
    static constexpr char kType = 'A';
    int calls = 0;
    void operator()(std::string_view) { ++calls; }
};

TEST(Dispatcher, RoutesByTypeTag) {
    Dispatcher d{AddHandler{}, ExecHandler{}};  // CTAD, THEORY §4

    EXPECT_TRUE(d.dispatch('A', "add-payload"));
    EXPECT_TRUE(d.dispatch('E', "exec-payload"));
    EXPECT_TRUE(d.dispatch('A', "add-again"));

    EXPECT_EQ(std::get<0>(d.handlers()).calls, 2);
    EXPECT_EQ(std::get<1>(d.handlers()).calls, 1);
}

TEST(Dispatcher, StatePersistsBecauseHandlersAreInvokedInPlace) {
    Dispatcher d{AddHandler{}, ExecHandler{}};
    d.dispatch('A', "first");
    d.dispatch('A', "second");
    EXPECT_EQ(std::get<0>(d.handlers()).last_payload, "second")
        << "handlers must be invoked on the STORED instances (auto&..., "
           "not auto...)";
}

TEST(Dispatcher, UnknownTagIsReportedNotDropped) {
    Dispatcher d{AddHandler{}, ExecHandler{}};
    EXPECT_FALSE(d.dispatch('X', "mystery"));
    EXPECT_EQ(std::get<0>(d.handlers()).calls, 0);
    EXPECT_EQ(std::get<1>(d.handlers()).calls, 0);
}

TEST(Dispatcher, FirstMatchingHandlerWins) {
    Dispatcher d{AddHandler{}, ShadowAddHandler{}};
    EXPECT_TRUE(d.dispatch('A', "x"));
    EXPECT_EQ(std::get<0>(d.handlers()).calls, 1);
    EXPECT_EQ(std::get<1>(d.handlers()).calls, 0)
        << "the || fold must short-circuit left to right";
}

TEST(Dispatcher, SingleHandlerAlsoWorks) {
    Dispatcher d{ExecHandler{}};
    EXPECT_TRUE(d.dispatch('E', ""));
    EXPECT_FALSE(d.dispatch('A', ""));
}

}  // namespace
