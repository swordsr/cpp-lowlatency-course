#include "course/trade_tape.hpp"

namespace course {

std::vector<std::string_view> split(std::string_view text, char sep) {
    // TODO (Hint 1)
    (void)text;
    (void)sep;
    return {};
}

std::optional<Trade> parse_trade(std::string_view line) {
    // TODO (Hint 2)
    (void)line;
    return std::nullopt;
}

std::vector<Trade> parse_tape(std::string_view text) {
    // TODO
    (void)text;
    return {};
}

std::int64_t total_volume(const std::vector<Trade>& trades,
                          std::string_view symbol) {
    // TODO (Hint 3)
    (void)trades;
    (void)symbol;
    return 0;
}

std::optional<std::int64_t> vwap(const std::vector<Trade>& trades,
                                 std::string_view symbol) {
    // TODO
    (void)trades;
    (void)symbol;
    return std::nullopt;
}

std::string most_active_symbol(const std::vector<Trade>& trades) {
    // TODO (Hint 3)
    (void)trades;
    return {};
}

std::vector<Trade> sorted_by_time(std::vector<Trade> trades) {
    // TODO (Hint 3)
    return trades;
}

}  // namespace course
