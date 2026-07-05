#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

namespace course {

/// Vyukov's bounded MPMC queue: per-cell sequence numbers arbitrate
/// competing producers/consumers (README THEORY §2). Lock-free; FIFO per
/// producer; capacity is a power of two.
template <typename T>
class MpmcQueue {
public:
    static constexpr std::size_t kAlign = 128;

    explicit MpmcQueue(std::size_t capacity)
        : cells_(new Cell[capacity]), mask_(capacity - 1) {
        if (capacity < 2 || (capacity & (capacity - 1)) != 0) {
            throw std::invalid_argument{"capacity must be a power of two >= 2"};
        }
        for (std::size_t i = 0; i < capacity; ++i) {
            cells_[i].seq.store(i, std::memory_order_relaxed);
        }
    }

    MpmcQueue(const MpmcQueue&) = delete;
    MpmcQueue& operator=(const MpmcQueue&) = delete;

    /// False when full. Never blocks; retries only while losing races.
    bool try_push(T value) {
        // TODO: THEORY §2, left column (Hint 2 — keep the 3-way branch).
        (void)value;
        return false;
    }

    /// nullopt when empty.
    std::optional<T> try_pop() {
        // TODO: THEORY §2, right column.
        return std::nullopt;
    }

    std::size_t capacity() const { return mask_ + 1; }

    /// Approximate; exact only single-threaded.
    std::size_t size() const {
        // TODO
        return 0;
    }

private:
    struct Cell {
        std::atomic<std::uint64_t> seq{0};
        T value{};
    };

    std::unique_ptr<Cell[]> cells_;
    std::size_t mask_;
    // TODO: pad these two so producers and consumers don't false-share
    // each other's cursor (the sizeof test is watching).
    std::atomic<std::uint64_t> enqueue_pos_{0};
    std::atomic<std::uint64_t> dequeue_pos_{0};
};

}  // namespace course
