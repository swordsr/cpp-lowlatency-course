#pragma once

#include <cstdint>
#include <string_view>

#include "course/wire.hpp"  // your m09a03 ByteReader

namespace course {

// --- feed events (given) ------------------------------------------------------

struct MdAdd {
    std::uint64_t order_id;
    char side;  // 'B' or 'S'
    std::uint32_t qty;
    std::int64_t price;
};

struct MdExecute {
    std::uint64_t order_id;
    std::uint32_t qty;
};

struct MdDelete {
    std::uint64_t order_id;
};

struct GapInfo {
    std::uint64_t expected;
    std::uint64_t got;
};

/// Anything that can consume the feed. Static dispatch on purpose
/// (README THEORY §4).
template <typename H>
concept FeedHandler =
    requires(H h, const MdAdd& a, const MdExecute& e, const MdDelete& d,
             const GapInfo& g) {
        h.on_add(a);
        h.on_execute(e);
        h.on_delete(d);
        h.on_gap(g);
    };

/// CMD decoding + the sequencing state machine (README THEORY §2-3).
/// A fresh session adopts the first packet's sequence as its start.
class FeedSession {
public:
    /// Decode one datagram, deliver events to the handler. False (and
    /// deliver/advance NOTHING) on any malformed content. Sequencing
    /// per the state-machine table; gaps are reported via on_gap and
    /// then processed.
    template <FeedHandler H>
    bool on_datagram(std::string_view dgram, H& handler) {
        // TODO (Hints 1-3). Your m09a03 ByteReader does the byte work.
        (void)dgram;
        (void)handler;
        return false;
    }

    /// The next message sequence number this session expects.
    std::uint64_t next_expected() const {
        // TODO
        return 0;
    }

    /// Snapshot recovery hook (a02): "everything below `next` is
    /// reflected elsewhere — expect `next` next, and treat older
    /// messages as stale."
    void reset_sequence(std::uint64_t next) {
        // TODO
        (void)next;
    }

    std::uint64_t packets() const {
        // TODO: datagrams accepted (incl. heartbeats; excl. malformed).
        return 0;
    }

    std::uint64_t messages() const {
        // TODO: messages DELIVERED (post skip/stale filtering).
        return 0;
    }

    std::uint64_t gaps() const {
        // TODO: gap events reported.
        return 0;
    }

private:
    // TODO: your state. (Careful with "no packet seen yet" vs
    // "expecting sequence 0" — they are different states.)
};

}  // namespace course
