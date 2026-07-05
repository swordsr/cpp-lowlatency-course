// The spec for m11a03. Full-system tests over loopback: the test plays
// exchange (UDP feed in, TCP gateway out), your Pipeline plays firm.
// Deadline-bounded throughout; tsan-green is part of done.
#include "course/jthread.hpp"
#include "course/md_encode.hpp"
#include "course/pipeline.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace course;
using md::PacketBuilder;
using namespace std::chrono_literals;

/// Test-side exchange gateway: accepts ONE pipeline connection, collects
/// newline-terminated order lines.
class FakeGateway {
public:
    FakeGateway() : listener_(TcpListener::bind_any(0)) {}

    bool ok() const { return listener_.has_value(); }
    std::uint16_t port() const { return listener_->local_port(); }

    void run() {
        server_ = course::Jthread{[this] {
            auto conn = listener_->accept_one();
            if (!conn) return;
            char buf[512];
            while (true) {
                auto n = conn->recv_some(buf, sizeof buf);
                if (!n || *n == 0) break;  // error or orderly close
                std::lock_guard g{mu_};
                partial_.append(buf, *n);
                std::size_t nl;
                while ((nl = partial_.find('\n')) != std::string::npos) {
                    lines_.push_back(partial_.substr(0, nl));
                    partial_.erase(0, nl + 1);
                }
            }
        }};
    }

    std::vector<std::string> lines() const {
        std::lock_guard g{mu_};
        return lines_;
    }

    bool wait_for_lines(std::size_t count,
                        std::chrono::milliseconds timeout) const {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (lines().size() >= count) return true;
            std::this_thread::sleep_for(1ms);
        }
        return lines().size() >= count;
    }

private:
    std::optional<TcpListener> listener_;
    course::Jthread server_;
    mutable std::mutex mu_;
    std::string partial_;
    std::vector<std::string> lines_;
};

/// Test-side feed: sends CMD datagrams at the pipeline.
class FakeFeed {
public:
    FakeFeed() : sock_(UdpSocket::bind_any(0)) {}
    bool ok() const { return sock_.has_value(); }
    void send(std::uint16_t to_port, const std::string& dgram) {
        ASSERT_TRUE(sock_->send_to_local(to_port, dgram));
    }

private:
    std::optional<UdpSocket> sock_;
};

struct Rig {
    FakeGateway gateway;
    FakeFeed feed;
    std::optional<Pipeline> pipeline;

    bool up() {
        if (!gateway.ok() || !feed.ok()) return false;
        gateway.run();
        pipeline.emplace(
            PipelineConfig{0, gateway.port(), /*max_spread_ticks=*/2});
        if (!pipeline->start()) return false;
        return pipeline->md_port() != 0;
    }
};

TEST(Pipeline, TightSpreadTriggersAnOrder) {
    Rig rig;
    ASSERT_TRUE(rig.up()) << "start() failed — sockets or threads";

    // bid 100 / ask 101: spread 1 <= 2 -> the toy buys at 101.
    rig.feed.send(rig.pipeline->md_port(), PacketBuilder{1}
                                               .add(10, 'B', 5, 100)
                                               .add(11, 'S', 5, 101)
                                               .bytes());
    ASSERT_TRUE(rig.gateway.wait_for_lines(1, 5000ms))
        << "no order reached the gateway";
    EXPECT_EQ(rig.gateway.lines()[0], "B,101,1");

    const auto stats = rig.pipeline->stats();
    EXPECT_GE(stats.packets, 1u);
    EXPECT_GE(stats.updates_pushed, 1u);
    EXPECT_GE(stats.orders_sent, 1u);
    rig.pipeline->stop();
}

TEST(Pipeline, WideSpreadStaysQuiet) {
    Rig rig;
    ASSERT_TRUE(rig.up());

    rig.feed.send(rig.pipeline->md_port(), PacketBuilder{1}
                                               .add(10, 'B', 5, 100)
                                               .add(11, 'S', 5, 110)
                                               .bytes());
    EXPECT_FALSE(rig.gateway.wait_for_lines(1, 300ms))
        << "spread 10 > 2: the toy must not trade";
    rig.pipeline->stop();
}

TEST(Pipeline, GapHaltsTradingUntilFurtherNotice) {
    Rig rig;
    ASSERT_TRUE(rig.up());

    rig.feed.send(rig.pipeline->md_port(),
                  PacketBuilder{1}.add(10, 'B', 5, 100).bytes());
    // Sequence jump: gap. The tight spread inside must NOT trade.
    rig.feed.send(rig.pipeline->md_port(),
                  PacketBuilder{50}.add(11, 'S', 5, 101).bytes());
    EXPECT_FALSE(rig.gateway.wait_for_lines(1, 300ms))
        << "stale book, yet an order went out — THE risk bug (THEORY §3)";
    rig.pipeline->stop();
}

TEST(Pipeline, DeduplicatesUpdates) {
    Rig rig;
    ASSERT_TRUE(rig.up());

    // Same top of book three times: one push, and the strategy sees no
    // spread <= 2 anyway (checking updates_pushed, not orders).
    for (std::uint64_t seq = 1; seq <= 3; ++seq) {
        rig.feed.send(rig.pipeline->md_port(),
                      PacketBuilder{seq}
                          .add(100 + seq, 'B', 1, 90)  // deep, top unchanged
                          .bytes());
    }
    rig.feed.send(rig.pipeline->md_port(),
                  PacketBuilder{4}.add(200, 'B', 5, 100).bytes());

    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (rig.pipeline->stats().packets < 4 &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(1ms);
    }
    const auto stats = rig.pipeline->stats();
    ASSERT_EQ(stats.packets, 4u) << "all four datagrams must arrive";
    EXPECT_LE(stats.updates_pushed, 2u)
        << "only actual top-of-book changes may be pushed (Hint 2)";
    rig.pipeline->stop();
}

TEST(Pipeline, ShutdownIsCleanAndRepeatable) {
    for (int round = 0; round < 2; ++round) {
        Rig rig;
        ASSERT_TRUE(rig.up()) << "round " << round;
        rig.feed.send(rig.pipeline->md_port(),
                      PacketBuilder{1}.add(1, 'B', 1, 100).bytes());
        rig.pipeline->stop();
        rig.pipeline->stop();  // idempotent
        // Destructors run here; leaks/hangs fail via asan or timeout.
    }
    SUCCEED();
}

}  // namespace
