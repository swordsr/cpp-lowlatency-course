// The spec for m09a01. Real loopback traffic: your listener + your
// client on kernel-assigned ephemeral ports (no collisions, no sudo).
// Blocking calls are covered by the 60s ctest timeout.
#include "course/jthread.hpp"
#include "course/sockets.hpp"

#include <gtest/gtest.h>

#include <string>
#include <thread>

namespace {

using namespace course;

// --- byte order --------------------------------------------------------------

TEST(ByteOrder, KnownValues16) {
    // 0x1234 big-endian on the wire is bytes {0x12, 0x34}. On our
    // little-endian hosts that bit pattern reads back as 0x3412.
    EXPECT_EQ(to_be16(0x1234), 0x3412);
    EXPECT_EQ(from_be16(0x3412), 0x1234);
    EXPECT_EQ(to_be16(0x00FF), 0xFF00);
}

TEST(ByteOrder, KnownValues32) {
    EXPECT_EQ(to_be32(0x12345678u), 0x78563412u);
    EXPECT_EQ(from_be32(0x78563412u), 0x12345678u);
}

TEST(ByteOrder, RoundTripsEverywhere) {
    for (std::uint32_t v : {0u, 1u, 0xFFFFFFFFu, 0x80000001u, 47'000'000u}) {
        EXPECT_EQ(from_be32(to_be32(v)), v);
    }
    EXPECT_EQ(from_be16(to_be16(0xBEEF)), 0xBEEF);
}

// --- TCP -----------------------------------------------------------------------

TEST(Tcp, ListenerGetsAnEphemeralPort) {
    auto listener = TcpListener::bind_any(0);
    ASSERT_TRUE(listener.has_value()) << "bind failed";
    EXPECT_GT(listener->local_port(), 0) << "port 0 must resolve to a real "
                                            "kernel-assigned port";
}

TEST(Tcp, EchoRoundTrip) {
    auto listener = TcpListener::bind_any(0);
    ASSERT_TRUE(listener.has_value());
    const auto port = listener->local_port();
    ASSERT_GT(port, 0);

    course::Jthread server{[&] {
        auto conn = listener->accept_one();
        if (!conn) return;
        char buf[128];
        auto n = conn->recv_some(buf, sizeof buf);
        if (n && *n > 0) conn->send_all(std::string_view{buf, *n});
    }};

    auto client = TcpStream::connect_local(port);
    ASSERT_TRUE(client.has_value()) << "connect failed";
    ASSERT_TRUE(client->send_all("hello, exchange"));

    char buf[128];
    auto n = client->recv_some(buf, sizeof buf);
    ASSERT_TRUE(n.has_value());
    EXPECT_EQ((std::string_view{buf, *n}), "hello, exchange");
}

TEST(Tcp, RecvReportsEofAsZero) {
    auto listener = TcpListener::bind_any(0);
    ASSERT_TRUE(listener.has_value());
    const auto port = listener->local_port();

    course::Jthread server{[&] {
        auto conn = listener->accept_one();
        // conn destroyed immediately: orderly close.
    }};

    auto client = TcpStream::connect_local(port);
    ASSERT_TRUE(client.has_value());
    char buf[8];
    auto n = client->recv_some(buf, sizeof buf);
    ASSERT_TRUE(n.has_value()) << "EOF is not an error";
    EXPECT_EQ(*n, 0u) << "orderly EOF must read as 0";
}

TEST(Tcp, SendAllSurvivesLargePayloads) {
    // 4MB >> any socket buffer: a naive single send() can't do this.
    const std::string big(4 * 1024 * 1024, 'x');
    auto listener = TcpListener::bind_any(0);
    ASSERT_TRUE(listener.has_value());
    const auto port = listener->local_port();

    course::Jthread server{[&] {
        auto conn = listener->accept_one();
        if (!conn) return;
        std::size_t total = 0;
        char buf[65536];
        while (total < big.size()) {
            auto n = conn->recv_some(buf, sizeof buf);
            if (!n || *n == 0) break;
            total += *n;
        }
        conn->send_all(std::to_string(total));
    }};

    auto client = TcpStream::connect_local(port);
    ASSERT_TRUE(client.has_value());
    ASSERT_TRUE(client->send_all(big)) << "partial-send loop missing?";

    char buf[32];
    auto n = client->recv_some(buf, sizeof buf);
    ASSERT_TRUE(n.has_value());
    EXPECT_EQ((std::string_view{buf, *n}), std::to_string(big.size()));
}

TEST(Tcp, NodelayIsReallySet) {
    auto listener = TcpListener::bind_any(0);
    ASSERT_TRUE(listener.has_value());
    course::Jthread server{[&] { auto conn = listener->accept_one(); }};

    auto client = TcpStream::connect_local(listener->local_port());
    ASSERT_TRUE(client.has_value());
    EXPECT_FALSE(client->nodelay_enabled()) << "default is Nagle ON";
    ASSERT_TRUE(client->set_nodelay());
    EXPECT_TRUE(client->nodelay_enabled())
        << "getsockopt disagrees — Hint 3";
}

// --- UDP -----------------------------------------------------------------------

TEST(Udp, DatagramRoundTrip) {
    auto rx = UdpSocket::bind_any(0);
    ASSERT_TRUE(rx.has_value());
    auto tx = UdpSocket::bind_any(0);
    ASSERT_TRUE(tx.has_value());

    ASSERT_TRUE(tx->send_to_local(rx->local_port(), "tick"));
    auto got = rx->recv_one();
    ASSERT_TRUE(got.has_value());
    EXPECT_EQ(*got, "tick");
}

TEST(Udp, BoundariesArePreserved) {
    auto rx = UdpSocket::bind_any(0);
    ASSERT_TRUE(rx.has_value());
    auto tx = UdpSocket::bind_any(0);
    ASSERT_TRUE(tx.has_value());

    ASSERT_TRUE(tx->send_to_local(rx->local_port(), "first"));
    ASSERT_TRUE(tx->send_to_local(rx->local_port(), "second"));

    EXPECT_EQ(rx->recv_one(), "first") << "one datagram per recv — UDP "
                                          "keeps message boundaries";
    EXPECT_EQ(rx->recv_one(), "second");
}

}  // namespace
