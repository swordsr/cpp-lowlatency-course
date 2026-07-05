// The Pipeline's lifecycle and thread bodies. Most solutions grow three
// private member functions (feed_main / strategy_main / gateway_main)
// launched from start() — declare them in the header's private section
// as you go; it's your file. Keep the hot paths honest: no allocation
// in the feed loop after start() (reuse the datagram buffer), no locks
// anywhere — the rings ARE the synchronization.
#include "course/pipeline.hpp"

namespace course {

bool Pipeline::start() {
    // TODO (README THEORY §1, Hint 1).
    return false;
}

void Pipeline::stop() {
    // TODO (THEORY §2, Hint 3 — unblock, join, close, in that order).
}

std::uint16_t Pipeline::md_port() const {
    // TODO
    return 0;
}

PipelineStats Pipeline::stats() const {
    // TODO: relaxed reads of your counters.
    return {};
}

const LatencyHistogram& Pipeline::tick_to_trade() const {
    // TODO (a04): return your gateway-thread histogram. The static
    // placeholder keeps a03 compiling before a04 exists.
    static const LatencyHistogram placeholder;
    return placeholder;
}

}  // namespace course
