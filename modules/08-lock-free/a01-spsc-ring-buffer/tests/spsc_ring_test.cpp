// The spec for m08a01. The stress test is deadline-bounded: a broken
// ring FAILS within seconds, it never hangs. Green = debug + asan + tsan.
#include "course/jthread.hpp"
#include "course/spsc_ring.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

namespace {

using course::SpscRing;
using namespace std::chrono_literals;

TEST(SpscRing, SingleThreadFifo) {
    SpscRing<int, 8> ring;
    EXPECT_EQ(ring.capacity(), 8u);
    EXPECT_EQ(ring.size(), 0u);
    EXPECT_TRUE(ring.try_push(1));
    EXPECT_TRUE(ring.try_push(2));
    EXPECT_TRUE(ring.try_push(3));
    EXPECT_EQ(ring.size(), 3u);
    EXPECT_EQ(ring.try_pop(), 1);
    EXPECT_EQ(ring.try_pop(), 2);
    EXPECT_EQ(ring.try_pop(), 3);
    EXPECT_EQ(ring.try_pop(), std::nullopt);
    EXPECT_EQ(ring.size(), 0u);
}

TEST(SpscRing, FullMeansExactlyN) {
    SpscRing<int, 4> ring;
    for (int i = 0; i < 4; ++i) {
        EXPECT_TRUE(ring.try_push(i)) << "slot " << i << " of 4 must fit";
    }
    EXPECT_FALSE(ring.try_push(99)) << "capacity is N, not N-1 (Hint 3)";
    EXPECT_EQ(ring.try_pop(), 0);
    EXPECT_TRUE(ring.try_push(4)) << "one pop frees one slot";
}

TEST(SpscRing, WrapsAroundCleanly) {
    SpscRing<int, 8> ring;
    // 5 full laps through an 8-slot ring: the mask better be right.
    for (int lap = 0; lap < 5; ++lap) {
        for (int i = 0; i < 8; ++i) ASSERT_TRUE(ring.try_push(lap * 8 + i));
        EXPECT_FALSE(ring.try_push(-1));
        for (int i = 0; i < 8; ++i) {
            ASSERT_EQ(ring.try_pop(), lap * 8 + i) << "lap " << lap;
        }
        EXPECT_EQ(ring.try_pop(), std::nullopt);
    }
}

TEST(SpscRing, MoveOnlyPayloads) {
    SpscRing<std::unique_ptr<int>, 4> ring;
    EXPECT_TRUE(ring.try_push(std::make_unique<int>(7)));
    auto out = ring.try_pop();
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(**out, 7);
}

TEST(SpscRing, CursorsArePadded) {
    // Two cursors, each on its own line, plus storage: the object must be
    // at least 2 * kAlign big. Fails until the alignas goes in (Hint 1).
    using Ring = SpscRing<int, 8>;
    EXPECT_GE(sizeof(Ring), 2 * Ring::kAlign)
        << "unpadded cursors false-share — m07a03 showed you the bill";
}

TEST(SpscRing, SpscStressPreservesSequence) {
    constexpr std::int64_t kCount = 1'000'000;
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    SpscRing<std::int64_t, 1024> ring;

    std::vector<std::int64_t> received;
    received.reserve(kCount);

    course::Jthread consumer{[&] {
        while (static_cast<std::int64_t>(received.size()) < kCount &&
               std::chrono::steady_clock::now() < deadline) {
            if (auto v = ring.try_pop()) received.push_back(*v);
        }
    }};
    course::Jthread producer{[&] {
        for (std::int64_t i = 0; i < kCount; ++i) {
            while (!ring.try_push(i)) {
                if (std::chrono::steady_clock::now() >= deadline) return;
            }
        }
    }};
    producer.join();
    consumer.join();

    ASSERT_EQ(received.size(), static_cast<std::size_t>(kCount))
        << "values lost — on ARM this usually means a relaxed where a "
           "release/acquire belongs (Hint 2)";
    for (std::int64_t i = 0; i < kCount; ++i) {
        ASSERT_EQ(received[i], i)
            << "sequence broken at " << i
            << " — slot overwritten before the consumer finished (Hint 3), "
               "or published before it was written (Hint 2)";
    }
}

}  // namespace
