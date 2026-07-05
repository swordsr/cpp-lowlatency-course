// Release preset. Two experiments (README's benchmark section):
//   1. Contended counter at 4 threads: mutex vs TAS vs TTAS vs backoff.
//      TAS should be the WORST under contention — every spinner writes
//      the line every iteration. Numbers vary run to run on macOS
//      (P-core/E-core scheduling); use repetitions.
//   2. False sharing: unpadded vs padded counters at 2 threads. Expect
//      5-20x. This is the single most quotable benchmark of the course.
#include "course/spinlocks.hpp"

#include <benchmark/benchmark.h>

#include <cstdint>
#include <mutex>

namespace {

template <typename Lock>
void contended_increment(benchmark::State& state) {
    static Lock lock;
    static std::int64_t counter;
    if (state.thread_index() == 0) counter = 0;

    for (auto _ : state) {
        std::lock_guard guard{lock};
        benchmark::DoNotOptimize(++counter);
    }
}

void BM_Contended_Mutex(benchmark::State& s) {
    contended_increment<std::mutex>(s);
}
void BM_Contended_TAS(benchmark::State& s) {
    contended_increment<course::TasSpinLock>(s);
}
void BM_Contended_TTAS(benchmark::State& s) {
    contended_increment<course::TtasSpinLock>(s);
}
void BM_Contended_Backoff(benchmark::State& s) {
    contended_increment<course::BackoffSpinLock>(s);
}
BENCHMARK(BM_Contended_Mutex)->Threads(4)->UseRealTime();
BENCHMARK(BM_Contended_TAS)->Threads(4)->UseRealTime();
BENCHMARK(BM_Contended_TTAS)->Threads(4)->UseRealTime();
BENCHMARK(BM_Contended_Backoff)->Threads(4)->UseRealTime();

template <typename Counters>
void false_sharing(benchmark::State& state) {
    static Counters counters;
    for (auto _ : state) {
        if (state.thread_index() == 0) {
            counters.add_a(1);
        } else {
            counters.add_b(1);
        }
    }
    benchmark::DoNotOptimize(counters.total_a() + counters.total_b());
}

void BM_FalseSharing_Unpadded(benchmark::State& s) {
    false_sharing<course::UnpaddedCounters>(s);
}
void BM_FalseSharing_Padded(benchmark::State& s) {
    false_sharing<course::PaddedCounters>(s);
}
BENCHMARK(BM_FalseSharing_Unpadded)->Threads(2)->UseRealTime();
BENCHMARK(BM_FalseSharing_Padded)->Threads(2)->UseRealTime();

}  // namespace
