#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace course {

/// Prices are fixed-point: an int64_t count of ticks at 4 decimal places.
/// "155.1" <-> 1'551'000 ticks.
inline constexpr std::int64_t kPriceScale = 10'000;

/// Parses a decimal price string into ticks.
///
/// Grammar: '-'? digit+ ('.' digit{1,4})?
/// Returns std::nullopt for anything else: empty input, stray characters,
/// missing digits, more than 4 fraction digits, or a value that overflows
/// int64_t ticks. Never uses floating point.
std::optional<std::int64_t> parse_price(std::string_view text);

/// Formats ticks as the canonical decimal string: no trailing zeros in the
/// fraction, no '.' when the fraction is zero. format_price(-1) == "-0.0001".
/// Inverse of parse_price for canonical strings.
std::string format_price(std::int64_t ticks);

}  // namespace course
