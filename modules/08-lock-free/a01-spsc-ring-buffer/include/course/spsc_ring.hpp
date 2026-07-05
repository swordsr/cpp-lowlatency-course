#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

namespace course {

/// Wait-free single-producer/single-consumer ring buffer. Exactly one
/// thread may push and exactly one may pop, ever. Monotonic uint64
/// cursors; slot = index & (N-1); occupancy = tail - head; full at N.
template <typename T, std::size_t N>
class SpscRing {
    static_assert(N > 0 && (N & (N - 1)) == 0, "N must be a power of two");

public:
    /// Alignment for the cursors (Apple Silicon line size; see m07a03).
    static constexpr std::size_t kAlign = 128;

    /// Producer only. False when full; on success the value is moved in.
    bool try_push(T value) {
        // TODO (Hints 1-2).
        (void)value;
        return false;
    }

    /// Consumer only. nullopt when empty; on success the value is moved
    /// out BEFORE the slot is returned to the producer (Hint 3).
    std::optional<T> try_pop() {
        // TODO
        return std::nullopt;
    }

    /// Approximate occupancy: exact only when no other thread is active
    /// (a racy snapshot is inherent and fine — document, don't fight it).
    std::size_t size() const {
        // TODO
        return 0;
    }

    static constexpr std::size_t capacity() { return N; }

private:
    std::array<T, N> slots_{};
    // TODO: the two cursors — each written by exactly one side, each
    // padded so they can't false-share (README THEORY §3, Hint 1).
    std::atomic<std::uint64_t> head_{0};
    std::atomic<std::uint64_t> tail_{0};
};

}  // namespace course
