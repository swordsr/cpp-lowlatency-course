// Release preset. This harness is a LESSON — run it before implementing:
//
//   * BM_ParseSum_DCE reports absurd sub-ns time: the compiler proved the
//     result unused and deleted the work. THIS NUMBER IS A LIE.
//   * BM_ParseSum_Escaped is the same loop measured honestly.
//   * BM_EmptyLoop is what "nothing" costs — your noise floor.
//   * BM_HistogramRecord is your implementation's hot path: target <=5ns.
//
// Then rerun everything with:
//   --benchmark_repetitions=10 --benchmark_report_aggregates_only=true
// and look at the cv column: that spread is your environment talking.
#include "course/latency_histogram.hpp"

#include <benchmark/benchmark.h>

#include <cstdint>

namespace {

// A small honest workload: sum of digits of a fixed string.
int digit_sum(const char* s) {
    int total = 0;
    for (; *s != '\0'; ++s) {
        if (*s >= '0' && *s <= '9') total += *s - '0';
    }
    return total;
}

void BM_ParseSum_DCE(benchmark::State& state) {
    for (auto _ : state) {
        int r = digit_sum("1551000,100,B,20260705");
        (void)r;  // "used", says the naive author. -O2 disagrees.
    }
}
BENCHMARK(BM_ParseSum_DCE);

void BM_ParseSum_Escaped(benchmark::State& state) {
    for (auto _ : state) {
        int r = digit_sum("1551000,100,B,20260705");
        benchmark::DoNotOptimize(r);  // the result now escapes
    }
}
BENCHMARK(BM_ParseSum_Escaped);

void BM_EmptyLoop(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::DoNotOptimize(state.iterations());
    }
}
BENCHMARK(BM_EmptyLoop);

void BM_HistogramRecord(benchmark::State& state) {
    course::LatencyHistogram h;
    std::int64_t fake_latency = 1;
    for (auto _ : state) {
        h.record(fake_latency);
        fake_latency = (fake_latency * 33 + 7) & 0xFFFF;  // vary the bucket
        benchmark::DoNotOptimize(h);
    }
    if (h.count() == 0) {
        state.SkipWithError("record() not implemented yet");
    }
}
BENCHMARK(BM_HistogramRecord);

void BM_HistogramPercentile(benchmark::State& state) {
    course::LatencyHistogram h;
    for (std::int64_t i = 1; i < 100'000; ++i) h.record(i & 0x3FFF);
    for (auto _ : state) {
        benchmark::DoNotOptimize(h.percentile(99.9));
    }
}
BENCHMARK(BM_HistogramPercentile);

}  // namespace
