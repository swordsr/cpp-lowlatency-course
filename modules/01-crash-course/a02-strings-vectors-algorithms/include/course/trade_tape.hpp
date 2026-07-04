#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace course {

/// One executed trade. `symbol` owns its bytes (std::string, not
/// string_view) so a Trade can outlive the tape text it was parsed from.
struct Trade {
    std::int64_t timestamp_ns = 0;
    std::string symbol;
    std::int64_t price_ticks = 0;  // fixed-point, scale 10^4 (see m00a01)
    std::int32_t quantity = 0;
    char side = '?';  // 'B' or 'S'
};

/// Zero-copy tokenizer. Views point into `text`; caller keeps `text` alive.
/// split("a,,b", ',') == {"a", "", "b"};  split("", ',') == {""}.
std::vector<std::string_view> split(std::string_view text, char sep);

/// Parses "timestamp_ns,symbol,price_ticks,quantity,side".
/// Exactly 5 fields; integers are plain decimal (no sign except that
/// timestamp/price must be >= 0 and quantity > 0); symbol non-empty;
/// side 'B' or 'S'. Anything else -> nullopt.
std::optional<Trade> parse_trade(std::string_view line);

/// Splits on '\n' and parses every line, skipping empty and malformed ones.
std::vector<Trade> parse_tape(std::string_view text);

/// Sum of quantities of trades in `symbol`.
std::int64_t total_volume(const std::vector<Trade>& trades,
                          std::string_view symbol);

/// Volume-weighted average price of `symbol`, in ticks, truncated toward
/// zero. nullopt if the symbol has no trades. Assumes no int64 overflow.
std::optional<std::int64_t> vwap(const std::vector<Trade>& trades,
                                 std::string_view symbol);

/// Symbol with the largest total quantity; ties -> lexicographically
/// smallest; empty input -> "".
std::string most_active_symbol(const std::vector<Trade>& trades);

/// Copy of `trades` sorted by timestamp_ns, stable for equal timestamps.
std::vector<Trade> sorted_by_time(std::vector<Trade> trades);

}  // namespace course
