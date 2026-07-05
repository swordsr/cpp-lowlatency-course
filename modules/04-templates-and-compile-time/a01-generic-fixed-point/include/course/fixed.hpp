#pragma once

#include <compare>
#include <cstdint>
#include <type_traits>

namespace course {

/// Fixed-point value with the scale carried in the type: Fixed<10'000>
/// stores 4-decimal-place prices as int64 ticks. Different scales are
/// different types — mixing them does not compile.
template <std::int64_t Scale>
class Fixed {
    static_assert(Scale > 0, "scale must be positive");

public:
    static constexpr std::int64_t kScale = Scale;

    constexpr Fixed() = default;

    /// Value is `ticks` raw ticks: from_ticks(1'551'000) at Scale 10'000
    /// is the price 155.1.
    static constexpr Fixed from_ticks(std::int64_t ticks) {
        // TODO
        (void)ticks;
        return Fixed{};
    }

    /// Value is `units` whole units: from_units(155) is the price 155.0.
    static constexpr Fixed from_units(std::int64_t units) {
        // TODO
        (void)units;
        return Fixed{};
    }

    constexpr std::int64_t ticks() const {
        // TODO
        return 0;
    }

    /// Whole units, truncated toward zero: 155.1 -> 155, -155.1 -> -155.
    constexpr std::int64_t units_truncated() const {
        // TODO
        return 0;
    }

    /// Same-scale arithmetic only — cross-scale operands must not compile.
    constexpr Fixed operator+(Fixed rhs) const {
        // TODO
        (void)rhs;
        return Fixed{};
    }

    constexpr Fixed operator-(Fixed rhs) const {
        // TODO
        (void)rhs;
        return Fixed{};
    }

    /// Scaling by a dimensionless integer (e.g. a quantity) keeps the type.
    constexpr Fixed operator*(std::int64_t qty) const {
        // TODO
        (void)qty;
        return Fixed{};
    }

    /// Truncating division by a dimensionless integer.
    constexpr Fixed operator/(std::int64_t divisor) const {
        // TODO
        (void)divisor;
        return Fixed{};
    }

    // Comparisons. Both stubs can be replaced entirely (Hint 1 / THEORY §3).
    constexpr bool operator==(const Fixed& rhs) const {
        // TODO
        (void)rhs;
        return false;
    }

    constexpr std::strong_ordering operator<=>(const Fixed& rhs) const {
        // TODO
        (void)rhs;
        return std::strong_ordering::equal;
    }

    /// Exact conversion to another scale; truncates toward zero when
    /// downscaling. Scales must divide one another (THEORY §4, Hint 2).
    template <std::int64_t ToScale>
    constexpr Fixed<ToScale> rescale() const {
        // TODO
        return Fixed<ToScale>{};
    }

private:
    std::int64_t ticks_ = 0;
};

/// Midpoint without int64 overflow on large operands; truncation is
/// toward `a` (document this to your callers — midpoint(bid, ask) biases
/// toward the bid).
template <std::int64_t S>
constexpr Fixed<S> midpoint(Fixed<S> a, Fixed<S> b) {
    // TODO
    (void)a;
    (void)b;
    return Fixed<S>{};
}

/// Largest multiple of TickTicks (a tick size, in ticks) that is <= p.
/// Rounds toward negative infinity — NOT plain truncation (Hint 3).
template <std::int64_t TickTicks, std::int64_t S>
constexpr Fixed<S> round_down_to_tick(Fixed<S> p) {
    static_assert(TickTicks > 0, "tick size must be positive");
    // TODO
    (void)p;
    return Fixed<S>{};
}

// --- compile-time contracts: these hold in the stub state; keep them true ---

static_assert(sizeof(Fixed<10'000>) == sizeof(std::int64_t),
              "the scale must live in the type, not the object");
static_assert(std::is_trivially_copyable_v<Fixed<10'000>>,
              "a Fixed is just an int64 in a costume");
static_assert(!std::is_same_v<Fixed<10'000>, Fixed<100'000>>,
              "different scales are different types");

// The probe needs template parameters: only SUBSTITUTION failures turn a
// requires-expression false — a plainly invalid expression with concrete
// types is just a compile error.
template <typename A, typename B>
concept AddableWith = requires(A a, B b) { a + b; };
static_assert(AddableWith<Fixed<10'000>, Fixed<10'000>>,
              "same-scale addition must exist");
static_assert(!AddableWith<Fixed<10'000>, Fixed<100'000>>,
              "cross-scale addition must not compile");

}  // namespace course
