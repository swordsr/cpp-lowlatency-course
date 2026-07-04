// Run from the release preset only:
//   cmake --preset release && cmake --build --preset release --target m00a01_bench
//   ./build/release/modules/00-setup/a01-toolchain-hello/m00a01_bench
#include "course/price.hpp"

#include <benchmark/benchmark.h>

static void BM_ParseWhole(benchmark::State& state) {
    for (auto _ : state) {
        auto ticks = course::parse_price("155");
        benchmark::DoNotOptimize(ticks);
    }
}
BENCHMARK(BM_ParseWhole);

static void BM_ParseFractional(benchmark::State& state) {
    for (auto _ : state) {
        auto ticks = course::parse_price("155.1234");
        benchmark::DoNotOptimize(ticks);
    }
}
BENCHMARK(BM_ParseFractional);

static void BM_ParseReject(benchmark::State& state) {
    for (auto _ : state) {
        auto ticks = course::parse_price("155.12345");
        benchmark::DoNotOptimize(ticks);
    }
}
BENCHMARK(BM_ParseReject);

static void BM_Format(benchmark::State& state) {
    for (auto _ : state) {
        auto text = course::format_price(1'551'234);
        benchmark::DoNotOptimize(text);
    }
}
BENCHMARK(BM_Format);
