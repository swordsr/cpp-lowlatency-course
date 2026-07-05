#pragma once

#include <cstddef>
#include <cstdint>

namespace course {

// ---------------------------------------------------------------------------
// Part 1 — the layout algorithm, by hand.
// ---------------------------------------------------------------------------

/// Rounds offset up to the next multiple of alignment.
/// Precondition: alignment is a power of two (1, 2, 4, 8, ...).
/// aligned_up(0, 8) == 0, aligned_up(1, 8) == 8, aligned_up(8, 8) == 8.
/// Naive modulo arithmetic is fine; the bit trick
/// (offset + alignment - 1) & ~(alignment - 1) is fine too (THEORY §3).
constexpr std::size_t aligned_up(std::size_t offset, std::size_t alignment) {
    // TODO
    (void)offset;
    (void)alignment;
    return 0;
}

/// Computes what sizeof would be for a struct whose members have types
/// Ts... IN THAT ORDER, by replaying the compiler's layout algorithm
/// (THEORY §1): each member goes at the next offset that is a multiple of
/// its alignof, and the final size is rounded up to the largest member
/// alignment (tail padding). E.g. packed_struct_size<char, int>() == 8.
///
/// Everything you need: sizeof(Ts)..., alignof(Ts)..., and aligned_up.
/// A fold expression or a pack-expanded loop over two local arrays both
/// work; pick whichever reads best to you.
template <typename... Ts>
constexpr std::size_t packed_struct_size() {
    // TODO
    return 0;
}

// ---------------------------------------------------------------------------
// Part 2 — reordering. OrderNaive is a fossil: a wire-ish order record laid
// out in the order someone first thought of the fields. Do NOT touch it —
// the tests pin its 40 bytes as a monument to the waste.
// ---------------------------------------------------------------------------

struct OrderNaive {
    char side;           // offset 0, then 7 bytes of padding...
    double price;
    std::int32_t qty;
    char flag;
    std::int64_t id;
    std::int16_t venue;
};

/// Same six members, same names, same meaning — order is yours to choose.
/// Ships in the same wasteful order as OrderNaive.
// TODO: reorder members to reach sizeof == 24
struct OrderCompact {
    char side;
    double price;
    std::int32_t qty;
    char flag;
    std::int64_t id;
    std::int16_t venue;
};

// ---------------------------------------------------------------------------
// Part 3 — paying zero bytes for an empty member.
// ---------------------------------------------------------------------------

/// A value branded with a tag type so different quantities can't be mixed
/// up (TaggedValue<double, PriceTag> vs TaggedValue<double, QtyTag> are
/// distinct types). Tag is typically an EMPTY class — it exists only in the
/// type system — yet as written the tag_ member still costs bytes:
/// sizeof(TaggedValue<double, PriceTag>) is 16, not 8 (THEORY §4).
// TODO: make an empty Tag cost zero bytes, so the size equals sizeof(T).
template <typename T, typename Tag>
class TaggedValue {
public:
    TaggedValue() = default;
    explicit TaggedValue(T value) : value_(value) {}

    T value() const { return value_; }
    void set_value(T v) { value_ = v; }

private:
    T value_{};
    Tag tag_{};
};

}  // namespace course
