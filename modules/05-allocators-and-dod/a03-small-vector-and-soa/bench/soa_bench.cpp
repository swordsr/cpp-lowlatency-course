// Release preset. The AoS/SoA pair is the headline: same 100k orders, same
// sum, 3-6x apart — line utilization (12 useful bytes per 64-byte line)
// plus NEON vectorization of the dense SoA loop. Both read 0 instantly
// until your implementations are green. The SmallVector pair shows the
// no-allocation regime for tiny sizes.
#include "course/small_vector.hpp"
#include "course/soa.hpp"

#include <benchmark/benchmark.h>

#include <cstdint>
#include <vector>

namespace {

constexpr int kOrders = 100'000;

std::vector<course::OrderAoS> make_orders() {
    std::vector<course::OrderAoS> orders;
    orders.reserve(kOrders);
    for (int i = 0; i < kOrders; ++i) {
        orders.push_back(course::OrderAoS{(i % 1000) * 10'000LL,
                                          (i % 90) + 10, 0, {}});
    }
    return orders;
}

void BM_SumNotional_AoS(benchmark::State& state) {
    const auto orders = make_orders();
    for (auto _ : state) {
        benchmark::DoNotOptimize(course::sum_notional_aos(orders));
    }
    state.SetBytesProcessed(state.iterations() * kOrders *
                            static_cast<std::int64_t>(sizeof(course::OrderAoS)));
}
BENCHMARK(BM_SumNotional_AoS);

void BM_SumNotional_SoA(benchmark::State& state) {
    const auto cols = course::from_aos(make_orders());
    for (auto _ : state) {
        benchmark::DoNotOptimize(cols.sum_notional());
    }
    // Bytes actually needed by the computation: 8 + 4 per order.
    state.SetBytesProcessed(state.iterations() * kOrders * 12);
}
BENCHMARK(BM_SumNotional_SoA);

void BM_SmallVectorPush4(benchmark::State& state) {
    for (auto _ : state) {
        course::SmallVector<std::int64_t, 8> v;
        for (std::int64_t i = 0; i < 4; ++i) v.push_back(i);
        benchmark::DoNotOptimize(v.data());
    }
}
BENCHMARK(BM_SmallVectorPush4);

void BM_StdVectorPush4(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<std::int64_t> v;
        for (std::int64_t i = 0; i < 4; ++i) v.push_back(i);
        benchmark::DoNotOptimize(v.data());
    }
}
BENCHMARK(BM_StdVectorPush4);

}  // namespace
