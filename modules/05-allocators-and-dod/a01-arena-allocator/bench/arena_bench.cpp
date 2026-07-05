// Release preset. Run with --benchmark_repetitions=10 and compare not just
// the means but the spread: the arena's numbers should be far tighter —
// determinism is half the point (README THEORY §1).
#include "course/arena.hpp"

#include <benchmark/benchmark.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace {

struct Node {
    std::int64_t payload[6];  // 48 bytes: order-record-ish
    Node* next = nullptr;
};

constexpr int kNodes = 1000;

void BM_BuildList_NewDelete(benchmark::State& state) {
    for (auto _ : state) {
        Node* head = nullptr;
        for (int i = 0; i < kNodes; ++i) {
            Node* n = new Node{};
            n->next = head;
            head = n;
        }
        benchmark::DoNotOptimize(head);
        while (head != nullptr) {
            Node* next = head->next;
            delete head;
            head = next;
        }
    }
    state.SetItemsProcessed(state.iterations() * kNodes);
}
BENCHMARK(BM_BuildList_NewDelete);

void BM_BuildList_Arena(benchmark::State& state) {
    course::Arena arena{kNodes * sizeof(Node) + 1024};
    for (auto _ : state) {
        Node* head = nullptr;
        for (int i = 0; i < kNodes; ++i) {
            Node* n = arena.create<Node>();
            n->next = head;
            head = n;
        }
        benchmark::DoNotOptimize(head);
        arena.reset();  // "free" all 1000 nodes: one store
    }
    state.SetItemsProcessed(state.iterations() * kNodes);
}
BENCHMARK(BM_BuildList_Arena);

void BM_VectorGrowth_Default(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<std::int64_t> v;
        for (int i = 0; i < kNodes; ++i) v.push_back(i);
        benchmark::DoNotOptimize(v.data());
    }
    state.SetItemsProcessed(state.iterations() * kNodes);
}
BENCHMARK(BM_VectorGrowth_Default);

void BM_VectorGrowth_Arena(benchmark::State& state) {
    // Note the arena is reset each iteration; growth garbage accumulates
    // within one iteration (abandoned old buffers) — count that as part of
    // the deal you're benchmarking.
    course::Arena arena{1 << 20};
    for (auto _ : state) {
        arena.reset();
        std::vector<std::int64_t, course::ArenaAllocator<std::int64_t>> v{
            course::ArenaAllocator<std::int64_t>{arena}};
        for (int i = 0; i < kNodes; ++i) v.push_back(i);
        benchmark::DoNotOptimize(v.data());
    }
    state.SetItemsProcessed(state.iterations() * kNodes);
}
BENCHMARK(BM_VectorGrowth_Arena);

}  // namespace
