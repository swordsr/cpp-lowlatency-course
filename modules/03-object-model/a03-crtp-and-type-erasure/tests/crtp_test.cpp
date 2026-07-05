// The spec for a03-crtp-and-type-erasure. Two halves:
//   * Comparisons<D>: the two given primitives (==, <) pass from day one,
//     and so does != (C++20 rewrites it from == — see comparisons.hpp);
//     every test touching >, <=, >= is red until the mixin is written.
//   * FunctionRef: layout/triviality hold in the stub state (the members
//     are given); everything that actually CALLS through it is red.
#include "course/comparisons.hpp"
#include "course/function_ref.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <type_traits>

namespace {

using course::FunctionRef;
using course::PriceLevel;

// ===== Comparisons ==============================================================

// --- given primitives (green from day one) --------------------------------------

TEST(Comparisons, GivenPrimitivesOrderByPriceThenQty) {
    const PriceLevel a{{}, 100'50, 10};
    const PriceLevel b{{}, 100'50, 25};
    const PriceLevel c{{}, 101'00, 5};

    EXPECT_TRUE(a == a);
    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a < b) << "same price: lower qty orders first";
    EXPECT_TRUE(b < c) << "price dominates qty";
    EXPECT_FALSE(c < a);
}

TEST(Comparisons, NotEqualComesFreeInCpp20) {
    // Observation test — green from day one, and the mixin plays no part in
    // it: C++20 rewrites `a != b` as `!(a == b)`. The mixin deliberately
    // defines no operator!= (an inherited one would lose overload
    // resolution to the rewrite anyway — note in comparisons.hpp).
    const PriceLevel a{{}, 100'50, 10};
    const PriceLevel b{{}, 100'50, 25};

    EXPECT_TRUE(a != b);
    EXPECT_FALSE(a != a) << "an object must not differ from itself";
}

// --- synthesized operators (red until the mixin is implemented) -----------------

TEST(Comparisons, GreaterThan) {
    const PriceLevel a{{}, 100'50, 10};
    const PriceLevel b{{}, 101'00, 5};

    EXPECT_TRUE(b > a);
    EXPECT_FALSE(a > b);
    EXPECT_FALSE(a > a) << "> must be irreflexive";
}

TEST(Comparisons, LessOrEqual) {
    const PriceLevel a{{}, 100'50, 10};
    const PriceLevel b{{}, 101'00, 5};

    EXPECT_TRUE(a <= b);
    EXPECT_TRUE(a <= a) << "boundary: equal operands satisfy <=";
    EXPECT_FALSE(b <= a);
}

TEST(Comparisons, GreaterOrEqual) {
    const PriceLevel a{{}, 100'50, 10};
    const PriceLevel b{{}, 101'00, 5};

    EXPECT_TRUE(b >= a);
    EXPECT_TRUE(a >= a) << "boundary: equal operands satisfy >=";
    EXPECT_FALSE(a >= b);
}

// --- the mixin must be generic: a second, unrelated instantiation ---------------

struct Version : course::Comparisons<Version> {
    int major_ver = 0;
    int minor_ver = 0;

    bool operator==(const Version& rhs) const {
        return major_ver == rhs.major_ver && minor_ver == rhs.minor_ver;
    }
    bool operator<(const Version& rhs) const {
        if (major_ver != rhs.major_ver) return major_ver < rhs.major_ver;
        return minor_ver < rhs.minor_ver;
    }
};

TEST(Comparisons, WorksForASecondInstantiation) {
    const Version v1{{}, 1, 9};
    const Version v2{{}, 2, 0};

    EXPECT_TRUE(v1 != v2);
    EXPECT_TRUE(v2 > v1);
    EXPECT_TRUE(v1 <= v2);
    EXPECT_TRUE(v1 <= v1);
    EXPECT_TRUE(v2 >= v1);
    EXPECT_FALSE(v1 >= v2);
}

// ===== FunctionRef ==============================================================

// --- layout contract (holds even in the stub state — the members are given) -----

static_assert(std::is_trivially_copyable_v<FunctionRef<int(int)>>,
              "FunctionRef must be passable in registers — no ownership, "
              "no destructor, nothing to run on copy");

TEST(FunctionRefLayout, TwoWordsExactly) {
    EXPECT_EQ(sizeof(FunctionRef<int(int)>), 2 * sizeof(void*))
        << "one pointer to the callable + one pointer to the trampoline";
}

TEST(FunctionRefLayout, DefaultConstructedIsEmpty) {
    const FunctionRef<int(int)> f;
    EXPECT_FALSE(f);
}

// --- binding and calling (red until implemented) ---------------------------------

TEST(FunctionRefCall, BindingMakesItTruthy) {
    auto id = [](int x) { return x; };
    const FunctionRef<int(int)> f{id};
    EXPECT_TRUE(f) << "constructor must install the trampoline";
}

TEST(FunctionRefCall, CapturingLambdaSeesItsCaptures) {
    const int base = 40;
    auto add_base = [base](int x) { return base + x; };

    const FunctionRef<int(int)> f{add_base};
    EXPECT_EQ(f(2), 42);
    EXPECT_EQ(f(-40), 0);
}

int triple(int x) { return 3 * x; }

TEST(FunctionRefCall, WorksWithAFunctionPointer) {
    int (*fp)(int) = triple;  // bind to the pointer variable (see Hint 6)
    const FunctionRef<int(int)> f{fp};
    EXPECT_EQ(f(7), 21);
}

struct Counter {
    int calls = 0;
    int operator()(int x) {
        ++calls;
        return x;
    }
};

TEST(FunctionRefCall, ReferencesTheFunctorItDoesNotCopyIt) {
    Counter c;
    const FunctionRef<int(int)> f{c};

    f(1);
    f(2);
    EXPECT_EQ(c.calls, 2) << "mutations through the ref must be visible in "
                             "the ORIGINAL functor — FunctionRef must not "
                             "copy it (that's std::function's job)";
}

TEST(FunctionRefCall, VoidReturnWorks) {
    int hits = 0;
    auto bump = [&hits] { ++hits; };

    const FunctionRef<void()> f{bump};
    f();
    f();
    EXPECT_EQ(hits, 2);
}

// A FunctionRef parameter is the low-latency way to accept "any callback":
// no template in the interface, no allocation, fits in two registers.
int apply_twice(FunctionRef<int(int)> f, int x) { return f(x) + f(x); }

TEST(FunctionRefCall, PassesByValueIntoAHelper) {
    const int step = 10;
    auto add_step = [step](int x) { return x + step; };

    EXPECT_EQ(apply_twice(add_step, 1), 22);  // (1+10) + (1+10)
}

}  // namespace
