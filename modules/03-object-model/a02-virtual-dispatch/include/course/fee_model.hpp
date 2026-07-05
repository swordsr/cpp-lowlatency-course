// a02 — Virtual Dispatch. A small fee-model hierarchy: one abstract base,
// three concrete strategies, one free function that consumes them
// polymorphically. The constructors (plain config storage) are given; the
// fee arithmetic and — more importantly — one missing keyword are yours.
//
// Design note: to detect the virtual-destructor bug the tests define their
// own derived probe type (DtorProbe) whose destructor flips a flag. That
// keeps instrumentation out of production headers — the same reason a03's
// Tracker lived in the test file, not in unique_ptr.hpp.
#pragma once

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace course {

// Abstract base. Prices, quantities and fees are all in integer ticks —
// no floating point on the money path (Module 1 house rule).
class FeeModel {
public:
    // TODO: something is missing here — the DeleteThroughBasePointer test
    // will tell you. (Defined out of line in fee_model.cpp.)
    ~FeeModel();

    // The customization point: fee for a given notional (price * qty),
    // in ticks.
    virtual std::int64_t fee_ticks(std::int64_t notional_ticks) const = 0;

    // Non-virtual convenience: computes the notional, then dispatches to
    // fee_ticks(). Note the shape — a non-virtual public entry point over a
    // virtual customization point is a deliberate, common design.
    std::int64_t fee_for(std::int64_t price_ticks, std::int64_t qty) const;

protected:
    FeeModel() = default;
    FeeModel(const FeeModel&) = default;
    FeeModel& operator=(const FeeModel&) = default;
};

// Same fee no matter the notional (e.g. a fixed per-order exchange fee).
class FlatFee : public FeeModel {
public:
    explicit FlatFee(std::int64_t flat_fee_ticks)
        : flat_fee_ticks_(flat_fee_ticks) {}

    std::int64_t fee_ticks(std::int64_t notional_ticks) const override;

private:
    std::int64_t flat_fee_ticks_;
};

// Proportional fee: notional * bps / 10'000, integer (truncating) division.
class BasisPointsFee : public FeeModel {
public:
    explicit BasisPointsFee(int bps) : bps_(bps) {}

    std::int64_t fee_ticks(std::int64_t notional_ticks) const override;

private:
    int bps_;
};

// Volume-tiered fee. Tiers are sorted ascending by threshold; the fee uses
// the bps of the highest tier whose threshold_notional <= notional
// (exactly-at-threshold counts as reaching that tier). Notionals below the
// first threshold still use the first tier's bps — tiers[0].threshold_notional
// is effectively ignored / assumed to be 0.
class TieredFee : public FeeModel {
public:
    struct Tier {
        std::int64_t threshold_notional;
        int bps;
    };

    explicit TieredFee(std::vector<Tier> tiers) : tiers_(std::move(tiers)) {}

    std::int64_t fee_ticks(std::int64_t notional_ticks) const override;

private:
    std::vector<Tier> tiers_;
};

// Sums fee_ticks(notional_ticks) over every model in the list — the classic
// "iterate a heterogeneous collection through base pointers" pattern.
std::int64_t total_fees(const std::vector<std::unique_ptr<FeeModel>>& models,
                        std::int64_t notional_ticks);

}  // namespace course
