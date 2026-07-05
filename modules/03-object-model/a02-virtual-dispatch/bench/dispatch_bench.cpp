// Dispatch mechanisms head-to-head on the SAME trivial operation
// (x * 2 + 1, accumulated over 1024 values). Everything here is harness
// code — none of your fee-model classes are involved, so this builds and
// runs from day one. Build the release preset, then:
//   ./build/release/modules/03-object-model/a02-virtual-dispatch/m03a02_bench
//
// What to look for (THEORY §2):
//   - Direct:               the call inlines and mostly VANISHES — the loop
//                           may even vectorize. This is the baseline every
//                           other row is paying to abandon.
//   - VirtualMonomorphic:   every element is the same dynamic type, so the
//                           indirect branch always goes the same way — the
//                           branch target buffer predicts it. Small, steady
//                           overhead: the real cost is the LOST INLINING,
//                           not the branch.
//   - VirtualMegamorphicShuffled: four types in random order — the
//                           predictor keeps missing. Clearly worse.
//   - VirtualMegamorphicSorted: SAME objects, grouped by type — the
//                           predictor recovers almost completely. Dispatch
//                           cost is a property of the CALL SITE's history,
//                           not of the keyword `virtual`.
//   - FunctionPointer:      one indirect call, perfectly predicted — lands
//                           near monomorphic virtual.
//   - StdFunction:          type erasure: an indirect call PLUS a fatter
//                           object and less transparent callee. Worst row.
#include <benchmark/benchmark.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <random>
#include <vector>

namespace {

constexpr std::size_t kN = 1024;

std::vector<std::int64_t> make_values() {
    std::vector<std::int64_t> v(kN);
    std::mt19937_64 rng(7);
    for (auto& x : v) x = static_cast<std::int64_t>(rng() % 1'000'000);
    return v;
}

// --- (a) direct call ----------------------------------------------------------

std::int64_t transform_direct(std::int64_t x) { return x * 2 + 1; }

void BM_DirectCall(benchmark::State& state) {
    const auto values = make_values();
    for (auto _ : state) {
        std::int64_t sum = 0;
        for (std::int64_t v : values) sum += transform_direct(v);
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(kN));
}
BENCHMARK(BM_DirectCall);

// --- (b)/(c) virtual dispatch ---------------------------------------------------

// Four derived types, one behavior. Identical bodies on purpose: any timing
// difference between the virtual benchmarks is pure dispatch, not work.
struct Transform {
    virtual ~Transform() = default;
    virtual std::int64_t apply(std::int64_t x) const = 0;
};
struct T0 final : Transform {
    std::int64_t apply(std::int64_t x) const override { return x * 2 + 1; }
};
struct T1 final : Transform {
    std::int64_t apply(std::int64_t x) const override { return x * 2 + 1; }
};
struct T2 final : Transform {
    std::int64_t apply(std::int64_t x) const override { return x * 2 + 1; }
};
struct T3 final : Transform {
    std::int64_t apply(std::int64_t x) const override { return x * 2 + 1; }
};

enum class Mix { kMonomorphic, kSortedByType, kShuffled };

std::vector<std::unique_ptr<Transform>> make_transforms(Mix mix) {
    std::vector<std::unique_ptr<Transform>> out;
    out.reserve(kN);
    for (std::size_t i = 0; i < kN; ++i) {
        // Monomorphic: all T0. Otherwise: four equal blocks, grouped by type.
        const std::size_t type = (mix == Mix::kMonomorphic) ? 0 : i / (kN / 4);
        switch (type) {
            case 0: out.push_back(std::make_unique<T0>()); break;
            case 1: out.push_back(std::make_unique<T1>()); break;
            case 2: out.push_back(std::make_unique<T2>()); break;
            default: out.push_back(std::make_unique<T3>()); break;
        }
    }
    if (mix == Mix::kShuffled) {
        std::mt19937 rng(42);  // fixed seed: every run sees the same order
        std::shuffle(out.begin(), out.end(), rng);
    }
    return out;
}

void run_virtual(benchmark::State& state, Mix mix) {
    const auto values = make_values();
    auto transforms = make_transforms(mix);
    // Escape the array so the optimizer cannot prove every element's dynamic
    // type and devirtualize the loop behind our back.
    benchmark::DoNotOptimize(transforms.data());
    benchmark::ClobberMemory();
    for (auto _ : state) {
        std::int64_t sum = 0;
        for (std::size_t i = 0; i < kN; ++i) {
            sum += transforms[i]->apply(values[i]);  // load vptr, load slot, blr
        }
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(kN));
}

void BM_VirtualMonomorphic(benchmark::State& state) {
    run_virtual(state, Mix::kMonomorphic);
}
BENCHMARK(BM_VirtualMonomorphic);

void BM_VirtualMegamorphicShuffled(benchmark::State& state) {
    run_virtual(state, Mix::kShuffled);
}
BENCHMARK(BM_VirtualMegamorphicShuffled);

/*sorted*/ // Same four types, same objects — just grouped. Watch it recover.
void BM_VirtualMegamorphicSorted(benchmark::State& state) {
    run_virtual(state, Mix::kSortedByType);
}
BENCHMARK(BM_VirtualMegamorphicSorted);

// --- (d) function pointer --------------------------------------------------------

void BM_FunctionPointer(benchmark::State& state) {
    const auto values = make_values();
    std::int64_t (*fp)(std::int64_t) = transform_direct;
    // Hide the target so the compiler cannot fold the pointer back into a
    // direct (inlinable) call.
    benchmark::DoNotOptimize(fp);
    for (auto _ : state) {
        std::int64_t sum = 0;
        for (std::int64_t v : values) sum += fp(v);
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(kN));
}
BENCHMARK(BM_FunctionPointer);

// --- (e) std::function -------------------------------------------------------------

void BM_StdFunction(benchmark::State& state) {
    const auto values = make_values();
    std::function<std::int64_t(std::int64_t)> fn =
        [](std::int64_t x) { return x * 2 + 1; };
    benchmark::DoNotOptimize(fn);
    for (auto _ : state) {
        std::int64_t sum = 0;
        for (std::int64_t v : values) sum += fn(v);
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(kN));
}
BENCHMARK(BM_StdFunction);

}  // namespace
