// Release preset only. What to look for:
//   * reserve vs no-reserve: the gap is pure reallocation cost
//   * Vector vs std::vector: you should be within ~10-20%
#include "course/vector.hpp"

#include <benchmark/benchmark.h>

#include <cstdint>
#include <vector>

namespace {

constexpr int kN = 1'000'000;

void BM_PushBackNoReserve(benchmark::State& state) {
    for (auto _ : state) {
        course::Vector<std::int64_t> v;
        for (int i = 0; i < kN; ++i) v.push_back(i);
        benchmark::DoNotOptimize(v.data());
    }
    state.SetItemsProcessed(state.iterations() * kN);
}
BENCHMARK(BM_PushBackNoReserve)->Unit(benchmark::kMillisecond);

void BM_PushBackWithReserve(benchmark::State& state) {
    for (auto _ : state) {
        course::Vector<std::int64_t> v;
        v.reserve(kN);
        for (int i = 0; i < kN; ++i) v.push_back(i);
        benchmark::DoNotOptimize(v.data());
    }
    state.SetItemsProcessed(state.iterations() * kN);
}
BENCHMARK(BM_PushBackWithReserve)->Unit(benchmark::kMillisecond);

void BM_StdVectorBaseline(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<std::int64_t> v;
        for (int i = 0; i < kN; ++i) v.push_back(i);
        benchmark::DoNotOptimize(v.data());
    }
    state.SetItemsProcessed(state.iterations() * kN);
}
BENCHMARK(BM_StdVectorBaseline)->Unit(benchmark::kMillisecond);

}  // namespace
