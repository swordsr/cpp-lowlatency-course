#pragma once

#include <atomic>
#include <cstdint>
#include <cstddef>
#include <new>
#include <thread>

namespace course {

/// Destructive interference size with a fallback: libc++ has shipped
/// without std::hardware_destructive_interference_size for years, and on
/// Apple Silicon the true line size is 128 anyway.
#ifdef __cpp_lib_hardware_interference_size
inline constexpr std::size_t kDestructiveInterference =
    std::hardware_destructive_interference_size;
#else
inline constexpr std::size_t kDestructiveInterference = 128;
#endif

/// One polite spin-wait beat. On real hot paths this is a PAUSE (x86) /
/// YIELD or WFE (ARM) instruction; yield() is the portable stand-in and
/// good enough for this course's benchmarks.
inline void cpu_pause() { std::this_thread::yield(); }

/// Test-and-set spinlock: every spin iteration is a WRITE (README §2).
/// Meets the Lockable requirements — usable with std::lock_guard.
class TasSpinLock {
public:
    void lock() {
        // TODO (Hint 1).
    }

    bool try_lock() {
        // TODO
        return false;
    }

    void unlock() {
        // TODO — release, and be able to say why (README §2).
    }

private:
    std::atomic<bool> flag_{false};
};

/// Test-and-TEST-and-set: spin on local reads, exchange only when the
/// lock looks free. Same interface, radically less coherence traffic.
class TtasSpinLock {
public:
    void lock() {
        // TODO (Hint 2).
    }

    bool try_lock() {
        // TODO
        return false;
    }

    void unlock() {
        // TODO
    }

private:
    std::atomic<bool> flag_{false};
};

/// TTAS + exponential backoff: failed attempts wait 1, 2, 4, ... pauses
/// (cap at kMaxBackoff) so the release stampede collapses.
class BackoffSpinLock {
public:
    static constexpr int kMaxBackoff = 64;

    void lock() {
        // TODO
    }

    bool try_lock() {
        // TODO
        return false;
    }

    void unlock() {
        // TODO
    }

private:
    std::atomic<bool> flag_{false};
};

/// The bug, preserved for the benchmark: two "independent" hot counters
/// sharing one cache line (README §3). Do not fix this one.
struct UnpaddedCounters {
    std::atomic<std::int64_t> a{0};
    std::atomic<std::int64_t> b{0};

    void add_a(std::int64_t n) { a.fetch_add(n, std::memory_order_relaxed); }
    void add_b(std::int64_t n) { b.fetch_add(n, std::memory_order_relaxed); }
    std::int64_t total_a() const { return a.load(std::memory_order_relaxed); }
    std::int64_t total_b() const { return b.load(std::memory_order_relaxed); }
};

/// Same interface, YOUR layout: the two counters must live on different
/// cache lines (Hint 3 — the test measures their address distance).
struct PaddedCounters {
    // TODO: lay out a and b so they can't ping-pong.
    std::atomic<std::int64_t> a{0};
    std::atomic<std::int64_t> b{0};

    void add_a(std::int64_t n) { a.fetch_add(n, std::memory_order_relaxed); }
    void add_b(std::int64_t n) { b.fetch_add(n, std::memory_order_relaxed); }
    std::int64_t total_a() const { return a.load(std::memory_order_relaxed); }
    std::int64_t total_b() const { return b.load(std::memory_order_relaxed); }
};

}  // namespace course
