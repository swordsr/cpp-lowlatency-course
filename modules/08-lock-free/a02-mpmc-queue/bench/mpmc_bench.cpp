// Release preset. The contention story: at 1 thread per side the MUTEX
// queue often WINS (uncontended locks are ~20ns and the CPU loves them);
// by 4x4 the lock-free queue should be clearly ahead and, more
// importantly, degrade gracefully instead of collapsing. That crossover
// is the entire argument for this module. Skipped until implemented.
#include "course/mpmc_queue.hpp"

#include <benchmark/benchmark.h>

#include <cstdint>
#include <mutex>
#include <optional>
#include <queue>

namespace {

// The honest baseline: what you'd write in 5 minutes.
class MutexQueue {
public:
    explicit MutexQueue(std::size_t) {}
    bool try_push(std::int64_t v) {
        std::lock_guard g{m_};
        if (q_.size() >= 1024) return false;
        q_.push(v);
        return true;
    }
    std::optional<std::int64_t> try_pop() {
        std::lock_guard g{m_};
        if (q_.empty()) return std::nullopt;
        auto v = q_.front();
        q_.pop();
        return v;
    }

private:
    std::mutex m_;
    std::queue<std::int64_t> q_;
};

template <typename Queue>
void churn(benchmark::State& state) {
    static Queue* q = nullptr;
    if (state.thread_index() == 0) q = new Queue{1024};

    if constexpr (std::is_same_v<Queue, course::MpmcQueue<std::int64_t>>) {
        course::MpmcQueue<std::int64_t> probe{4};
        if (!probe.try_push(0)) {  // stub guard
            state.SkipWithError("MpmcQueue not implemented yet");
            return;
        }
    }

    // Even thread indexes produce, odd consume.
    std::int64_t i = 0;
    for (auto _ : state) {
        if (state.thread_index() % 2 == 0) {
            benchmark::DoNotOptimize(q->try_push(++i));
        } else {
            benchmark::DoNotOptimize(q->try_pop());
        }
    }
    state.SetItemsProcessed(state.iterations());

    if (state.thread_index() == 0) {
        delete q;
        q = nullptr;
    }
}

void BM_Mutex(benchmark::State& s) { churn<MutexQueue>(s); }
void BM_Vyukov(benchmark::State& s) {
    churn<course::MpmcQueue<std::int64_t>>(s);
}
BENCHMARK(BM_Mutex)->Threads(2)->Threads(4)->Threads(8)->UseRealTime();
BENCHMARK(BM_Vyukov)->Threads(2)->Threads(4)->Threads(8)->UseRealTime();

}  // namespace
