#include "course/order_book.hpp"

namespace course {

bool OrderBook::add(OrderId id, Side side, Price price, Qty qty) {
    // TODO
    (void)id;
    (void)side;
    (void)price;
    (void)qty;
    return false;
}

bool OrderBook::cancel(OrderId id) {
    // TODO
    (void)id;
    return false;
}

bool OrderBook::reduce(OrderId id, Qty by) {
    // TODO
    (void)id;
    (void)by;
    return false;
}

std::optional<Price> OrderBook::best_bid() const {
    // TODO
    return std::nullopt;
}

std::optional<Price> OrderBook::best_ask() const {
    // TODO
    return std::nullopt;
}

Qty OrderBook::qty_at(Side side, Price price) const {
    // TODO
    (void)side;
    (void)price;
    return 0;
}

std::size_t OrderBook::depth(Side side) const {
    // TODO
    (void)side;
    return 0;
}

std::size_t OrderBook::order_count() const {
    // TODO
    return 0;
}

std::optional<OrderId> OrderBook::front_order(Side side, Price price) const {
    // TODO
    (void)side;
    (void)price;
    return std::nullopt;
}

std::optional<Qty> OrderBook::quantity_of(OrderId id) const {
    // TODO
    (void)id;
    return std::nullopt;
}

}  // namespace course
