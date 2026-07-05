// Release preset only. Four ways to dispatch the same +k over 1024 ints,
// plus the cost of BINDING each wrapper. What to look for:
//   * direct lambda: the floor — the compiler inlines it and often
//     vectorizes the whole loop; nothing else here can win.
//   * FunctionRef ≈ raw function pointer ≈ monomorphic virtual: one
//     indirect call per element, no allocation. (NOTE: until you implement
//     FunctionRef, its call stub returns 0 immediately — the number you see
//     is meaningless-fast. Re-run when the tests are green.)
//   * std::function: same call cost or slightly worse (double indirection),
//     but look at the CONSTRUCTION benchmarks — the capture below is too
//     big for its small-buffer optimization, so every bind is a heap
//     allocation. Binding a FunctionRef is two stores.
#include "course/function_ref.hpp"

#include <benchmark/benchmark.h>

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <numeric>

namespace {

constexpr int kN = 1024;

std::array<int, kN> make_input() {
    std::array<int, kN> xs{};
    std::iota(xs.begin(), xs.end(), 1);
    return xs;
}

// Capture 4 words: comfortably over libstdc++/libc++'s std::function
// small-buffer threshold, so std::function must heap-allocate. FunctionRef
// never cares — it stores a pointer regardless of capture size.
struct FatState {
    std::int64_t k, pad1, pad2, pad3;
};

// --- virtual baseline: the classic OO answer ------------------------------------

struct Transform {
    virtual ~Transform() = default;
    virtual int apply(int x) const = 0;
};

struct AddK final : Transform {
    int k;
    explicit AddK(int k_) : k(k_) {}
    int apply(int x) const override { return x + k; }
};

// Factory keeps the dynamic type opaque enough to defeat devirtualization.
std::unique_ptr<Transform> make_transform(int k) {
    return std::make_unique<AddK>(k);
}

// --- call-cost benchmarks --------------------------------------------------------

void BM_DirectLambda(benchmark::State& state) {
    const auto xs = make_input();
    const FatState s{3, 0, 0, 0};
    auto f = [s](int x) { return x + static_cast<int>(s.k); };
    for (auto _ : state) {
        int sum = 0;
        for (int x : xs) sum += f(x);
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * kN);
}
BENCHMARK(BM_DirectLambda);

void BM_FunctionRefCall(benchmark::State& state) {
    const auto xs = make_input();
    const FatState s{3, 0, 0, 0};
    auto f = [s](int x) { return x + static_cast<int>(s.k); };
    const course::FunctionRef<int(int)> ref{f};
    for (auto _ : state) {
        int sum = 0;
        for (int x : xs) sum += ref(x);
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * kN);
}
BENCHMARK(BM_FunctionRefCall);

void BM_StdFunctionCall(benchmark::State& state) {
    const auto xs = make_input();
    const FatState s{3, 0, 0, 0};
    const std::function<int(int)> fn = [s](int x) {
        return x + static_cast<int>(s.k);
    };
    for (auto _ : state) {
        int sum = 0;
        for (int x : xs) sum += fn(x);
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * kN);
}
BENCHMARK(BM_StdFunctionCall);

void BM_VirtualCall(benchmark::State& state) {
    const auto xs = make_input();
    const std::unique_ptr<Transform> t = make_transform(3);
    const Transform* tp = t.get();
    benchmark::DoNotOptimize(tp);
    for (auto _ : state) {
        int sum = 0;
        for (int x : xs) sum += tp->apply(x);
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * kN);
}
BENCHMARK(BM_VirtualCall);

// --- construction-cost benchmarks: where std::function really loses --------------

void BM_ConstructStdFunction(benchmark::State& state) {
    const FatState s{3, 0, 0, 0};
    for (auto _ : state) {
        // Capture exceeds the SBO buffer: this line heap-allocates.
        std::function<int(int)> fn = [s](int x) {
            return x + static_cast<int>(s.k);
        };
        benchmark::DoNotOptimize(fn);
    }
}
BENCHMARK(BM_ConstructStdFunction);

void BM_BindFunctionRef(benchmark::State& state) {
    const FatState s{3, 0, 0, 0};
    auto f = [s](int x) { return x + static_cast<int>(s.k); };
    for (auto _ : state) {
        // Two pointer stores, no allocation, no matter the capture size.
        course::FunctionRef<int(int)> ref{f};
        benchmark::DoNotOptimize(ref);
    }
}
BENCHMARK(BM_BindFunctionRef);

}  // namespace
