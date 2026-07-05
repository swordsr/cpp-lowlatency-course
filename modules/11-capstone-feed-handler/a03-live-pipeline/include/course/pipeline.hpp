#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <optional>
#include <thread>

#include "course/book_builder.hpp"       // m11a02 (and m11a01/m10a01 beneath)
#include "course/latency_histogram.hpp"  // m06a01 — a04's ruler
#include "course/sockets.hpp"            // m09a01
#include "course/spsc_ring.hpp"          // m08a01

namespace course {

struct PipelineConfig {
    std::uint16_t md_port = 0;       // 0 = ephemeral; read back via md_port()
    std::uint16_t gateway_port = 0;  // TCP server the tests provide
    std::int64_t max_spread_ticks = 2;
};

/// Market state flowing feed -> strategy. rx_time is stamped right
/// after recv: the tick-to-trade clock starts THERE (a04).
struct TopOfBook {
    std::int64_t bid;
    std::int64_t ask;
    std::uint64_t seq;
    std::chrono::steady_clock::time_point rx_time;
};

struct OrderRequest {
    char side;  // 'B' or 'S'
    std::int64_t price;
    std::uint32_t qty;
    std::chrono::steady_clock::time_point rx_time;  // carried through
};

/// GIVEN: the toy decision. Crossed-or-tight market -> lift the ask.
/// Deliberately dumb; the pipeline around it is the assignment.
inline std::optional<OrderRequest> toy_strategy(const TopOfBook& top,
                                                std::int64_t max_spread) {
    if (top.ask - top.bid <= max_spread) {
        return OrderRequest{'B', top.ask, 1, top.rx_time};
    }
    return std::nullopt;
}

struct PipelineStats {
    std::uint64_t packets = 0;
    std::uint64_t updates_pushed = 0;
    std::uint64_t orders_sent = 0;
    std::uint64_t ring_full_drops = 0;
};

/// Three threads, two rings, sockets at both ends (README THEORY §1).
class Pipeline {
public:
    explicit Pipeline(PipelineConfig config) : config_(config) {}
    ~Pipeline() { stop(); }
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    /// Bind UDP, connect TCP (nodelay!), launch threads. False on any
    /// socket failure.
    bool start();

    /// Unblock, join, close — in that order. Idempotent (THEORY §2,
    /// Hint 3).
    void stop();

    /// The actually-bound market-data port.
    std::uint16_t md_port() const;

    PipelineStats stats() const;

    /// Tick-to-trade: recorded in the gateway thread as (time after
    /// send_all completes) - (the order's rx_time). Implemented as part
    /// of a04 — a03's tests never look at it; a04's do.
    const LatencyHistogram& tick_to_trade() const;

private:
    // TODO: thread bodies (feed / strategy / gateway) as private
    // members, plus the state from Hint 1.

    PipelineConfig config_;
};

}  // namespace course
