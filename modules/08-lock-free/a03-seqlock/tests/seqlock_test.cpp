// The spec for m08a03. The torn-snapshot stress is the real referee:
// every loaded snapshot must be internally consistent (all four fields
// from the SAME store). Deadline-bounded. Green = debug + asan + tsan.
#include "course/seqlock.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

namespace {

using course::SeqLock;
using namespace std::chrono_literals;

struct Snapshot {  // 32 bytes, trivially copyable, multiple of 8
    std::int64_t a;
    std::int64_t b;
    std::int64_t c;
    std::int64_t d;
};

TEST(SeqLockTest, RoundTripsSingleThreaded) {
    SeqLock<Snapshot> lock;
    lock.store({1, 2, 3, 4});
    const Snapshot s = lock.load();
    EXPECT_EQ(s.a, 1);
    EXPECT_EQ(s.b, 2);
    EXPECT_EQ(s.c, 3);
    EXPECT_EQ(s.d, 4);
}

TEST(SeqLockTest, LatestStoreWins) {
    SeqLock<Snapshot> lock;
    lock.store({1, 1, 1, 1});
    lock.store({2, 2, 2, 2});
    EXPECT_EQ(lock.load().a, 2);
}

TEST(SeqLockTest, VersionCountsCompletedStores) {
    SeqLock<Snapshot> lock;
    EXPECT_EQ(lock.version(), 0u);
    lock.store({1, 1, 1, 1});
    EXPECT_EQ(lock.version(), 1u);
    lock.store({2, 2, 2, 2});
    lock.store({3, 3, 3, 3});
    EXPECT_EQ(lock.version(), 3u);
}

TEST(SeqLockTest, DefaultConstructedLoadsZeros) {
    SeqLock<Snapshot> lock;
    const Snapshot s = lock.load();
    EXPECT_EQ(s.a, 0);
    EXPECT_EQ(s.d, 0);
}

TEST(SeqLockTest, NoTornSnapshotsUnderWriteStorm) {
    // Writer publishes {x, x+1, x+2, x+3}; ANY mix of two writes breaks
    // the +1/+2/+3 relation. On ARM a missing fence fails this within
    // milliseconds; x86 may need the tsan preset to notice (README §3).
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    SeqLock<Snapshot> lock;
    lock.store({0, 1, 2, 3});
    std::atomic<bool> stop{false};
    std::atomic<std::int64_t> torn{0};
    std::atomic<std::int64_t> reads{0};

    std::vector<std::jthread> readers;
    for (int r = 0; r < 3; ++r) {
        readers.emplace_back([&] {
            while (!stop.load(std::memory_order_relaxed)) {
                const Snapshot s = lock.load();
                if (s.b != s.a + 1 || s.c != s.a + 2 || s.d != s.a + 3) {
                    torn.fetch_add(1, std::memory_order_relaxed);
                }
                reads.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    std::int64_t x = 0;
    while (std::chrono::steady_clock::now() < deadline) {
        ++x;
        lock.store({x, x + 1, x + 2, x + 3});
    }
    stop.store(true);
    readers.clear();

    EXPECT_EQ(torn.load(), 0)
        << "torn snapshots observed (out of " << reads.load()
        << " reads) — a fence is missing or a seq check is skipped";
    EXPECT_GT(reads.load(), 0);
    EXPECT_EQ(lock.load().a, x) << "readers must eventually see the last "
                                   "completed store";
}

}  // namespace
