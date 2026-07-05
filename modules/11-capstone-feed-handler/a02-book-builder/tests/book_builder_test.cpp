// The spec for m11a02. Three layers of your own code run under these
// tests: m10a01 book <- m11a01 session <- this builder.
#include "course/book_builder.hpp"
#include "course/md_encode.hpp"

#include <gtest/gtest.h>

namespace {

using namespace course;
using md::PacketBuilder;

TEST(Builder, MirrorsAddsIntoTheBook) {
    BookBuilder b;
    ASSERT_TRUE(b.on_datagram(PacketBuilder{1}
                                  .add(10, 'B', 5, 100)
                                  .add(11, 'S', 7, 102)
                                  .bytes()));
    EXPECT_FALSE(b.is_stale()) << "fresh builder is LIVE";
    EXPECT_EQ(b.book().best_bid(), 100);
    EXPECT_EQ(b.book().best_ask(), 102);
    EXPECT_EQ(b.book().qty_at(Side::Bid, 100), 5u);
    EXPECT_EQ(b.book().order_count(), 2u);
}

TEST(Builder, ExecuteReducesAndDeleteRemoves) {
    BookBuilder b;
    ASSERT_TRUE(b.on_datagram(PacketBuilder{1}
                                  .add(10, 'B', 5, 100)
                                  .add(11, 'S', 7, 102)
                                  .execute(10, 2)
                                  .del(11)
                                  .bytes()));
    EXPECT_EQ(b.book().quantity_of(10), 3u) << "execute = reduce";
    EXPECT_EQ(b.book().best_ask(), std::nullopt) << "delete = cancel";
    ASSERT_TRUE(b.on_datagram(PacketBuilder{5}.execute(10, 3).bytes()));
    EXPECT_EQ(b.book().order_count(), 0u) << "full execution removes";
}

TEST(Builder, UnknownIdsAreDroppedNotFatal) {
    BookBuilder b;
    ASSERT_TRUE(b.on_datagram(PacketBuilder{1}.del(999).bytes()));
    EXPECT_EQ(b.dropped(), 1u);
    ASSERT_TRUE(b.on_datagram(PacketBuilder{2}.execute(998, 4).bytes()));
    EXPECT_EQ(b.dropped(), 2u);
    EXPECT_EQ(b.book().order_count(), 0u);
    EXPECT_FALSE(b.is_stale()) << "dropped events are not gaps";
}

TEST(Builder, GapMakesTheBookStaleAndBuffers) {
    BookBuilder b;
    ASSERT_TRUE(b.on_datagram(PacketBuilder{1}.add(10, 'B', 5, 100).bytes()));
    // Sequence jumps 2 -> 8: gap.
    ASSERT_TRUE(b.on_datagram(PacketBuilder{8}.add(20, 'S', 5, 105).bytes()));
    EXPECT_TRUE(b.is_stale());
    EXPECT_EQ(b.book().order_count(), 1u)
        << "post-gap events must NOT touch the book (buffered instead)";
    // More arrives while stale: also buffered.
    ASSERT_TRUE(b.on_datagram(PacketBuilder{9}.del(20).bytes()));
    EXPECT_EQ(b.book().order_count(), 1u);
}

TEST(Builder, SnapshotThenReplayGoesLive) {
    BookBuilder b;
    ASSERT_TRUE(b.on_datagram(PacketBuilder{1}.add(10, 'B', 5, 100).bytes()));
    ASSERT_TRUE(b.on_datagram(PacketBuilder{8}.add(20, 'S', 5, 105).bytes()));
    ASSERT_TRUE(b.on_datagram(PacketBuilder{9}.execute(20, 2).bytes()));
    ASSERT_TRUE(b.is_stale());

    // Snapshot as of seq 8: the world contains orders 10 and 20 (the
    // exchange applied seq [2..8) that we missed, plus packet 8's add).
    b.load_snapshot({{10, Side::Bid, 100, 5}, {20, Side::Ask, 105, 5}}, 9);

    EXPECT_FALSE(b.is_stale()) << "contiguous replay must go LIVE";
    EXPECT_EQ(b.book().order_count(), 2u);
    EXPECT_EQ(b.book().quantity_of(20), 3u)
        << "the buffered execute (seq 9) must be replayed onto the snapshot";
    EXPECT_EQ(b.book().best_bid(), 100);

    // Business as usual afterwards.
    ASSERT_TRUE(b.on_datagram(PacketBuilder{10}.del(10).bytes()));
    EXPECT_EQ(b.book().best_bid(), std::nullopt);
}

TEST(Builder, ReplayFiltersWhatTheSnapshotAlreadyCovers) {
    BookBuilder b;
    ASSERT_TRUE(b.on_datagram(PacketBuilder{1}.add(10, 'B', 5, 100).bytes()));
    ASSERT_TRUE(b.on_datagram(PacketBuilder{6}.add(30, 'B', 2, 99).bytes()));
    ASSERT_TRUE(b.is_stale());
    // The buffered packet (seq 6) is BELOW the snapshot's next_seq — the
    // snapshot already reflects it. Replaying it must be a no-op (a01's
    // stale filter after reset_sequence).
    b.load_snapshot({{10, Side::Bid, 100, 5}, {30, Side::Bid, 99, 2}}, 7);
    EXPECT_FALSE(b.is_stale());
    EXPECT_EQ(b.book().qty_at(Side::Bid, 99), 2u)
        << "double-applied buffered add — the seq filter failed (Hint 3)";
    EXPECT_EQ(b.book().order_count(), 2u);
}

TEST(Builder, GapDuringReplayStaysStale) {
    BookBuilder b;
    ASSERT_TRUE(b.on_datagram(PacketBuilder{1}.add(10, 'B', 5, 100).bytes()));
    ASSERT_TRUE(b.on_datagram(PacketBuilder{8}.del(10).bytes()));   // gap 1
    ASSERT_TRUE(b.on_datagram(PacketBuilder{20}.add(40, 'S', 1, 105).bytes()));
    ASSERT_TRUE(b.is_stale());

    // Snapshot splices at 8; buffer holds seq 8 (contiguous) and seq 20
    // (ANOTHER gap). The drain must apply 8, hit the gap, and re-arm.
    b.load_snapshot({{10, Side::Bid, 100, 5}}, 8);
    EXPECT_TRUE(b.is_stale()) << "the second gap must keep us stale";
    EXPECT_EQ(b.book().order_count(), 0u) << "seq 8's delete applied";

    // Second snapshot completes recovery.
    b.load_snapshot({{40, Side::Ask, 105, 1}}, 21);
    EXPECT_FALSE(b.is_stale());
    EXPECT_EQ(b.book().best_ask(), 105);
}

TEST(Builder, MalformedWhileLiveIsRejectedNotBuffered) {
    BookBuilder b;
    ASSERT_TRUE(b.on_datagram(PacketBuilder{1}.add(10, 'B', 5, 100).bytes()));
    EXPECT_FALSE(b.on_datagram("garbage"));
    EXPECT_FALSE(b.is_stale()) << "malformed is not a gap";
    EXPECT_EQ(b.book().order_count(), 1u);
}

}  // namespace
