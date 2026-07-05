#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <tuple>

namespace course {

// ---------------------------------------------------------------------------
// Part 1 — compile-time tables (THEORY §2).
// ---------------------------------------------------------------------------

/// '0'..'9' -> 0..9; every other byte -> 255.
constexpr std::array<std::uint8_t, 256> make_digit_table() {
    std::array<std::uint8_t, 256> table{};
    // TODO (Hint 1). Note: the stub's zero-filled table wrongly maps EVERY
    // byte to "digit 0" — the tests will let you know.
    return table;
}

/// 10^0 .. 10^18 (the last power of ten that fits in int64).
constexpr std::array<std::int64_t, 19> make_pow10_table() {
    std::array<std::int64_t, 19> table{};
    // TODO (Hint 1).
    return table;
}

/// The tables, baked into .rodata at compile time.
inline constexpr auto kDigitTable = make_digit_table();
inline constexpr auto kPow10 = make_pow10_table();

// ---------------------------------------------------------------------------
// Part 2 — a constexpr, table-driven parser.
// ---------------------------------------------------------------------------

/// Non-negative decimal integer via kDigitTable. nullopt for empty input,
/// any non-digit byte, or overflow past int64 max. Must be UB-free even on
/// hostile input — compile-time evaluations turn UB into build errors
/// (THEORY §1, Hint 2).
constexpr std::optional<std::int64_t> parse_uint(std::string_view text) {
    // TODO
    (void)text;
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// Part 3 — packs and folds.
// ---------------------------------------------------------------------------

/// Sum of all arguments as int64; sum_all() with no arguments is 0.
template <typename... Ts>
constexpr std::int64_t sum_all(Ts... xs) {
    // TODO: one fold expression (THEORY §3).
    ((void)xs, ...);
    return 0;
}

// ---------------------------------------------------------------------------
// Part 4 — the variadic dispatcher (THEORY §4).
// ---------------------------------------------------------------------------

/// Routes (type tag, payload) to the stored handler whose kType matches.
/// Handlers are stored BY VALUE and invoked on the stored instance — their
/// state persists across dispatch calls. First matching handler wins.
template <typename... Handlers>
class Dispatcher {
public:
    explicit Dispatcher(Handlers... handlers)
        : handlers_(std::move(handlers)...) {}

    /// Invokes the matching handler with `payload`; returns true iff one
    /// matched.
    bool dispatch(char type, std::string_view payload) {
        // TODO (Hint 3).
        (void)type;
        (void)payload;
        return false;
    }

    /// Test access to the stored handlers: std::get<I>(d.handlers()).
    std::tuple<Handlers...>& handlers() { return handlers_; }

private:
    std::tuple<Handlers...> handlers_;
};

}  // namespace course
