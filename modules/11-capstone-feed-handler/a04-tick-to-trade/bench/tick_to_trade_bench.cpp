// GIVEN, complete: the load experiment (release preset). Drives your
// pipeline at increasing packet rates over loopback and reports the
// tick-to-trade percentiles at each — the data for REPORT.md §3.
//
//   ./m11a04_bench --benchmark_min_time=1x
//
// Read the counters, not the wall time: p50/p99/p999 in NANOSECONDS,
// plus drops. Expect the flat region at low rates and the knee as the
// rate climbs (README THEORY §2). Skipped until the pipeline works.
#include "course/jthread.hpp"
#include "course/md_encode.hpp"
#include "course/pipeline.hpp"

#include <benchmark/benchmark.h>

#include <chrono>
#include <optional>
#include <thread>

namespace {

using namespace course;

class NullSink {  // gateway that swallows orders as fast as TCP allows
public:
    NullSink() : listener_(TcpListener::bind_any(0)) {}
    bool ok() const { return listener_.has_value(); }
    std::uint16_t port() const { return listener_->local_port(); }
    void run() {
        server_ = course::Jthread{[this] {
            auto conn = listener_->accept_one();
            if (!conn) return;
            char buf[4096];
            while (true) {
                auto n = conn->recv_some(buf, sizeof buf);
                if (!n || *n == 0) break;
            }
        }};
    }

private:
    std::optional<TcpListener> listener_;
    course::Jthread server_;
};

void BM_TickToTrade(benchmark::State& state) {
    const long pps = state.range(0);

    for (auto _ : state) {
        NullSink sink;
        if (!sink.ok()) {
            state.SkipWithError("sink bind failed");
            return;
        }
        sink.run();
        Pipeline pipeline{PipelineConfig{0, sink.port(), 2}};
        if (!pipeline.start() || pipeline.md_port() == 0) {
            state.SkipWithError("pipeline not implemented yet");
            return;
        }
        auto feed = UdpSocket::bind_any(0);
        if (!feed) {
            state.SkipWithError("feed bind failed");
            return;
        }

        // 3 seconds of walking, always-tight market at the target rate.
        const auto interval =
            std::chrono::nanoseconds(1'000'000'000L / pps);
        auto next_send = std::chrono::steady_clock::now();
        const auto end = next_send + std::chrono::seconds(3);
        std::uint64_t seq = 1;
        std::uint64_t id = 1;
        std::int64_t mid = 10'000;
        while (std::chrono::steady_clock::now() < end) {
            mid += (seq % 3 == 0) ? 1 : ((seq % 3 == 1) ? -1 : 0);
            md::PacketBuilder pkt{seq};
            if (id > 2) pkt.del(id - 2).del(id - 1);
            pkt.add(id, 'B', 5, mid - 1).add(id + 1, 'S', 5, mid + 1);
            const auto n_msgs = (id > 2) ? 4u : 2u;
            id += 2;
            const auto bytes = pkt.bytes();
            feed->send_to_local(pipeline.md_port(), bytes);
            seq += n_msgs;
            next_send += interval;
            std::this_thread::sleep_until(next_send);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        pipeline.stop();

        const auto& hist = pipeline.tick_to_trade();
        const auto stats = pipeline.stats();
        if (hist.count() == 0) {
            state.SkipWithError("no latencies recorded — a04 step 1 missing");
            return;
        }
        state.counters["p50_ns"] = static_cast<double>(hist.percentile(50));
        state.counters["p99_ns"] = static_cast<double>(hist.percentile(99));
        state.counters["p999_ns"] = static_cast<double>(hist.percentile(99.9));
        state.counters["max_ns"] = static_cast<double>(hist.max());
        state.counters["orders"] = static_cast<double>(stats.orders_sent);
        state.counters["drops"] = static_cast<double>(stats.ring_full_drops);
    }
}
BENCHMARK(BM_TickToTrade)
    ->Arg(1'000)
    ->Arg(5'000)
    ->Arg(10'000)
    ->Arg(20'000)
    ->ArgName("pps")
    ->Iterations(1)
    ->Unit(benchmark::kMillisecond);

}  // namespace
