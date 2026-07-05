// Release preset. Two numbers matter (README's benchmark section):
//   * steady-state throughput (items/s): expect 50-150M/s green, more
//     with the cached-cursor stretch;
//   * ping-pong round trip (two rings): expect ~100-300ns — this is the
//     inter-thread latency floor your Module 11 pipeline inherits.
// Stubs make both report nonsense-fast numbers; ignore until green.
#include "course/spsc_ring.hpp"

#include <benchmark/benchmark.h>

#include <atomic>
#include <cstdint>
#include <thread>

namespace {

void BM_Throughput(benchmark::State& state) {
    static course::SpscRing<std::int64_t, 4096>* ring = nullptr;
    static std::atomic<bool> stop{false};
    static std::thread consumer;

    {
        // Stub guard: an unimplemented ring would spin forever below.
        course::SpscRing<std::int64_t, 4096> probe;
        if (!probe.try_push(0)) {
            state.SkipWithError("SpscRing not implemented yet");
            return;
        }
    }

    if (state.thread_index() == 0) {
        // Single-threaded benchmark that owns a consumer thread.
        ring = new course::SpscRing<std::int64_t, 4096>{};
        stop.store(false);
        consumer = std::thread{[&] {
            while (!stop.load(std::memory_order_relaxed)) {
                benchmark::DoNotOptimize(ring->try_pop());
            }
        }};
    }

    std::int64_t i = 0;
    for (auto _ : state) {
        while (!ring->try_push(i)) {
        }
        ++i;
    }
    state.SetItemsProcessed(state.iterations());

    stop.store(true);
    consumer.join();
    delete ring;
    ring = nullptr;
}
BENCHMARK(BM_Throughput);

// Round trip: main thread pushes to `there`, echo thread pops and pushes
// to `back`, main pops. One iteration = one full round trip.
void BM_PingPong(benchmark::State& state) {
    course::SpscRing<std::int64_t, 64> there;
    course::SpscRing<std::int64_t, 64> back;
    std::atomic<bool> stop{false};

    if (!there.try_push(0)) {  // stub guard
        state.SkipWithError("SpscRing not implemented yet");
        return;
    }
    there.try_pop();

    std::thread echo{[&] {
        while (!stop.load(std::memory_order_relaxed)) {
            if (auto v = there.try_pop()) {
                while (!back.try_push(*v)) {
                }
            }
        }
    }};

    std::int64_t i = 0;
    for (auto _ : state) {
        while (!there.try_push(i)) {
        }
        std::optional<std::int64_t> out;
        do {
            out = back.try_pop();
        } while (!out.has_value());
        benchmark::DoNotOptimize(*out);
        ++i;
    }
    stop.store(true);
    echo.join();
}
BENCHMARK(BM_PingPong);

}  // namespace
