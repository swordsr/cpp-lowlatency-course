#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace course {

/// "Row store": one order, all fields together. The cold[] ballast is
/// explicit here; in real systems it's timestamps, client ids, venue
/// routing state — fields the hot loop never reads but always pays for.
struct OrderAoS {
    std::int64_t price_ticks;
    std::int32_t qty;
    std::int32_t cold_flags;
    std::int64_t cold[6];  // 48 bytes of not-your-problem — total 64 bytes
};
static_assert(sizeof(OrderAoS) == 64, "the ballast is the point");

/// Σ price·qty over all orders (int64 math, assume no overflow).
std::int64_t sum_notional_aos(const std::vector<OrderAoS>& orders);

/// "Column store": same orders, hot fields as dense parallel arrays.
/// Invariant: prices, qtys, and cold_flags always have equal length;
/// index i names the same order in every column.
class OrderColumns {
public:
    void push_back(std::int64_t price_ticks, std::int32_t qty) {
        // TODO: append to every column (cold_flags gets 0).
        (void)price_ticks;
        (void)qty;
    }

    std::size_t size() const {
        // TODO
        return 0;
    }

    /// Σ price·qty — the loop the whole assignment exists for (Hint 3).
    std::int64_t sum_notional() const {
        // TODO
        return 0;
    }

    std::int64_t price_at(std::size_t i) const { return prices_[i]; }
    std::int32_t qty_at(std::size_t i) const { return qtys_[i]; }

private:
    std::vector<std::int64_t> prices_;
    std::vector<std::int32_t> qtys_;
    std::vector<std::int32_t> cold_flags_;
};

/// The row-store -> column-store transform.
OrderColumns from_aos(const std::vector<OrderAoS>& orders);

}  // namespace course
