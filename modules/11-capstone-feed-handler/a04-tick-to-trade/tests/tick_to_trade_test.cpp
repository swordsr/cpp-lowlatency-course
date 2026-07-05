// The spec for m11a04: the histogram must be populated, plausible, and
// in agreement with the stats. Runs your entire stack over loopback.
#include "course/jthread.hpp"
#include "course/md_encode.hpp"
#include "course/pipeline.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <mutex>
#include <optional>
#include <thread>

namespace {

using namespace course;
using md::PacketBuilder;
using namespace std::chrono_literals;

/// Minimal order sink: accepts one connection, counts newline-terminated
/// lines, discards content (a03's tests already verify it).
class OrderSink {
public:
    OrderSink() : listener_(TcpListener::bind_any(0)) {}
    bool ok() const { return listener_.has_value(); }
    std::uint16_t port() const { return listener_->local_port(); }
    void run() {
        server_ = course::Jthread{[this] {
            auto conn = listener_->accept_one();
            if (!conn) return;
            char buf[512];
            while (true) {
                auto n = conn->recv_some(buf, sizeof buf);
                if (!n || *n == 0) break;
                for (std::size_t i = 0; i < *n; ++i) {
                    if (buf[i] == '\n') lines_.fetch_add(1);
                }
            }
        }};
    }
    std::uint64_t lines() const { return lines_.load(); }

private:
    std::optional<TcpListener> listener_;
    course::Jthread server_;
    std::atomic<std::uint64_t> lines_{0};
};

TEST(TickToTrade, HistogramMatchesTheOrdersSent) {
    OrderSink sink;
    ASSERT_TRUE(sink.ok());
    sink.run();

    Pipeline pipeline{PipelineConfig{0, sink.port(), 2}};
    ASSERT_TRUE(pipeline.start());

    auto feed = UdpSocket::bind_any(0);
    ASSERT_TRUE(feed.has_value());

    // 20 distinct tight-spread markets -> 20 top-of-book changes -> 20
    // orders from the toy strategy.
    std::uint64_t seq = 1;
    for (int i = 0; i < 20; ++i) {
        const std::int64_t mid = 1'000 + 2 * i;
        const auto pkt = PacketBuilder{seq}
                             .add(100 + 2 * i, 'B', 5, mid - 1)
                             .add(101 + 2 * i, 'S', 5, mid + 1)
                             .bytes();
        ASSERT_TRUE(feed->send_to_local(pipeline.md_port(), pkt));
        seq += 2;
        std::this_thread::sleep_for(2ms);  // distinct, unhurried updates
    }

    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (sink.lines() < 20 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(1ms);
    }
    pipeline.stop();

    const auto stats = pipeline.stats();
    const auto& hist = pipeline.tick_to_trade();
    EXPECT_GE(stats.orders_sent, 20u);
    EXPECT_EQ(hist.count(), static_cast<std::int64_t>(stats.orders_sent))
        << "one histogram record per order actually sent (Hint 1)";
    EXPECT_GT(hist.count(), 0);
}

TEST(TickToTrade, LatenciesArePlausible) {
    OrderSink sink;
    ASSERT_TRUE(sink.ok());
    sink.run();

    Pipeline pipeline{PipelineConfig{0, sink.port(), 2}};
    ASSERT_TRUE(pipeline.start());
    auto feed = UdpSocket::bind_any(0);
    ASSERT_TRUE(feed.has_value());

    ASSERT_TRUE(feed->send_to_local(pipeline.md_port(),
                                    PacketBuilder{1}
                                        .add(1, 'B', 5, 999)
                                        .add(2, 'S', 5, 1'001)
                                        .bytes()));
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (sink.lines() < 1 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(1ms);
    }
    pipeline.stop();

    const auto& hist = pipeline.tick_to_trade();
    ASSERT_GT(hist.count(), 0) << "no latency recorded for a sent order";
    EXPECT_GT(hist.min(), 100)
        << "sub-100ns through two threads and two sockets: your clock "
           "starts too late or stops too early (Hint 2)";
    EXPECT_LT(hist.min(), 1'000'000'000) << "over a second: wrong clock?";
    EXPECT_GE(hist.max(), hist.min());
    EXPECT_GE(hist.percentile(99), hist.percentile(50));
}

}  // namespace
