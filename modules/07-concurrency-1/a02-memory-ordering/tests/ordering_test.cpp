// The spec for m07a02. Functional tests here; the MEMORY-MODEL half of
// the spec lives in (a) the tsan preset — a relaxed MailBox payload
// handoff is a data race and tsan says so — and (b) the litmus bench you
// run on your ARM Mac. Green means: debug + asan + TSAN.
#include "course/ordering.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

namespace {

using course::LazyBox;
using course::MailBox;
using course::RelaxedCounter;
using namespace std::chrono_literals;

// --- MailBox: functional --------------------------------------------------------

TEST(MailBox, TakeOnEmptyIsNullopt) {
    MailBox box;
    EXPECT_EQ(box.take(), std::nullopt);
}

TEST(MailBox, PostThenTakeDelivers) {
    MailBox box;
    ASSERT_TRUE(box.try_post(42));
    EXPECT_EQ(box.take(), 42);
}

TEST(MailBox, TakeConsumesThePublication) {
    MailBox box;
    ASSERT_TRUE(box.try_post(7));
    EXPECT_EQ(box.take(), 7);
    EXPECT_EQ(box.take(), std::nullopt) << "take() must consume";
}

TEST(MailBox, PostRefusesWhileFull) {
    MailBox box;
    ASSERT_TRUE(box.try_post(1));
    EXPECT_FALSE(box.try_post(2)) << "single slot: full box refuses";
    EXPECT_EQ(box.take(), 1);
    EXPECT_TRUE(box.try_post(3)) << "slot free again after take";
    EXPECT_EQ(box.take(), 3);
}

TEST(MailBox, SpscStressDeliversEverythingInOrder) {
    // Bounded by a deadline so broken implementations fail (slowly)
    // instead of hanging: worst case ~10s, well under the 60s timeout.
    constexpr std::int64_t kN = 20'000;
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    MailBox box;
    std::vector<std::int64_t> received;
    received.reserve(kN);

    std::jthread consumer{[&] {
        while (static_cast<std::int64_t>(received.size()) < kN &&
               std::chrono::steady_clock::now() < deadline) {
            if (auto v = box.take()) received.push_back(*v);
        }
    }};
    for (std::int64_t i = 0; i < kN; ++i) {
        while (!box.try_post(i)) {
            if (std::chrono::steady_clock::now() >= deadline) break;
        }
        if (std::chrono::steady_clock::now() >= deadline) break;
    }
    consumer.join();

    ASSERT_EQ(received.size(), static_cast<std::size_t>(kN))
        << "values were lost — check the orderings on BOTH sides of the "
           "handoff (Hints 1-2), then run this under the tsan preset";
    for (std::int64_t i = 0; i < kN; ++i) {
        ASSERT_EQ(received[i], i) << "value corrupted or reordered at " << i
                                  << " — the payload write isn't ordered "
                                     "against the flag";
    }
}

// --- LazyBox ----------------------------------------------------------------------

struct Expensive {
    int value;
    explicit Expensive(int v = -1) : value(v) {}
};

TEST(LazyBoxTest, CreatesOnceAndForwardsArgs) {
    LazyBox<Expensive> box;
    Expensive& a = box.get_or_create(31);
    EXPECT_EQ(box.created(), 1);
    EXPECT_EQ(a.value, 31);

    Expensive& b = box.get_or_create(99);  // already created: 99 ignored
    EXPECT_EQ(&a, &b) << "same instance forever after";
    EXPECT_EQ(box.created(), 1);
    EXPECT_EQ(b.value, 31);
}

TEST(LazyBoxTest, ConcurrentGetCreatesExactlyOnce) {
    constexpr int kThreads = 8;
    LazyBox<Expensive> box;
    std::vector<Expensive*> seen(kThreads, nullptr);
    {
        std::vector<std::jthread> threads;
        for (int t = 0; t < kThreads; ++t) {
            threads.emplace_back(
                [&box, &seen, t] { seen[t] = &box.get_or_create(5); });
        }
    }
    EXPECT_EQ(box.created(), 1)
        << "double-checked locking must construct exactly once";
    for (int t = 1; t < kThreads; ++t) {
        EXPECT_EQ(seen[t], seen[0]) << "thread " << t << " got a different "
                                       "instance";
    }
    EXPECT_EQ(seen[0]->value, 5);
}

// --- RelaxedCounter -----------------------------------------------------------------

TEST(RelaxedCounterTest, StartsAtZero) {
    const RelaxedCounter c;
    EXPECT_EQ(c.total(), 0);
}

TEST(RelaxedCounterTest, ConcurrentAddsAllLand) {
    constexpr int kThreads = 8;
    constexpr int kPerThread = 100'000;
    RelaxedCounter c;
    {
        std::vector<std::jthread> threads;
        for (int t = 0; t < kThreads; ++t) {
            threads.emplace_back([&c] {
                for (int i = 0; i < kPerThread; ++i) c.add(1);
            });
        }
    }  // all joined: the joins order the reads below
    EXPECT_EQ(c.total(), static_cast<std::int64_t>(kThreads) * kPerThread)
        << "atomicity (no lost updates) is exactly what relaxed DOES give";
}

}  // namespace
