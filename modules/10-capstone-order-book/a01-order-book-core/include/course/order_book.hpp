#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace course {

using OrderId = std::uint64_t;
using Price = std::int64_t;  // fixed-point ticks (course convention)
using Qty = std::uint32_t;

enum class Side : std::uint8_t { Bid, Ask };

/// Two-ladder limit order book: storage + queries only, NO matching
/// (crossed states are legal here — a02 owns the uncrossing rule).
/// Interface fixed; internals are your design (README Hints).
class OrderBook {
public:
    OrderBook() = default;
    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;

    /// Rests an order. False (no-op): duplicate live id, or qty == 0.
    bool add(OrderId id, Side side, Price price, Qty qty);

    /// Removes entirely. False: unknown or already-dead id.
    bool cancel(OrderId id);

    /// Shrinks by `by`, KEEPING queue position; by >= remaining removes.
    /// False: unknown id or by == 0.
    bool reduce(OrderId id, Qty by);

    std::optional<Price> best_bid() const;
    std::optional<Price> best_ask() const;

    /// Aggregate resting quantity at a level (0 if none).
    Qty qty_at(Side side, Price price) const;

    /// Number of non-empty levels on a side.
    std::size_t depth(Side side) const;

    /// Live resting orders.
    std::size_t order_count() const;

    /// Oldest order id at a level — a02's matching hook.
    std::optional<OrderId> front_order(Side side, Price price) const;

    /// Remaining quantity of a live order.
    std::optional<Qty> quantity_of(OrderId id) const;

private:
    // TODO: your representation goes here (README Hints 1-2). This
    // header is YOUR file — add members, private helpers, whatever the
    // design needs. Only the public interface above is fixed.
};

}  // namespace course
