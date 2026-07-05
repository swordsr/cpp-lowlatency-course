// Release preset. The working-set sweep IS the lesson: read ns/element
// down each column as the buffer outgrows L1, then L2, then the SLC.
// Sequential barely moves (prefetch); gather steps; chase cliffs.
// Numbers are meaningless until the kernels are implemented (stubs
// return instantly).
#include "course/kernels.hpp"

#include <benchmark/benchmark.h>

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <random>
#include <vector>

namespace {

// Buffer sizes in KiB: 16KB (deep in L1) to 64MB (deep in DRAM).
void SweepSizes(benchmark::internal::Benchmark* b) {
    for (int kib : {16, 64, 256, 1024, 4096, 16384, 65536}) b->Arg(kib);
}

std::vector<std::int64_t> make_data(std::size_t n) {
    std::vector<std::int64_t> data(n);
    std::iota(data.begin(), data.end(), 1);
    return data;
}

void BM_Sequential(benchmark::State& state) {
    const std::size_t n = state.range(0) * 1024 / sizeof(std::int64_t);
    const auto data = make_data(n);
    for (auto _ : state) {
        benchmark::DoNotOptimize(course::sum_sequential(data.data(), n));
    }
    state.SetItemsProcessed(state.iterations() * n);
    state.counters["ns/elem"] = benchmark::Counter(
        static_cast<double>(state.iterations()) * static_cast<double>(n),
        benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}
BENCHMARK(BM_Sequential)->Apply(SweepSizes)->ArgName("KiB");

void BM_Gather(benchmark::State& state) {
    const std::size_t n = state.range(0) * 1024 / sizeof(std::int64_t);
    const auto data = make_data(n);
    std::vector<std::uint32_t> idx(n);
    std::iota(idx.begin(), idx.end(), 0u);
    std::mt19937 rng{42};
    std::shuffle(idx.begin(), idx.end(), rng);
    for (auto _ : state) {
        benchmark::DoNotOptimize(course::sum_gather(data.data(), idx.data(), n));
    }
    state.SetItemsProcessed(state.iterations() * n);
    state.counters["ns/elem"] = benchmark::Counter(
        static_cast<double>(state.iterations()) * static_cast<double>(n),
        benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}
BENCHMARK(BM_Gather)->Apply(SweepSizes)->ArgName("KiB");

// A chain visiting every node once in SHUFFLED order: dependent random
// access. Compare against BM_Gather at the same size — same misses, but
// serialized (README THEORY §2).
void BM_Chase(benchmark::State& state) {
    const std::size_t n = state.range(0) * 1024 / sizeof(course::Node);
    std::vector<course::Node> nodes(n);
    std::vector<std::uint32_t> order(n);
    std::iota(order.begin(), order.end(), 0u);
    std::mt19937 rng{42};
    std::shuffle(order.begin(), order.end(), rng);
    for (std::size_t i = 0; i + 1 < n; ++i) {
        nodes[order[i]] = {static_cast<std::int64_t>(i),
                           &nodes[order[i + 1]]};
    }
    nodes[order[n - 1]] = {static_cast<std::int64_t>(n - 1), nullptr};

    for (auto _ : state) {
        benchmark::DoNotOptimize(course::chase(&nodes[order[0]]));
    }
    state.SetItemsProcessed(state.iterations() * n);
    state.counters["ns/hop"] = benchmark::Counter(
        static_cast<double>(state.iterations()) * static_cast<double>(n),
        benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}
BENCHMARK(BM_Chase)->Apply(SweepSizes)->ArgName("KiB");

// Stride sweep at a fixed 32MB buffer: line utilization in action. Note
// items processed FALLS as stride grows — read ns/elem, and watch it
// converge once each element occupies its own cache line.
void BM_Strided(benchmark::State& state) {
    const std::size_t n = 32 * 1024 * 1024 / sizeof(std::int64_t);
    const auto data = make_data(n);
    const auto stride = static_cast<std::size_t>(state.range(0));
    const std::size_t touched = (n + stride - 1) / stride;
    for (auto _ : state) {
        benchmark::DoNotOptimize(course::sum_strided(data.data(), n, stride));
    }
    state.SetItemsProcessed(state.iterations() * touched);
    state.counters["ns/elem"] = benchmark::Counter(
        static_cast<double>(state.iterations()) * static_cast<double>(touched),
        benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}
BENCHMARK(BM_Strided)->Arg(1)->Arg(2)->Arg(4)->Arg(8)->Arg(16)->Arg(32)
    ->ArgName("stride");

}  // namespace
