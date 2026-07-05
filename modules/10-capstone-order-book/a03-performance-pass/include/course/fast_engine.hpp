#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "course/matching_engine.hpp"  // Trade, plus your a01/a02 underneath

namespace course {

/// a02's observable behavior on a production spine: flat price ladder
/// over [band_min, band_max], pooled order nodes, intrusive FIFO queues.
/// No heap allocation in submit/cancel/reduce after construction — the
/// bench counts.
class FastEngine {
public:
    FastEngine(Price band_min, Price band_max);
    ~FastEngine();
    FastEngine(const FastEngine&) = delete;
    FastEngine& operator=(const FastEngine&) = delete;

    /// Same semantics as your a02 submit; out-of-band price: empty
    /// vector, no-op.
    std::vector<Trade> submit(OrderId id, Side side, Price price, Qty qty);

    bool cancel(OrderId id);
    bool reduce(OrderId id, Qty by);

    std::optional<Price> best_bid() const;
    std::optional<Price> best_ask() const;
    Qty qty_at(Side side, Price price) const;
    std::size_t depth(Side side) const;
    std::size_t order_count() const;
    std::optional<Qty> quantity_of(OrderId id) const;

private:
    // TODO: your spine (README Hints 1-2). The ctor/dtor pair is
    // declared so your pool/ladder members can be whatever they need
    // to be — define both in the .cpp.
    Price band_min_;
    Price band_max_;
};

}  // namespace course
