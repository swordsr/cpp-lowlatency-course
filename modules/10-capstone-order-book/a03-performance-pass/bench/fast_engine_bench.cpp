// Release preset. Three readings (README THEORY §4):
//   * BM_Baseline_A02 vs BM_FastEngine_FullMix: the headline speedup.
//     Targets: FastEngine >= 1M ops/s, >= 3x the baseline.
//   * BM_FastEngine_NoCrossMix: the "allocs" counter must be 0 — the
//     no-trade path may not touch the heap (global new is hooked below).
//   * The p50/p99/p99.9 counters (your m06a01 histogram): the prize is
//     p99.9 within ~3x of p99. A fat tail = a hidden malloc or scan.
// Skipped until FastEngine is implemented.
#include "course/fast_engine.hpp"
#include "course/latency_histogram.hpp"  // your m06a01

#include <benchmark/benchmark.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <new>
#include <random>
#include <vector>

// --- global allocation counter (the tattletale) --------------------------------
std::atomic<long> g_allocs{0};
void* operator new(std::size_t n) {
    g_allocs.fetch_add(1, std::memory_order_relaxed);
    if (void* p = std::malloc(n)) return p;
    throw std::bad_alloc{};
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

namespace {

using namespace course;

constexpr Price kBandMin = 90;
constexpr Price kBandMax = 110;

struct Op {
    int kind;  // 0 submit-passive, 1 cancel, 2 reduce, 3 submit-crossing
    OrderId id;
    Side side;
    Price price;
    Qty qty;
};

// Realistic-ish mix. `allow_cross` false => every submit rests (prices
// kept apart) so the stream never trades: the no-alloc phase.
std::vector<Op> make_mix(int n, bool allow_cross) {
    std::mt19937 rng{7};
    std::uniform_int_distribution<int> roll(0, 99);
    std::uniform_int_distribution<Qty> qty(1, 25);
    std::vector<Op> ops;
    ops.reserve(static_cast<std::size_t>(n));
    OrderId next_id = 1;
    std::vector<OrderId> live;
    for (int i = 0; i < n; ++i) {
        const int r = roll(rng);
        if (r < 55 || live.empty()) {
            const Side side = (r % 2 == 0) ? Side::Bid : Side::Ask;
            // Bids low, asks high: never crosses.
            std::uniform_int_distribution<Price> price(
                side == Side::Bid ? kBandMin + 2 : 101,
                side == Side::Bid ? 99 : kBandMax - 2);
            const OrderId id = next_id++;
            ops.push_back({0, id, side, price(rng), qty(rng)});
            live.push_back(id);
        } else if (r < 95 || !allow_cross) {
            std::uniform_int_distribution<std::size_t> pick(0, live.size() - 1);
            const std::size_t at = pick(rng);
            ops.push_back({r % 2 == 0 ? 1 : 2, live[at], Side::Bid, 0,
                           qty(rng)});
            if (r % 2 == 0) live.erase(live.begin() + static_cast<std::ptrdiff_t>(at));
        } else {
            // Aggressive order sweeping through the touch.
            const Side side = (r % 2 == 0) ? Side::Bid : Side::Ask;
            ops.push_back({3, next_id++, side,
                           side == Side::Bid ? Price{105} : Price{95},
                           qty(rng)});
        }
    }
    return ops;
}

template <typename Engine>
void drive(Engine& eng, const Op& op) {
    switch (op.kind) {
        case 1:
            benchmark::DoNotOptimize(eng.cancel(op.id));
            break;
        case 2:
            benchmark::DoNotOptimize(eng.reduce(op.id, op.qty));
            break;
        default:
            benchmark::DoNotOptimize(
                eng.submit(op.id, op.side, op.price, op.qty));
    }
}

void BM_Baseline_A02(benchmark::State& state) {
    const auto ops = make_mix(100'000, true);
    for (auto _ : state) {
        MatchingEngine eng;
        for (const auto& op : ops) drive(eng, op);
    }
    state.SetItemsProcessed(state.iterations() *
                            static_cast<std::int64_t>(ops.size()));
}
BENCHMARK(BM_Baseline_A02)->Unit(benchmark::kMillisecond);

void BM_FastEngine_FullMix(benchmark::State& state) {
    const auto ops = make_mix(100'000, true);
    {
        FastEngine probe{kBandMin, kBandMax};
        probe.submit(1, Side::Bid, 100, 1);
        if (probe.order_count() == 0) {
            state.SkipWithError("FastEngine not implemented yet");
            return;
        }
    }
    LatencyHistogram submit_hist, cancel_hist;
    for (auto _ : state) {
        FastEngine eng{kBandMin, kBandMax};
        for (const auto& op : ops) {
            const auto t0 = std::chrono::steady_clock::now();
            drive(eng, op);
            const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                std::chrono::steady_clock::now() - t0)
                                .count();
            (op.kind == 1 ? cancel_hist : submit_hist).record(ns);
        }
    }
    state.SetItemsProcessed(state.iterations() *
                            static_cast<std::int64_t>(ops.size()));
    state.counters["submit_p50_ns"] = static_cast<double>(submit_hist.percentile(50));
    state.counters["submit_p99_ns"] = static_cast<double>(submit_hist.percentile(99));
    state.counters["submit_p999_ns"] = static_cast<double>(submit_hist.percentile(99.9));
    state.counters["cancel_p99_ns"] = static_cast<double>(cancel_hist.percentile(99));
}
BENCHMARK(BM_FastEngine_FullMix)->Unit(benchmark::kMillisecond);

void BM_FastEngine_NoCrossMix(benchmark::State& state) {
    const auto ops = make_mix(100'000, false);
    {
        FastEngine probe{kBandMin, kBandMax};
        probe.submit(1, Side::Bid, 100, 1);
        if (probe.order_count() == 0) {
            state.SkipWithError("FastEngine not implemented yet");
            return;
        }
    }
    long allocs = 0;
    for (auto _ : state) {
        FastEngine eng{kBandMin, kBandMax};  // construction MAY allocate
        const long before = g_allocs.load(std::memory_order_relaxed);
        for (const auto& op : ops) drive(eng, op);
        allocs += g_allocs.load(std::memory_order_relaxed) - before;
    }
    state.SetItemsProcessed(state.iterations() *
                            static_cast<std::int64_t>(ops.size()));
    state.counters["allocs"] = static_cast<double>(allocs);  // target: 0
}
BENCHMARK(BM_FastEngine_NoCrossMix)->Unit(benchmark::kMillisecond);

}  // namespace
