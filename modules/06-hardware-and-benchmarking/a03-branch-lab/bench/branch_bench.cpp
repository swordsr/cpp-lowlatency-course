// Release preset. The 2x2 matrix: {branchy, branchless} x {sorted,
// shuffled}, threshold at the median (maximum unpredictability for the
// shuffled case). The SUM kernel shows the effect best — record your
// matrix in src/branches.cpp. Numbers are meaningless until implemented.
#include "course/branches.hpp"

#include <benchmark/benchmark.h>

#include <algorithm>
#include <cstdint>
#include <random>
#include <vector>

namespace {

constexpr std::size_t kN = 1 << 20;  // 8MB of int64: out of L1/L2 noise
constexpr std::int64_t kThreshold = kN / 2;  // the median -> 50/50 branch

std::vector<std::int64_t> make_values(bool sorted) {
    std::vector<std::int64_t> v(kN);
    for (std::size_t i = 0; i < kN; ++i) v[i] = static_cast<std::int64_t>(i);
    if (!sorted) {
        std::mt19937 rng{42};
        std::shuffle(v.begin(), v.end(), rng);
    }
    return v;
}

template <std::int64_t (*Kernel)(const std::int64_t*, std::size_t,
                                 std::int64_t)>
void run(benchmark::State& state, bool sorted) {
    const auto data = make_values(sorted);
    for (auto _ : state) {
        benchmark::DoNotOptimize(Kernel(data.data(), data.size(), kThreshold));
    }
    state.SetItemsProcessed(state.iterations() * kN);
    state.counters["ns/elem"] = benchmark::Counter(
        static_cast<double>(state.iterations()) * static_cast<double>(kN),
        benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}

void BM_SumBranchy_Sorted(benchmark::State& s) {
    run<course::sum_above_branchy>(s, true);
}
void BM_SumBranchy_Shuffled(benchmark::State& s) {
    run<course::sum_above_branchy>(s, false);
}
void BM_SumBranchless_Sorted(benchmark::State& s) {
    run<course::sum_above_branchless>(s, true);
}
void BM_SumBranchless_Shuffled(benchmark::State& s) {
    run<course::sum_above_branchless>(s, false);
}
BENCHMARK(BM_SumBranchy_Sorted);
BENCHMARK(BM_SumBranchy_Shuffled);
BENCHMARK(BM_SumBranchless_Sorted);
BENCHMARK(BM_SumBranchless_Shuffled);

void BM_CountBranchy_Shuffled(benchmark::State& s) {
    run<course::count_above_branchy>(s, false);
}
void BM_CountBranchless_Shuffled(benchmark::State& s) {
    run<course::count_above_branchless>(s, false);
}
// If these two match: the compiler already csel'd your branchy count.
// Confirm in the assembly, then appreciate that the SUM pair (heavier
// taken-side work) still shows the gap.
BENCHMARK(BM_CountBranchy_Shuffled);
BENCHMARK(BM_CountBranchless_Shuffled);

}  // namespace
