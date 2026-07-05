// The spec for m07a01. These tests run REAL threads. A deadlock = the
// 60s ctest timeout killing the test — that's a failure telling you
// about a lost wakeup, not a flaky test. Green must include the tsan
// preset.
#include "course/blocking_queue.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

namespace {

using course::BlockingQueue;
using namespace std::chrono_literals;

TEST(BlockingQueue, SingleThreadFifo) {
    BlockingQueue<int> q;
    EXPECT_TRUE(q.push(1));
    EXPECT_TRUE(q.push(2));
    EXPECT_TRUE(q.push(3));
    EXPECT_EQ(q.size(), 3u);
    EXPECT_EQ(q.pop(), 1);
    EXPECT_EQ(q.pop(), 2);
    EXPECT_EQ(q.pop(), 3);
    EXPECT_EQ(q.size(), 0u);
}

TEST(BlockingQueue, TryPopDoesNotBlock) {
    BlockingQueue<int> q;
    int out = -1;
    EXPECT_FALSE(q.try_pop(out));
    q.push(7);
    EXPECT_TRUE(q.try_pop(out));
    EXPECT_EQ(out, 7);
    EXPECT_FALSE(q.try_pop(out));
}

TEST(BlockingQueue, PopBlocksUntilPushArrives) {
    BlockingQueue<int> q;
    std::optional<int> got;

    std::jthread consumer{[&] { got = q.pop(); }};
    std::this_thread::sleep_for(50ms);  // let the consumer reach the wait
    EXPECT_FALSE(got.has_value()) << "pop must BLOCK on an empty queue, "
                                     "not return early";
    q.push(42);
    consumer.join();
    EXPECT_EQ(got, 42);
}

TEST(BlockingQueue, CloseWakesBlockedConsumers) {
    BlockingQueue<int> q;
    std::atomic<int> finished{0};

    std::vector<std::jthread> consumers;
    for (int i = 0; i < 3; ++i) {
        consumers.emplace_back([&] {
            EXPECT_EQ(q.pop(), std::nullopt);
            finished.fetch_add(1);
        });
    }
    std::this_thread::sleep_for(50ms);
    EXPECT_EQ(finished.load(), 0) << "consumers should be blocked";
    q.close();
    consumers.clear();  // joins all
    EXPECT_EQ(finished.load(), 3) << "close() must wake EVERY waiter";
}

TEST(BlockingQueue, CloseDrainsBeforeNullopt) {
    BlockingQueue<int> q;
    q.push(1);
    q.push(2);
    q.close();
    EXPECT_FALSE(q.push(3)) << "push after close must refuse";
    EXPECT_EQ(q.pop(), 1) << "items enqueued before close still drain";
    EXPECT_EQ(q.pop(), 2);
    EXPECT_EQ(q.pop(), std::nullopt);
    q.close();  // idempotent
    EXPECT_EQ(q.pop(), std::nullopt);
}

TEST(BlockingQueue, MpmcConservesEveryItem) {
    constexpr int kProducers = 4;
    constexpr int kConsumers = 4;
    constexpr int kPerProducer = 5'000;

    BlockingQueue<std::int64_t> q;
    std::atomic<std::int64_t> consumed_sum{0};
    std::atomic<std::int64_t> consumed_count{0};

    std::vector<std::jthread> consumers;
    for (int c = 0; c < kConsumers; ++c) {
        consumers.emplace_back([&] {
            while (auto item = q.pop()) {
                consumed_sum.fetch_add(*item);
                consumed_count.fetch_add(1);
            }
        });
    }
    {
        std::vector<std::jthread> producers;
        for (int p = 0; p < kProducers; ++p) {
            producers.emplace_back([&, p] {
                for (int i = 0; i < kPerProducer; ++i) {
                    ASSERT_TRUE(q.push(p * kPerProducer + i));
                }
            });
        }
    }  // producers joined
    q.close();
    consumers.clear();  // consumers drain and exit

    const std::int64_t n = kProducers * kPerProducer;
    EXPECT_EQ(consumed_count.load(), n) << "items lost or duplicated";
    EXPECT_EQ(consumed_sum.load(), n * (n - 1) / 2)
        << "sum mismatch: some item was corrupted, lost, or double-consumed";
}

TEST(BlockingQueue, SingleProducerSingleConsumerPreservesOrder) {
    BlockingQueue<int> q;
    std::vector<int> received;

    std::jthread consumer{[&] {
        while (auto item = q.pop()) received.push_back(*item);
    }};
    for (int i = 0; i < 1'000; ++i) ASSERT_TRUE(q.push(i));
    q.close();
    consumer.join();

    ASSERT_EQ(received.size(), 1'000u);
    for (int i = 0; i < 1'000; ++i) {
        ASSERT_EQ(received[i], i) << "FIFO order violated at " << i;
    }
}

TEST(BlockingQueue, WorksWithMoveOnlyTypes) {
    BlockingQueue<std::unique_ptr<int>> q;
    q.push(std::make_unique<int>(5));
    auto item = q.pop();
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(**item, 5);
}

}  // namespace
