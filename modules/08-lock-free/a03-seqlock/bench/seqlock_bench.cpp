// Release preset. Compare read-side cost as reader count grows:
//   * seqlock, quiet writer: ~10-30ns and FLAT across 1/2/4 readers —
//     readers share the line in S state and never write it.
//   * mutex-protected struct: readers WRITE the lock word; watch it
//     degrade as readers multiply.
//   * seqlock under a write storm: retries appear; the mean rises.
// Numbers meaningless until SeqLock is implemented (stub returns
// instantly).
#include "course/seqlock.hpp"

#include <benchmark/benchmark.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>

namespace {

struct Snapshot {
    std::int64_t a, b, c, d;
};

void BM_SeqlockRead_QuietWriter(benchmark::State& state) {
    static course::SeqLock<Snapshot> lock;
    if (state.thread_index() == 0) lock.store({1, 2, 3, 4});
    for (auto _ : state) {
        benchmark::DoNotOptimize(lock.load());
    }
}
BENCHMARK(BM_SeqlockRead_QuietWriter)->Threads(1)->Threads(2)->Threads(4)
    ->UseRealTime();

void BM_SeqlockRead_WriteStorm(benchmark::State& state) {
    static course::SeqLock<Snapshot> lock;
    static std::atomic<bool> stop{false};
    static std::thread writer;

    if (state.thread_index() == 0) {
        stop.store(false);
        writer = std::thread{[&] {
            std::int64_t x = 0;
            while (!stop.load(std::memory_order_relaxed)) {
                ++x;
                lock.store({x, x + 1, x + 2, x + 3});
            }
        }};
    }
    for (auto _ : state) {
        benchmark::DoNotOptimize(lock.load());
    }
    if (state.thread_index() == 0) {
        stop.store(true);
        writer.join();
    }
}
BENCHMARK(BM_SeqlockRead_WriteStorm)->Threads(1)->Threads(2)->Threads(4)
    ->UseRealTime();

void BM_MutexRead(benchmark::State& state) {
    static std::mutex m;
    static Snapshot snap{1, 2, 3, 4};
    for (auto _ : state) {
        std::lock_guard g{m};
        benchmark::DoNotOptimize(snap);
    }
}
BENCHMARK(BM_MutexRead)->Threads(1)->Threads(2)->Threads(4)->UseRealTime();

}  // namespace
