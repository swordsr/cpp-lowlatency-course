#include "course/fast_engine.hpp"

namespace course {

FastEngine::FastEngine(Price band_min, Price band_max)
    : band_min_(band_min), band_max_(band_max) {
    // TODO: size the ladder, pre-size the pool.
}

FastEngine::~FastEngine() {
    // TODO (whatever your pool needs; possibly nothing).
}

std::vector<Trade> FastEngine::submit(OrderId id, Side side, Price price,
                                      Qty qty) {
    // TODO
    (void)id;
    (void)side;
    (void)price;
    (void)qty;
    return {};
}

bool FastEngine::cancel(OrderId id) {
    // TODO
    (void)id;
    return false;
}

bool FastEngine::reduce(OrderId id, Qty by) {
    // TODO
    (void)id;
    (void)by;
    return false;
}

std::optional<Price> FastEngine::best_bid() const {
    // TODO
    return std::nullopt;
}

std::optional<Price> FastEngine::best_ask() const {
    // TODO
    return std::nullopt;
}

Qty FastEngine::qty_at(Side side, Price price) const {
    // TODO
    (void)side;
    (void)price;
    return 0;
}

std::size_t FastEngine::depth(Side side) const {
    // TODO
    (void)side;
    return 0;
}

std::size_t FastEngine::order_count() const {
    // TODO
    return 0;
}

std::optional<Qty> FastEngine::quantity_of(OrderId id) const {
    // TODO
    (void)id;
    return std::nullopt;
}

}  // namespace course
