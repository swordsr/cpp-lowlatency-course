// The spec for m11a01. The Recorder below is the simplest possible
// FeedHandler; the PacketBuilder (md_encode.hpp, given) is the
// exchange's side of the wire.
#include "course/feed.hpp"
#include "course/md_encode.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace {

using namespace course;
using md::PacketBuilder;

struct Recorder {
    std::vector<MdAdd> adds;
    std::vector<MdExecute> execs;
    std::vector<MdDelete> dels;
    std::vector<GapInfo> gaps;

    void on_add(const MdAdd& a) { adds.push_back(a); }
    void on_execute(const MdExecute& e) { execs.push_back(e); }
    void on_delete(const MdDelete& d) { dels.push_back(d); }
    void on_gap(const GapInfo& g) { gaps.push_back(g); }
};

static_assert(FeedHandler<Recorder>, "the concept must accept Recorder");

TEST(Feed, DecodesAMixedPacketInOrder) {
    FeedSession session;
    Recorder rec;
    const auto pkt = PacketBuilder{100}
                         .add(1, 'B', 10, 1'000)
                         .execute(1, 4)
                         .del(1)
                         .bytes();
    ASSERT_TRUE(session.on_datagram(pkt, rec));

    ASSERT_EQ(rec.adds.size(), 1u);
    EXPECT_EQ(rec.adds[0].order_id, 1u);
    EXPECT_EQ(rec.adds[0].side, 'B');
    EXPECT_EQ(rec.adds[0].qty, 10u);
    EXPECT_EQ(rec.adds[0].price, 1'000);
    ASSERT_EQ(rec.execs.size(), 1u);
    EXPECT_EQ(rec.execs[0].qty, 4u);
    ASSERT_EQ(rec.dels.size(), 1u);
    EXPECT_TRUE(rec.gaps.empty());
    EXPECT_EQ(session.next_expected(), 103u);
    EXPECT_EQ(session.messages(), 3u);
    EXPECT_EQ(session.packets(), 1u);
}

TEST(Feed, FreshSessionAdoptsAnyStartingSequence) {
    FeedSession session;
    Recorder rec;
    ASSERT_TRUE(session.on_datagram(
        PacketBuilder{777'000}.add(1, 'S', 1, 5).bytes(), rec));
    EXPECT_TRUE(rec.gaps.empty()) << "joining mid-stream is not a gap";
    EXPECT_EQ(session.next_expected(), 777'001u);
}

TEST(Feed, ContiguousPacketsFlow) {
    FeedSession session;
    Recorder rec;
    ASSERT_TRUE(session.on_datagram(
        PacketBuilder{10}.add(1, 'B', 1, 100).add(2, 'B', 1, 101).bytes(),
        rec));
    ASSERT_TRUE(session.on_datagram(
        PacketBuilder{12}.del(1).bytes(), rec));
    EXPECT_TRUE(rec.gaps.empty());
    EXPECT_EQ(session.next_expected(), 13u);
    EXPECT_EQ(session.messages(), 3u);
}

TEST(Feed, GapIsReportedAndStreamContinues) {
    FeedSession session;
    Recorder rec;
    ASSERT_TRUE(session.on_datagram(PacketBuilder{10}.del(1).bytes(), rec));
    ASSERT_TRUE(session.on_datagram(PacketBuilder{15}.del(2).bytes(), rec));

    ASSERT_EQ(rec.gaps.size(), 1u);
    EXPECT_EQ(rec.gaps[0].expected, 11u);
    EXPECT_EQ(rec.gaps[0].got, 15u);
    EXPECT_EQ(rec.dels.size(), 2u) << "post-gap messages are still real "
                                      "(THEORY §2: decode anyway)";
    EXPECT_EQ(session.next_expected(), 16u);
    EXPECT_EQ(session.gaps(), 1u);
}

TEST(Feed, StaleAndDuplicatePacketsAreDropped) {
    FeedSession session;
    Recorder rec;
    const auto pkt = PacketBuilder{20}.del(1).del(2).bytes();
    ASSERT_TRUE(session.on_datagram(pkt, rec));
    ASSERT_TRUE(session.on_datagram(pkt, rec)) << "duplicate is VALID input";
    EXPECT_EQ(rec.dels.size(), 2u) << "...but delivers nothing new";
    EXPECT_EQ(session.next_expected(), 22u);

    ASSERT_TRUE(session.on_datagram(PacketBuilder{5}.del(9).bytes(), rec));
    EXPECT_EQ(rec.dels.size(), 2u) << "ancient retransmit: dropped";
    EXPECT_EQ(session.next_expected(), 22u);
}

TEST(Feed, OverlapDeliversOnlyUnseen) {
    FeedSession session;
    Recorder rec;
    ASSERT_TRUE(session.on_datagram(
        PacketBuilder{30}.del(1).del(2).bytes(), rec));  // expected: 32

    // Packet covering [31, 33): del(2) is seq 31 (seen), del(3) is 32 (new).
    ASSERT_TRUE(session.on_datagram(
        PacketBuilder{31}.del(2).del(3).bytes(), rec));
    ASSERT_EQ(rec.dels.size(), 3u) << "exactly ONE new message";
    EXPECT_EQ(rec.dels.back().order_id, 3u)
        << "the skipped one must be the ALREADY-SEEN one (Hint 2)";
    EXPECT_EQ(session.next_expected(), 33u);
    EXPECT_TRUE(rec.gaps.empty());
}

TEST(Feed, HeartbeatsKeepAliveWithoutAdvancing) {
    FeedSession session;
    Recorder rec;
    ASSERT_TRUE(session.on_datagram(PacketBuilder{40}.del(1).bytes(), rec));
    ASSERT_TRUE(session.on_datagram(md::heartbeat(41), rec));
    EXPECT_EQ(session.next_expected(), 41u);
    EXPECT_TRUE(rec.gaps.empty());
    EXPECT_EQ(session.messages(), 1u);
    EXPECT_EQ(session.packets(), 2u) << "heartbeats count as packets";
}

TEST(Feed, HeartbeatDuringGapDoesNotFakeRecovery) {
    FeedSession session;
    Recorder rec;
    ASSERT_TRUE(session.on_datagram(PacketBuilder{50}.del(1).bytes(), rec));
    // Sender is ahead of us; its heartbeat says "next is 60".
    ASSERT_TRUE(session.on_datagram(md::heartbeat(60), rec));
    EXPECT_EQ(session.next_expected(), 51u)
        << "a heartbeat DELIVERS nothing, so it may not advance you (Hint 3)";
}

TEST(Feed, MalformedPacketsAreAllOrNothing) {
    FeedSession session;
    Recorder rec;
    ASSERT_TRUE(session.on_datagram(PacketBuilder{70}.del(1).bytes(), rec));

    // Valid first message, garbage second: NOTHING may be delivered.
    const auto bad = PacketBuilder{71}.del(2).raw_message("Q???").bytes();
    EXPECT_FALSE(session.on_datagram(bad, rec));
    EXPECT_EQ(rec.dels.size(), 1u) << "all-or-nothing (THEORY §3)";
    EXPECT_EQ(session.next_expected(), 71u) << "nothing advanced";

    EXPECT_FALSE(session.on_datagram("tiny", rec)) << "truncated header";
    // Truncated mid-message: header promises 2 messages, body has 1.
    auto truncated = PacketBuilder{71}.del(2).del(3).bytes();
    truncated.resize(truncated.size() - 5);
    EXPECT_FALSE(session.on_datagram(truncated, rec));
    // Trailing bytes after the last framed message: also malformed.
    auto trailing = PacketBuilder{71}.del(2).bytes();
    trailing += "x";
    EXPECT_FALSE(session.on_datagram(trailing, rec));

    // The session must still be usable.
    ASSERT_TRUE(session.on_datagram(PacketBuilder{71}.del(2).bytes(), rec));
    EXPECT_EQ(session.next_expected(), 72u);
}

TEST(Feed, ResetSequenceRearmsTheFilter) {
    FeedSession session;
    Recorder rec;
    ASSERT_TRUE(session.on_datagram(PacketBuilder{10}.del(1).bytes(), rec));
    session.reset_sequence(500);
    EXPECT_EQ(session.next_expected(), 500u);
    ASSERT_TRUE(session.on_datagram(PacketBuilder{490}.del(2).bytes(), rec));
    EXPECT_EQ(rec.dels.size(), 1u) << "seq 490 < 500: stale after reset";
    ASSERT_TRUE(session.on_datagram(PacketBuilder{500}.del(3).bytes(), rec));
    EXPECT_EQ(rec.dels.size(), 2u);
}

}  // namespace
