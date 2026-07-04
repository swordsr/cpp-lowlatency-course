#include "course/price.hpp"

namespace course {

std::optional<std::int64_t> parse_price(std::string_view text) {
    // TODO: implement (see README §"Assignment spec" and the tests).
    (void)text;
    return std::nullopt;
}

std::string format_price(std::int64_t ticks) {
    // TODO: implement (see README §"Assignment spec" and the tests).
    (void)ticks;
    return {};
}

}  // namespace course
