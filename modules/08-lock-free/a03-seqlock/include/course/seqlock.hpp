#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace course {

/// Sequence lock: one never-blocked writer, optimistic retrying readers
/// (README THEORY §1-2). The payload is stored as relaxed atomic words so
/// the whole thing is UB-free and tsan-clean by construction — your job
/// is the sequence/fence protocol, not a serializer.
template <typename T>
class SeqLock {
    static_assert(std::is_trivially_copyable_v<T>,
                  "readers copy the payload out; T must be memcpy-safe");
    static_assert(sizeof(T) % 8 == 0,
                  "pad T to a multiple of 8 bytes (the word store below)");

public:
    /// Writer only; wait-free. Completes the snapshot and bumps version()
    /// by exactly one.
    void store(const T& value) {
        // TODO (Hint 1 — the release FENCE is not optional).
        (void)value;
    }

    /// Any reader; retries until it obtains a consistent snapshot.
    T load() const {
        // TODO (Hint 2).
        return T{};
    }

    /// Number of completed stores (from a quiet state).
    std::uint64_t version() const {
        // TODO (Hint 3).
        return 0;
    }

private:
    static constexpr std::size_t kWords = sizeof(T) / 8;

    std::atomic<std::uint64_t> seq_{0};
    std::array<std::atomic<std::uint64_t>, kWords> words_{};
};

}  // namespace course
