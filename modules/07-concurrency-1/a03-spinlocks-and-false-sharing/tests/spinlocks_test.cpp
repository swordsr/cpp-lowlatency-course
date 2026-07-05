// The spec for m07a03. The mutual-exclusion tests protect a PLAIN int64
// with your lock: if the lock is broken the count comes out wrong here,
// and the tsan preset names the race outright. Green = debug + asan + tsan.
#include "course/spinlocks.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <thread>
#include <vector>

namespace {

using course::BackoffSpinLock;
using course::kDestructiveInterference;
using course::PaddedCounters;
using course::TasSpinLock;
using course::TtasSpinLock;
using course::UnpaddedCounters;

template <typename Lock>
std::int64_t hammer_counter(int threads, int iters_per_thread) {
    Lock lock;
    std::int64_t counter = 0;  // plain! the lock is its only protection
    {
        std::vector<std::jthread> workers;
        for (int t = 0; t < threads; ++t) {
            workers.emplace_back([&] {
                for (int i = 0; i < iters_per_thread; ++i) {
                    std::lock_guard guard{lock};
                    ++counter;
                }
            });
        }
    }
    return counter;
}

template <typename Lock>
void check_try_lock_semantics() {
    Lock lock;
    EXPECT_TRUE(lock.try_lock()) << "free lock must be acquirable";

    bool second_attempt = true;
    std::jthread other{[&] { second_attempt = lock.try_lock(); }};
    other.join();
    EXPECT_FALSE(second_attempt) << "held lock must refuse try_lock";

    lock.unlock();
    bool third_attempt = false;
    std::jthread another{[&] {
        third_attempt = lock.try_lock();
        if (third_attempt) lock.unlock();
    }};
    another.join();
    EXPECT_TRUE(third_attempt) << "released lock must be acquirable again";
}

TEST(TasSpinLockTest, MutualExclusion) {
    EXPECT_EQ(hammer_counter<TasSpinLock>(4, 50'000), 200'000)
        << "increments were lost: the lock does not exclude";
}

TEST(TasSpinLockTest, TryLockSemantics) {
    check_try_lock_semantics<TasSpinLock>();
}

TEST(TtasSpinLockTest, MutualExclusion) {
    EXPECT_EQ(hammer_counter<TtasSpinLock>(4, 50'000), 200'000);
}

TEST(TtasSpinLockTest, TryLockSemantics) {
    check_try_lock_semantics<TtasSpinLock>();
}

TEST(BackoffSpinLockTest, MutualExclusion) {
    EXPECT_EQ(hammer_counter<BackoffSpinLock>(4, 50'000), 200'000);
}

TEST(BackoffSpinLockTest, TryLockSemantics) {
    check_try_lock_semantics<BackoffSpinLock>();
}

TEST(BackoffSpinLockTest, UncontendedLockUnlockWorks) {
    BackoffSpinLock lock;
    for (int i = 0; i < 1'000; ++i) {
        lock.lock();
        lock.unlock();
    }
    SUCCEED();
}

// --- layout ------------------------------------------------------------------------

std::ptrdiff_t address_distance(const void* p, const void* q) {
    return std::abs(reinterpret_cast<const char*>(p) -
                    reinterpret_cast<const char*>(q));
}

TEST(FalseSharing, UnpaddedCountersShareALine) {
    // Observation test (green from day one): this is the bug, preserved.
    UnpaddedCounters c;
    EXPECT_LT(address_distance(&c.a, &c.b),
              static_cast<std::ptrdiff_t>(kDestructiveInterference))
        << "if this fails your compiler padded spontaneously — interesting, "
           "report it";
}

TEST(FalseSharing, PaddedCountersDoNot) {
    PaddedCounters c;
    EXPECT_GE(address_distance(&c.a, &c.b),
              static_cast<std::ptrdiff_t>(kDestructiveInterference))
        << "a and b must be at least one destructive-interference size "
           "apart (Hint 3: alignas EACH member)";
}

TEST(FalseSharing, PaddedCountersStillCount) {
    PaddedCounters c;
    {
        std::vector<std::jthread> workers;
        workers.emplace_back([&] {
            for (int i = 0; i < 10'000; ++i) c.add_a(1);
        });
        workers.emplace_back([&] {
            for (int i = 0; i < 10'000; ++i) c.add_b(2);
        });
    }
    EXPECT_EQ(c.total_a(), 10'000);
    EXPECT_EQ(c.total_b(), 20'000);
}

}  // namespace
