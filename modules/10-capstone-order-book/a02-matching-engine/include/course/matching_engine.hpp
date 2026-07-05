#pragma once

#include <vector>

#include "course/order_book.hpp"  // your m10a01

namespace course {

/// One execution: maker rested, taker aggressed, printed at the maker's
/// level.
struct Trade {
    OrderId maker_id;
    OrderId taker_id;
    Price price;
    Qty qty;
};

/// Price-time priority matcher over YOUR OrderBook. Postcondition of
/// every submit: the book is uncrossed (best_bid < best_ask when both
/// exist).
class MatchingEngine {
public:
    /// Match while crossing, rest the remainder. Trades in execution
    /// order. Invalid input (duplicate live id, qty 0): empty vector,
    /// book untouched.
    std::vector<Trade> submit(OrderId id, Side side, Price limit, Qty qty);

    bool cancel(OrderId id);

    /// Passthrough to the book (partial cancels keep queue position —
    /// cancel+resubmit would NOT be equivalent).
    bool reduce(OrderId id, Qty by);

    const OrderBook& book() const { return book_; }

private:
    OrderBook book_;
};

}  // namespace course
