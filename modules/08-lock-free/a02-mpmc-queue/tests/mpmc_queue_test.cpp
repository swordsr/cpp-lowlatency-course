// The spec for m08a02. Values in the stress tests are tagged
// (producer_id << 32 | sequence) so consumers can verify per-producer
// FIFO order — the property Vyukov's design guarantees and broken
// implementations lose first. Deadline-bounded; green = debug+asan+tsan.
#include "course/jthread.hpp"
#include "course/mpmc_queue.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace {

using course::MpmcQueue;
using namespace std::chrono_literals;

TEST(MpmcQueue, RejectsBadCapacity) {
    EXPECT_THROW(MpmcQueue<int>{3}, std::invalid_argument);
    EXPECT_THROW(MpmcQueue<int>{0}, std::invalid_argument);
    EXPECT_THROW(MpmcQueue<int>{1}, std::invalid_argument);
    EXPECT_NO_THROW(MpmcQueue<int>{8});
}

TEST(MpmcQueue, SingleThreadFifo) {
    MpmcQueue<int> q{8};
    EXPECT_EQ(q.capacity(), 8u);
    EXPECT_EQ(q.try_pop(), std::nullopt);
    EXPECT_TRUE(q.try_push(1));
    EXPECT_TRUE(q.try_push(2));
    EXPECT_EQ(q.size(), 2u);
    EXPECT_EQ(q.try_pop(), 1);
    EXPECT_EQ(q.try_pop(), 2);
    EXPECT_EQ(q.try_pop(), std::nullopt);
}

TEST(MpmcQueue, FullMeansExactlyCapacity) {
    MpmcQueue<int> q{4};
    for (int i = 0; i < 4; ++i) ASSERT_TRUE(q.try_push(i));
    EXPECT_FALSE(q.try_push(99));
    EXPECT_EQ(q.try_pop(), 0);
    EXPECT_TRUE(q.try_push(4));
}

TEST(MpmcQueue, WrapsThroughManyLaps) {
    MpmcQueue<int> q{4};
    for (int lap = 0; lap < 10; ++lap) {
        for (int i = 0; i < 4; ++i) ASSERT_TRUE(q.try_push(lap * 4 + i));
        for (int i = 0; i < 4; ++i) {
            ASSERT_EQ(q.try_pop(), lap * 4 + i) << "lap " << lap;
        }
    }
}

TEST(MpmcQueue, MoveOnlyPayloads) {
    MpmcQueue<std::unique_ptr<int>> q{4};
    ASSERT_TRUE(q.try_push(std::make_unique<int>(5)));
    auto out = q.try_pop();
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(**out, 5);
}

TEST(MpmcQueue, CursorsArePadded) {
    EXPECT_GE(sizeof(MpmcQueue<int>), 2 * MpmcQueue<int>::kAlign);
}

TEST(MpmcQueue, MpmcStressConservesAndOrdersPerProducer) {
    constexpr int kProducers = 4;
    constexpr int kConsumers = 4;
    constexpr std::int64_t kPerProducer = 50'000;
    const auto deadline = std::chrono::steady_clock::now() + 10s;

    MpmcQueue<std::int64_t> q{1024};
    std::atomic<std::int64_t> total_count{0};
    std::atomic<std::int64_t> total_sum{0};

    // Each consumer sees a SUBSEQUENCE of each producer's stream, and a
    // subsequence of an increasing sequence is increasing — so each
    // consumer checks per-producer monotonicity against its own local
    // last-seen. No cross-consumer coordination needed.
    std::atomic<bool> order_ok{true};

    std::vector<course::Jthread> consumers;
    for (int c = 0; c < kConsumers; ++c) {
        consumers.emplace_back([&] {
            std::int64_t last_seen[kProducers];
            for (auto& v : last_seen) v = -1;
            while (total_count.load(std::memory_order_relaxed) <
                       kProducers * kPerProducer &&
                   std::chrono::steady_clock::now() < deadline) {
                if (auto v = q.try_pop()) {
                    const auto producer = static_cast<int>(*v >> 32);
                    const auto seq = static_cast<std::int64_t>(*v & 0xFFFFFFFF);
                    if (producer < 0 || producer >= kProducers ||
                        seq <= last_seen[producer]) {
                        order_ok.store(false, std::memory_order_relaxed);
                    } else {
                        last_seen[producer] = seq;
                    }
                    total_sum.fetch_add(seq, std::memory_order_relaxed);
                    total_count.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    {
        std::vector<course::Jthread> producers;
        for (int p = 0; p < kProducers; ++p) {
            producers.emplace_back([&, p] {
                for (std::int64_t i = 0; i < kPerProducer; ++i) {
                    const std::int64_t tagged =
                        (static_cast<std::int64_t>(p) << 32) | i;
                    while (!q.try_push(tagged)) {
                        if (std::chrono::steady_clock::now() >= deadline)
                            return;
                    }
                }
            });
        }
    }
    consumers.clear();

    EXPECT_EQ(total_count.load(), kProducers * kPerProducer)
        << "items lost or duplicated";
    EXPECT_EQ(total_sum.load(),
              kProducers * (kPerProducer * (kPerProducer - 1) / 2))
        << "payload corruption — a consumer read a claimed-but-unwritten "
           "cell (Hint 3)";
    EXPECT_TRUE(order_ok.load())
        << "per-producer FIFO violated — cells published out of ticket "
           "order";
}

}  // namespace
