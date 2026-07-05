#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>

namespace course {

/// Unbounded MPMC blocking FIFO with close() shutdown semantics:
/// after close(), push() refuses, pop() drains what's left and then
/// returns nullopt. All members are guarded by mutex_ — no exceptions.
template <typename T>
class BlockingQueue {
public:
    /// Enqueues and wakes one waiter. Returns false (dropping the value)
    /// if the queue is closed.
    bool push(T value) {
        // TODO
        (void)value;
        return false;
    }

    /// Blocks until an item is available or the queue is closed. Drains
    /// remaining items after close; nullopt only when closed AND empty.
    std::optional<T> pop() {
        // TODO (Hint 2 — the predicate encodes the drain semantics).
        return std::nullopt;
    }

    /// Non-blocking: takes an item if one is ready.
    bool try_pop(T& out) {
        // TODO
        (void)out;
        return false;
    }

    std::size_t size() const {
        // TODO
        return 0;
    }

    /// Idempotent. Wakes ALL waiters (README commandment 3).
    void close() {
        // TODO
    }

private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool closed_ = false;
};

}  // namespace course
