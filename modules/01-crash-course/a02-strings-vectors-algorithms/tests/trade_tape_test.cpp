// The spec for a02-strings-vectors-algorithms.
#include "course/trade_tape.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

using namespace course;

// --- split -------------------------------------------------------------------

TEST(Split, BasicFields) {
    const std::string text = "1700,AAPL,1551000,100,B";
    const auto parts = split(text, ',');
    ASSERT_EQ(parts.size(), 5u);
    EXPECT_EQ(parts[0], "1700");
    EXPECT_EQ(parts[1], "AAPL");
    EXPECT_EQ(parts[4], "B");
}

TEST(Split, EmptyFieldsArePreserved) {
    const std::string text = "a,,b";
    const auto parts = split(text, ',');
    ASSERT_EQ(parts.size(), 3u);
    EXPECT_EQ(parts[0], "a");
    EXPECT_EQ(parts[1], "");
    EXPECT_EQ(parts[2], "b");
}

TEST(Split, EdgeShapes) {
    const std::string empty;
    EXPECT_EQ(split(empty, ','), (std::vector<std::string_view>{""}));

    const std::string lone = ",";
    EXPECT_EQ(split(lone, ','), (std::vector<std::string_view>{"", ""}));

    const std::string trailing = "a,b,";
    EXPECT_EQ(split(trailing, ','),
              (std::vector<std::string_view>{"a", "b", ""}));

    const std::string none = "abc";
    EXPECT_EQ(split(none, ','), (std::vector<std::string_view>{"abc"}));
}

TEST(Split, ViewsAliasTheInput) {
    // Zero-copy means the views point INTO the input buffer.
    const std::string text = "xy,z";
    const auto parts = split(text, ',');
    ASSERT_EQ(parts.size(), 2u);
    EXPECT_EQ(parts[0].data(), text.data());
    EXPECT_EQ(parts[1].data(), text.data() + 3);
}

// --- parse_trade -------------------------------------------------------------

TEST(ParseTrade, ParsesAValidLine) {
    const auto t = parse_trade("1700000000123456789,AAPL,1551000,100,B");
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->timestamp_ns, 1'700'000'000'123'456'789);
    EXPECT_EQ(t->symbol, "AAPL");
    EXPECT_EQ(t->price_ticks, 1'551'000);
    EXPECT_EQ(t->quantity, 100);
    EXPECT_EQ(t->side, 'B');
}

TEST(ParseTrade, RejectsWrongFieldCount) {
    EXPECT_EQ(parse_trade("1,AAPL,2,3"), std::nullopt);          // 4 fields
    EXPECT_EQ(parse_trade("1,AAPL,2,3,B,extra"), std::nullopt);  // 6 fields
    EXPECT_EQ(parse_trade(""), std::nullopt);
}

TEST(ParseTrade, RejectsBadFields) {
    EXPECT_EQ(parse_trade("abc,AAPL,2,3,B"), std::nullopt);   // timestamp
    EXPECT_EQ(parse_trade("1,,2,3,B"), std::nullopt);         // empty symbol
    EXPECT_EQ(parse_trade("1,AAPL,2.5,3,B"), std::nullopt);   // non-integer price
    EXPECT_EQ(parse_trade("1,AAPL,2,0,B"), std::nullopt);     // qty must be > 0
    EXPECT_EQ(parse_trade("1,AAPL,2,-5,B"), std::nullopt);    // negative qty
    EXPECT_EQ(parse_trade("1,AAPL,2,3,X"), std::nullopt);     // bad side
    EXPECT_EQ(parse_trade("1,AAPL,2,3,BB"), std::nullopt);    // side too long
}

// --- parse_tape ----------------------------------------------------------------

TEST(ParseTape, SkipsMalformedAndEmptyLines) {
    const std::string tape =
        "1,AAPL,10,5,B\n"
        "this is not a trade\n"
        "\n"
        "2,MSFT,20,7,S\n";
    const auto trades = parse_tape(tape);
    ASSERT_EQ(trades.size(), 2u);
    EXPECT_EQ(trades[0].symbol, "AAPL");
    EXPECT_EQ(trades[1].symbol, "MSFT");
    EXPECT_EQ(trades[1].side, 'S');
}

TEST(ParseTape, TradesOutliveTheTapeText) {
    std::vector<Trade> trades;
    {
        std::string tape = "1,AAPL,10,5,B\n2,GOOG,30,2,S\n";
        trades = parse_tape(tape);
        tape.assign(200, 'x');  // clobber the buffer the views pointed into
    }
    // Trade::symbol owns its bytes, so this is safe. ASan will scream if
    // your Trade accidentally kept views into the dead tape.
    ASSERT_EQ(trades.size(), 2u);
    EXPECT_EQ(trades[0].symbol, "AAPL");
    EXPECT_EQ(trades[1].symbol, "GOOG");
}

// --- analytics -----------------------------------------------------------------

std::vector<Trade> fixture() {
    return parse_tape(
        "100,AAPL,1000000,10,B\n"
        "200,MSFT,2000000,5,S\n"
        "300,AAPL,1100000,20,S\n"
        "400,AAPL,1060000,10,B\n"
        "500,MSFT,2020000,25,B\n");
}

TEST(Analytics, TotalVolume) {
    const auto trades = fixture();
    EXPECT_EQ(total_volume(trades, "AAPL"), 40);
    EXPECT_EQ(total_volume(trades, "MSFT"), 30);
    EXPECT_EQ(total_volume(trades, "TSLA"), 0);
}

TEST(Analytics, VwapIsExactIntegerMath) {
    const auto trades = fixture();
    // AAPL: (1000000*10 + 1100000*20 + 1060000*10) / 40 = 1065000 exactly.
    EXPECT_EQ(vwap(trades, "AAPL"), 1'065'000);
    // MSFT: (2000000*5 + 2020000*25) / 30 = 2016666.66... -> truncates.
    EXPECT_EQ(vwap(trades, "MSFT"), 2'016'666);
    EXPECT_EQ(vwap(trades, "TSLA"), std::nullopt);
}

TEST(Analytics, MostActiveSymbol) {
    const auto trades = fixture();
    EXPECT_EQ(most_active_symbol(trades), "AAPL");  // 40 vs 30
    EXPECT_EQ(most_active_symbol({}), "");
}

TEST(Analytics, MostActiveTieBreaksLexicographically) {
    const auto trades = parse_tape(
        "1,ZZZ,10,7,B\n"
        "2,AAA,10,3,S\n"
        "3,AAA,10,4,B\n");  // both symbols total 7
    EXPECT_EQ(most_active_symbol(trades), "AAA");
}

TEST(Analytics, SortedByTimeIsStable) {
    const auto trades = parse_tape(
        "300,CCC,1,1,B\n"
        "100,AAA,1,1,B\n"
        "200,FIRST,1,1,B\n"
        "200,SECOND,1,1,S\n");
    const auto sorted = sorted_by_time(trades);
    ASSERT_EQ(sorted.size(), 4u);
    EXPECT_EQ(sorted[0].symbol, "AAA");
    EXPECT_EQ(sorted[1].symbol, "FIRST");   // equal timestamps keep
    EXPECT_EQ(sorted[2].symbol, "SECOND");  // their original order
    EXPECT_EQ(sorted[3].symbol, "CCC");

    // Input is untouched (you sorted a copy).
    EXPECT_EQ(trades[0].symbol, "CCC");
}

}  // namespace
