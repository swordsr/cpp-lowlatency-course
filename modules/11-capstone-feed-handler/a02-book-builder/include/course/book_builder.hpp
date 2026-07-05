#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "course/feed.hpp"        // your m11a01
#include "course/order_book.hpp"  // your m10a01

namespace course {

struct SnapshotOrder {
    OrderId id;
    Side side;
    Price price;
    Qty qty;
};

/// Mirrors the CMD feed into YOUR OrderBook, with the stale/snapshot/
/// replay recovery lifecycle (README THEORY §2). Satisfies FeedHandler.
class BookBuilder {
public:
    BookBuilder() : book_(std::make_unique<OrderBook>()) {}

    /// LIVE: decode and apply. STALE: buffer (returns true — buffering
    /// is not failure). Malformed datagram: false. A datagram whose
    /// processing FLIPS us stale (the gap carrier) is itself buffered,
    /// and its post-gap events are ignored — they return via replay
    /// (README THEORY §2).
    bool on_datagram(std::string_view dgram) {
        // TODO (Hint 2 — you'll want a private apply(), plus a
        // was-stale-before check around it for the gap carrier).
        (void)dgram;
        return false;
    }

    /// False until the first gap; snapshot recovery restores it.
    bool is_stale() const {
        // TODO
        return true;
    }

    /// THEORY §2: fresh book from the snapshot, reset the session to
    /// next_seq, drain the buffer through normal filtering; a gap
    /// mid-drain re-arms staleness with the tail still buffered.
    void load_snapshot(const std::vector<SnapshotOrder>& orders,
                       std::uint64_t next_seq) {
        // TODO (Hints 1-2).
        (void)orders;
        (void)next_seq;
    }

    const OrderBook& book() const { return *book_; }

    /// Events referencing ids the book doesn't know (possible around
    /// recovery): counted, skipped, never fatal.
    std::uint64_t dropped() const {
        // TODO
        return 0;
    }

    // --- FeedHandler surface (the session calls these; public so the
    // concept sees them) ---
    void on_add(const MdAdd& a) {
        // TODO
        (void)a;
    }
    void on_execute(const MdExecute& e) {
        // TODO
        (void)e;
    }
    void on_delete(const MdDelete& d) {
        // TODO
        (void)d;
    }
    void on_gap(const GapInfo& g) {
        // TODO
        (void)g;
    }

private:
    std::unique_ptr<OrderBook> book_;
    FeedSession session_;
    std::vector<std::string> buffered_;
    // TODO: whatever else the lifecycle needs.
};

static_assert(FeedHandler<BookBuilder>,
              "BookBuilder must satisfy the m11a01 concept");

}  // namespace course
