// Litmus experiments, packaged as benchmarks (release preset; do NOT run
// under tsan — the relaxed variants are the experiment, not a bug).
//
//   ./m07a02_bench --benchmark_filter=Litmus
//
// Read the "violations" counter, not the time:
//   * MP (message passing) relaxed: violations > 0 on your ARM Mac —
//     THE weak-memory moment of this course. On an x86 box: always 0
//     (TSO forbids it). Same C++, different machine, different outcomes.
//   * MP acquire/release: 0. Guaranteed. Everywhere. That's the contract.
//   * SB (store buffering) relaxed: violations > 0 on BOTH ARM and x86 —
//     the one reordering even x86 admits (store buffers).
//   * SB seq_cst: 0 — the global order seq_cst pays for.
//
// Everything here uses atomics (no UB): "relaxed" reordering is legal
// C++ behavior being OBSERVED, not a race being exploited.
#include <benchmark/benchmark.h>

#include <atomic>
#include <cstdint>
#include <thread>

namespace {

// Lockstep harness: two worker threads run one tiny "trial body" per
// round, synchronized by round number so both race within a tight window.
template <typename Body1, typename Body2, typename Check>
std::int64_t run_trials(std::int64_t trials, Body1 body1, Body2 body2,
                        Check check_and_reset) {
    std::atomic<std::int64_t> round{0};
    std::atomic<std::int64_t> done1{0}, done2{0};
    std::atomic<bool> stop{false};
    std::int64_t violations = 0;

    std::thread t1{[&] {
        std::int64_t r = 1;
        while (!stop.load(std::memory_order_acquire)) {
            if (round.load(std::memory_order_acquire) >= r) {
                body1();
                done1.store(r, std::memory_order_release);
                ++r;
            }
        }
    }};
    std::thread t2{[&] {
        std::int64_t r = 1;
        while (!stop.load(std::memory_order_acquire)) {
            if (round.load(std::memory_order_acquire) >= r) {
                body2();
                done2.store(r, std::memory_order_release);
                ++r;
            }
        }
    }};

    for (std::int64_t r = 1; r <= trials; ++r) {
        round.store(r, std::memory_order_release);
        while (done1.load(std::memory_order_acquire) < r ||
               done2.load(std::memory_order_acquire) < r) {
        }
        if (check_and_reset()) ++violations;
    }
    stop.store(true, std::memory_order_release);
    t1.join();
    t2.join();
    return violations;
}

constexpr std::int64_t kTrials = 200'000;

// --- MP: message passing -----------------------------------------------------------

void mp_litmus(benchmark::State& state, std::memory_order store_ord,
               std::memory_order load_ord) {
    static std::atomic<int> data{0};
    static std::atomic<int> flag{0};
    static std::atomic<int> seen_flag{0}, seen_data{0};

    for (auto _ : state) {
        const std::int64_t violations = run_trials(
            kTrials,
            [&, store_ord] {  // T1: publish
                data.store(1, std::memory_order_relaxed);
                flag.store(1, store_ord);
            },
            [&, load_ord] {  // T2: observe
                seen_flag.store(flag.load(load_ord),
                                std::memory_order_relaxed);
                seen_data.store(data.load(std::memory_order_relaxed),
                                std::memory_order_relaxed);
            },
            [&] {
                const bool violation = seen_flag.load() == 1 &&
                                       seen_data.load() == 0;
                data.store(0);
                flag.store(0);
                seen_flag.store(0);
                seen_data.store(0);
                return violation;
            });
        state.counters["violations"] = static_cast<double>(violations);
        state.counters["per_million"] =
            static_cast<double>(violations) * 1e6 / kTrials;
    }
}

void BM_Litmus_MP_Relaxed(benchmark::State& state) {
    mp_litmus(state, std::memory_order_relaxed, std::memory_order_relaxed);
}
void BM_Litmus_MP_AcqRel(benchmark::State& state) {
    mp_litmus(state, std::memory_order_release, std::memory_order_acquire);
}
BENCHMARK(BM_Litmus_MP_Relaxed)->Iterations(1)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_Litmus_MP_AcqRel)->Iterations(1)->Unit(benchmark::kMillisecond);

// --- SB: store buffering -------------------------------------------------------------

void sb_litmus(benchmark::State& state, std::memory_order ord) {
    static std::atomic<int> x{0}, y{0};
    static std::atomic<int> r1{0}, r2{0};

    for (auto _ : state) {
        const std::int64_t violations = run_trials(
            kTrials,
            [&, ord] {  // T1
                x.store(1, ord);
                r1.store(y.load(ord), std::memory_order_relaxed);
            },
            [&, ord] {  // T2
                y.store(1, ord);
                r2.store(x.load(ord), std::memory_order_relaxed);
            },
            [&] {
                const bool violation = r1.load() == 0 && r2.load() == 0;
                x.store(0);
                y.store(0);
                r1.store(0);
                r2.store(0);
                return violation;
            });
        state.counters["violations"] = static_cast<double>(violations);
        state.counters["per_million"] =
            static_cast<double>(violations) * 1e6 / kTrials;
    }
}

void BM_Litmus_SB_Relaxed(benchmark::State& state) {
    sb_litmus(state, std::memory_order_relaxed);
}
void BM_Litmus_SB_SeqCst(benchmark::State& state) {
    sb_litmus(state, std::memory_order_seq_cst);
}
BENCHMARK(BM_Litmus_SB_Relaxed)->Iterations(1)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_Litmus_SB_SeqCst)->Iterations(1)->Unit(benchmark::kMillisecond);

}  // namespace
