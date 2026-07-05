#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <utility>

namespace course {

/// Single-producer/single-consumer, single-slot mailbox. The payload is
/// deliberately a PLAIN int64 — the atomic flag's orderings are what make
/// the handoff legal (README THEORY §2, Hint 1). Every ordering below is
/// wrong on purpose; fixing them is the assignment.
class MailBox {
public:
    /// Producer side: if the box is empty, write the payload, publish it,
    /// and return true; if still full, return false (caller retries).
    bool try_post(std::int64_t payload) {
        // TODO: check full_, write payload_, then publish via full_ —
        // with the ordering that makes the payload write visible to
        // take(). The stub "succeeds" without doing anything so the
        // tests fail fast instead of spinning forever.
        (void)payload;
        return true;
    }

    /// Consumer side: nullopt unless a payload is published; consumes the
    /// publication (the next take() is nullopt until the next post()).
    std::optional<std::int64_t> take() {
        // TODO (Hint 2 — consuming publishes "slot free" back).
        return std::nullopt;
    }

private:
    std::int64_t payload_ = 0;
    std::atomic<bool> full_{false};
};

/// Double-checked lazy initialization (README THEORY §4). get_or_create
/// must construct exactly once under any concurrency, and fast-path
/// readers must see the fully constructed object.
template <typename T>
class LazyBox {
public:
    template <typename... Args>
    T& get_or_create(Args&&... args) {
        // TODO: fast-path acquire load; slow path under the mutex with a
        // relaxed re-check (Hint 3), construct, release-store.
        ((void)args, ...);
        static T dummy{};  // placeholder so the stub compiles & links
        return dummy;
    }

    /// How many times construction ran. Must end up exactly 1.
    int created() const { return created_.load(std::memory_order_relaxed); }

    ~LazyBox() { delete ptr_.load(std::memory_order_relaxed); }

private:
    std::atomic<T*> ptr_{nullptr};
    std::mutex mutex_;
    std::atomic<int> created_{0};
};

/// Event counter for statistics. Relaxed is CORRECT here — your job is
/// the one-paragraph argument why (write it in this comment when you
/// implement): what does relaxed guarantee, and why is that enough for a
/// count that is only read after the threads join?
class RelaxedCounter {
public:
    void add(std::int64_t n) {
        // TODO
        (void)n;
    }

    std::int64_t total() const {
        // TODO
        return -1;
    }

private:
    std::atomic<std::int64_t> count_{0};
};

}  // namespace course
