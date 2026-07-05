#pragma once

#include <concepts>
#include <cstdint>
#include <type_traits>

namespace course {

// ---------------------------------------------------------------------------
// Part 1 — the museum piece: a hand-rolled void_t detector (THEORY §2).
// True iff a `const T` has a callable `.bid()`. You add the partial
// specialization that detects it; the primary template is the fallback.
// ---------------------------------------------------------------------------

template <typename T, typename = void>
struct has_bid : std::false_type {};

// TODO: add the detecting partial specialization (Hint 1).

template <typename T>
inline constexpr bool has_bid_v = has_bid<T>::value;

// ---------------------------------------------------------------------------
// Part 2 — the modern spelling (THEORY §3).
// ---------------------------------------------------------------------------

/// A const T can quote both sides: bid() and ask(), each convertible to
/// int64 ticks.
///
/// Stub note: ships as `true` (not false!) so the constrained functions
/// below remain callable and the tests keep BUILDING; the accepts/rejects
/// tests are red until you write the real requires-expression (Hint 2).
template <typename T>
concept Quotable = true;  // TODO (Hint 2)

/// Refines Quotable: also exposes qty_at_bid()/qty_at_ask() convertible to
/// int32. MUST stay defined in terms of Quotable<T> so it subsumes it
/// (Hint 3) — the weighted_mid overload pair depends on that. The stub's
/// `(sizeof(T) > 0)` is an always-true placeholder for your extra
/// requirements that keeps the subsumption relation (and the build) alive.
template <typename T>
concept TightlyQuotable = Quotable<T> && (sizeof(T) > 0);  // TODO

// ---------------------------------------------------------------------------
// Part 3 — constrained functions.
// ---------------------------------------------------------------------------

/// ask - bid, in ticks.
template <typename Q>
    requires Quotable<Q>
std::int64_t spread(const Q& q) {
    // TODO
    (void)q;
    return 0;
}

/// Plain midpoint (bid+ask)/2, truncating. Selected for types that are
/// Quotable but not TightlyQuotable.
template <typename Q>
    requires Quotable<Q>
std::int64_t weighted_mid(const Q& q) {
    // TODO
    (void)q;
    return 0;
}

/// Quantity-weighted midpoint (bid*qty_ask + ask*qty_bid) / (qty_bid +
/// qty_ask), truncating integer math. Subsumption must make this overload
/// win for TightlyQuotable types — same name, no casts, no tag params.
template <typename Q>
    requires TightlyQuotable<Q>
std::int64_t weighted_mid(const Q& q) {
    // TODO
    (void)q;
    return 0;
}

}  // namespace course
