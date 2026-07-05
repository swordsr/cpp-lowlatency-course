// Release preset. Churn = the pool's home turf: acquire/release pairs in a
// tight loop. Look for (1) the mean gap vs new/delete, (2) how little the
// pool's number moves between capacities 64 and 4096 — the LIFO head stays
// in cache regardless of pool size (README THEORY §3). Run with
// --benchmark_repetitions=10 and compare stddevs too.
#include "course/pool.hpp"

#include <benchmark/benchmark.h>

#include <cstdint>

namespace {

struct Order {
    std::int64_t price;
    std::int32_t qty;
    std::int64_t padding[5];  // ~64 bytes, like a real order record
    Order(std::int64_t p, std::int32_t q) : price(p), qty(q), padding{} {}
};

void BM_Churn_NewDelete(benchmark::State& state) {
    std::int64_t i = 0;
    for (auto _ : state) {
        Order* o = new Order{i, static_cast<std::int32_t>(i)};
        benchmark::DoNotOptimize(o);
        delete o;
        ++i;
    }
}
BENCHMARK(BM_Churn_NewDelete);

void BM_Churn_Pool(benchmark::State& state) {
    course::ObjectPool<Order> pool{static_cast<std::size_t>(state.range(0))};
    std::int64_t i = 0;
    for (auto _ : state) {
        Order* o = pool.acquire(i, static_cast<std::int32_t>(i));
        benchmark::DoNotOptimize(o);
        pool.release(o);
        ++i;
    }
}
BENCHMARK(BM_Churn_Pool)->Arg(64)->Arg(4096);

// Burst pattern: fill a quarter of the pool, drain it, repeat — closer to
// real order flow than pure churn.
void BM_Burst_Pool(benchmark::State& state) {
    constexpr int kBurst = 16;
    course::ObjectPool<Order> pool{64};
    Order* live[kBurst];
    for (auto _ : state) {
        for (int j = 0; j < kBurst; ++j) live[j] = pool.acquire(j, j);
        benchmark::DoNotOptimize(live);
        for (int j = kBurst - 1; j >= 0; --j) pool.release(live[j]);
    }
    state.SetItemsProcessed(state.iterations() * kBurst);
}
BENCHMARK(BM_Burst_Pool);

void BM_Burst_NewDelete(benchmark::State& state) {
    constexpr int kBurst = 16;
    Order* live[kBurst];
    for (auto _ : state) {
        for (int j = 0; j < kBurst; ++j) live[j] = new Order{j, j};
        benchmark::DoNotOptimize(live);
        for (int j = kBurst - 1; j >= 0; --j) delete live[j];
    }
    state.SetItemsProcessed(state.iterations() * kBurst);
}
BENCHMARK(BM_Burst_NewDelete);

}  // namespace
