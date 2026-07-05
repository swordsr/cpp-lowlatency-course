#pragma once

#include <cstddef>
#include <cstdint>

namespace course {

/// Dense scan: the prefetcher's favorite food.
std::int64_t sum_sequential(const std::int64_t* data, std::size_t n);

/// Sum of data[0], data[stride], data[2*stride], ... (indices < n).
/// Precondition: stride >= 1.
std::int64_t sum_strided(const std::int64_t* data, std::size_t n,
                         std::size_t stride);

/// Independent random access: sum of data[idx[i]] for i in [0, n).
/// Misses galore, but the core can keep many in flight at once.
std::int64_t sum_gather(const std::int64_t* data, const std::uint32_t* idx,
                        std::size_t n);

/// One hop of a linked list. 16 bytes on purpose: several nodes fit in a
/// cache line, so the SHUFFLED chain's misery in the bench is about
/// dependency, not size.
struct Node {
    std::int64_t value;
    Node* next;
};

/// Dependent random access: walk to null, summing values. Each load waits
/// for the previous one — the memory-latency worst case.
std::int64_t chase(const Node* head);

}  // namespace course
