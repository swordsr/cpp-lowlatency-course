// The spec for a02-virtual-dispatch. Three groups:
//   1. fee arithmetic through CONCRETE types (no dispatch involved),
//   2. polymorphic use through FeeModel* / unique_ptr<FeeModel>,
//   3. the virtual-destructor trap, caught by a probe type defined HERE
//      (instrumentation stays out of production headers).
// Plus two observation tests that document object layout facts.
#include "course/fee_model.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace {

using course::BasisPointsFee;
using course::FeeModel;
using course::FlatFee;
using course::TieredFee;
using course::total_fees;

// --- concrete arithmetic: FlatFee ---------------------------------------------

TEST(FlatFeeTest, IgnoresNotional) {
    const FlatFee fee{25};
    EXPECT_EQ(fee.fee_ticks(0), 25);
    EXPECT_EQ(fee.fee_ticks(1), 25);
    EXPECT_EQ(fee.fee_ticks(1'000'000'000), 25);
}

TEST(FlatFeeTest, ZeroFlatFeeIsZero) {
    const FlatFee fee{0};
    EXPECT_EQ(fee.fee_ticks(123'456), 0);
}

// --- concrete arithmetic: BasisPointsFee ---------------------------------------

TEST(BasisPointsFeeTest, WholeBasisPoints) {
    const BasisPointsFee fee{5};  // 5 bps = 0.05%
    EXPECT_EQ(fee.fee_ticks(1'000'000), 500);
    EXPECT_EQ(fee.fee_ticks(10'000), 5);
    EXPECT_EQ(fee.fee_ticks(0), 0);
}

TEST(BasisPointsFeeTest, TruncatesTowardZero) {
    const BasisPointsFee fee{1};  // 1 bp
    EXPECT_EQ(fee.fee_ticks(9'999), 0) << "integer division truncates";
    EXPECT_EQ(fee.fee_ticks(10'000), 1);
    EXPECT_EQ(fee.fee_ticks(19'999), 1);
    EXPECT_EQ(fee.fee_ticks(20'000), 2);
}

// --- concrete arithmetic: TieredFee ---------------------------------------------

TEST(TieredFeeTest, PicksTierByNotional) {
    // 10 bps up to 1M, 5 bps from 1M, 2 bps from 10M.
    const TieredFee fee{{{0, 10}, {1'000'000, 5}, {10'000'000, 2}}};
    EXPECT_EQ(fee.fee_ticks(500'000), 500);        // first tier: 10 bps
    EXPECT_EQ(fee.fee_ticks(2'000'000), 1'000);    // middle tier: 5 bps
    EXPECT_EQ(fee.fee_ticks(50'000'000), 10'000);  // top tier: 2 bps
}

TEST(TieredFeeTest, ExactlyAtThresholdUsesThatTier) {
    const TieredFee fee{{{0, 10}, {1'000'000, 5}, {10'000'000, 2}}};
    EXPECT_EQ(fee.fee_ticks(999'999), 999) << "just below: previous tier (10 bps)";
    EXPECT_EQ(fee.fee_ticks(1'000'000), 500) << "at threshold: that tier (5 bps)";
    EXPECT_EQ(fee.fee_ticks(10'000'000), 2'000) << "at threshold: top tier (2 bps)";
}

TEST(TieredFeeTest, BelowFirstThresholdUsesFirstTier) {
    // tiers[0].threshold_notional is documented as ignored / assumed 0.
    const TieredFee fee{{{500'000, 10}, {1'000'000, 5}}};
    EXPECT_EQ(fee.fee_ticks(100'000), 100)
        << "below the first threshold still means the first tier's bps";
}

TEST(TieredFeeTest, SingleTierBehavesLikeBasisPoints) {
    const TieredFee fee{{{0, 7}}};
    EXPECT_EQ(fee.fee_ticks(1'000'000), 700);
}

// --- the non-virtual entry point --------------------------------------------------

TEST(FeeForTest, ComputesNotionalThenDispatches) {
    const BasisPointsFee fee{5};
    // price 2'000 * qty 500 = notional 1'000'000 -> 500 ticks.
    EXPECT_EQ(fee.fee_for(2'000, 500), 500);
}

TEST(FeeForTest, DispatchesVirtuallyEvenThroughBaseReference) {
    // fee_for is non-virtual, but it must reach the DERIVED fee_ticks.
    const FlatFee flat{42};
    const FeeModel& base = flat;
    EXPECT_EQ(base.fee_for(1'000'000, 1'000), 42)
        << "fee_for called through a base reference must still dispatch to "
           "the dynamic type's fee_ticks";
}

// --- polymorphic use ---------------------------------------------------------------

TEST(PolymorphismTest, CallThroughBasePointer) {
    const std::unique_ptr<FeeModel> fee = std::make_unique<BasisPointsFee>(10);
    EXPECT_EQ(fee->fee_ticks(1'000'000), 1'000);
}

TEST(PolymorphismTest, TotalFeesSumsMixedModels) {
    std::vector<std::unique_ptr<FeeModel>> models;
    models.push_back(std::make_unique<FlatFee>(100));
    models.push_back(std::make_unique<BasisPointsFee>(5));
    models.push_back(std::make_unique<TieredFee>(
        std::vector<TieredFee::Tier>{{0, 10}, {1'000'000, 2}}));

    // notional 1'000'000: 100 (flat) + 500 (5 bps) + 200 (2 bps tier) = 800.
    EXPECT_EQ(total_fees(models, 1'000'000), 800);
}

TEST(PolymorphismTest, TotalFeesOverEmptyListIsZero) {
    const std::vector<std::unique_ptr<FeeModel>> models;
    EXPECT_EQ(total_fees(models, 1'000'000), 0);
}

// --- THE virtual-destructor test ------------------------------------------------------

// A probe whose destructor flips a flag. If deleting through FeeModel* runs
// only the BASE destructor, the flag stays 0 and we know the derived
// destructor was skipped.
struct DtorProbe : FeeModel {
    int* flag_;
    explicit DtorProbe(int* f) : flag_(f) {}
    ~DtorProbe() { *flag_ = 1; }
    std::int64_t fee_ticks(std::int64_t) const override { return 0; }
};

TEST(DestructorTest, DeleteThroughBasePointer) {
    // NOTE: with the shipped non-virtual base destructor this delete is
    // formally UNDEFINED BEHAVIOR ([expr.delete]/3) — in practice compilers
    // just skip the derived destructor, which is what this test detects.
    // asan/ubsan may flag or abort here instead; that is the SAME bug
    // reported a different way, and equally instructive. Fix the base class
    // and both symptoms disappear.
    int flag = 0;
    {
        std::unique_ptr<FeeModel> p = std::make_unique<DtorProbe>(&flag);
        p.reset();  // delete through FeeModel*
    }
    EXPECT_EQ(flag, 1)
        << "derived destructor never ran — deleting through a base pointer "
           "without a virtual destructor is UB";
}

// --- layout observation tests ----------------------------------------------------------

// These pass from day one. They are not part of the spec you implement —
// they DOCUMENT what THEORY §1 claims: the first virtual function costs one
// pointer of object size (the vptr).
struct NonVirt {
    std::int64_t x;
};
struct Virt {
    std::int64_t x;
    virtual ~Virt() = default;
};

TEST(LayoutObservation, PlainStructIsJustItsMembers) {
    EXPECT_EQ(sizeof(NonVirt), 8u);
}

TEST(LayoutObservation, VirtualFunctionAddsOnePointer) {
    EXPECT_EQ(sizeof(Virt), 16u) << "8 bytes of data + 8 bytes of vptr";
}

}  // namespace
