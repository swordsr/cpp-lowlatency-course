#pragma once

#include <cstdint>

namespace course {

/// CRTP mixin. Derive as `struct T : Comparisons<T>` and define ONLY
/// `operator==` and `operator<` (members are fine); the mixin synthesizes
/// `>`, `<=` and `>=` on top of them (`!=` needs no synthesis in C++20 —
/// see the note inside the class).
///
/// Inside these member functions `*this` is a Comparisons<Derived> — the
/// base subobject. The left-hand operand you actually want is the Derived
/// object it lives inside: `static_cast<const Derived&>(*this)`. That
/// downcast is safe by construction (the only thing that ever inherits
/// Comparisons<Derived> is Derived itself) and it is legal to write here
/// even though Derived is incomplete at this point — these bodies are not
/// instantiated until someone calls them, long after Derived is complete
/// (README THEORY §1).
template <typename Derived>
struct Comparisons {
    // No operator!= here — and that absence is a lesson. Since C++20 the
    // compiler REWRITES `a != b` as `!(a == b)` whenever an operator== is
    // visible, and that rewritten candidate beats an inherited member !=
    // in overload resolution (exact-match object parameter vs. a
    // derived-to-base binding). A != in this mixin would be unreachable
    // dead code. The three below have no rewritten competitor unless the
    // type defines operator<=> — which is the whole modern replacement for
    // this mixin (README THEORY §2).

    bool operator>(const Derived& rhs) const {
        // TODO: derive from Derived::operator< with the operands arranged
        // appropriately.
        (void)rhs;
        return false;
    }

    bool operator<=(const Derived& rhs) const {
        // TODO: a <= b  ==  !(some primitive comparison).
        (void)rhs;
        return false;
    }

    bool operator>=(const Derived& rhs) const {
        // TODO
        (void)rhs;
        return false;
    }
};

/// One rung of an order book. Given complete — it is the fixture the tests
/// exercise the mixin through, not the lesson. Note what it does NOT
/// define: no !=, >, <=, >= anywhere in this struct.
struct PriceLevel : Comparisons<PriceLevel> {
    std::int64_t price = 0;  // fixed-point ticks (Module 1 convention)
    std::int32_t qty = 0;

    // given: the two primitives; the mixin synthesizes the rest.
    bool operator==(const PriceLevel& rhs) const {
        return price == rhs.price && qty == rhs.qty;
    }
    bool operator<(const PriceLevel& rhs) const {
        if (price != rhs.price) return price < rhs.price;
        return qty < rhs.qty;
    }
};

}  // namespace course
