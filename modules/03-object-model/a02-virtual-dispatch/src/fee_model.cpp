// Stub state: everything compiles, the fee logic returns 0, and the base
// destructor has a bug the DeleteThroughBasePointer test will expose.
#include "course/fee_model.hpp"

namespace course {

// Defined out of line on purpose: this gives the class a home translation
// unit, so the vtable is emitted here instead of as a weak symbol in every
// TU that uses FeeModel (the Itanium ABI "key function" rule — once the
// destructor is virtual AND defined out of line, it plays that role).
FeeModel::~FeeModel() = default;

std::int64_t FeeModel::fee_for(std::int64_t /*price_ticks*/,
                               std::int64_t /*qty*/) const {
    return 0;  // TODO: compute the notional, then dispatch to fee_ticks().
}

std::int64_t FlatFee::fee_ticks(std::int64_t /*notional_ticks*/) const {
    return 0;  // TODO: flat means flat.
}

std::int64_t BasisPointsFee::fee_ticks(std::int64_t /*notional_ticks*/) const {
    return 0;  // TODO: notional * bps / 10'000, truncating integer division.
}

std::int64_t TieredFee::fee_ticks(std::int64_t /*notional_ticks*/) const {
    return 0;  // TODO: pick the highest tier whose threshold <= notional
               // (see the header comment for the below-first-threshold rule),
               // then apply its bps.
}

std::int64_t total_fees(
    const std::vector<std::unique_ptr<FeeModel>>& /*models*/,
    std::int64_t /*notional_ticks*/) {
    return 0;  // TODO: sum fee_ticks over all models.
}

}  // namespace course
