# a01 — Generic Fixed-Point: A Price Whose Scale Is a Type

**Estimated effort:** 5–7 hours.

## Learning objectives

- Write a class template with a **non-type template parameter** and use the
  type system to make unit-mixing bugs uncompilable.
- Write member and free function templates; know where deduction works and
  where arguments must be spelled.
- Use the defaulted spaceship operator (`<=>`) — and know what it expands
  to.
- Do exact cross-scale integer conversion with explicit truncation rules.

## THEORY

### 1. Non-type template parameters: values in the type

Module 0's prices hardcoded scale 10⁴. Real venues disagree: US equities
quote in 10⁻⁴ dollars, but options, futures, FX all differ — and the most
expensive class of bugs in trading history is arithmetic that mixes units
that "are both just int64". The C++ answer is to move the scale *into the
type*:

```cpp
template <std::int64_t Scale>
class Fixed { std::int64_t ticks_; ... };

using Price   = Fixed<10'000>;   // 4 decimal places
using FxPrice = Fixed<100'000>;  // 5 — a DIFFERENT TYPE
```

`Fixed<10'000>` and `Fixed<100'000>` are unrelated types: adding them is a
compile error, not a Friday-night incident. The scale costs zero bytes —
it lives in the type, not the object (`sizeof(Fixed<S>) == 8`), and every
use of it compiles to a literal constant. This is monomorphization working
*for* you: one template, one machine-code body per scale actually used,
each with its scale baked into the immediates.

Java has no equivalent — a generic can't be parameterized on a *value*.
The closest JVM idiom is a runtime `scale` field on every object (8 wasted
bytes and a runtime multiply) or discipline-and-prayer.

### 2. Where the compiler deduces, and where you must spell

```cpp
auto p = Fixed<10'000>::from_ticks(1'551'000);  // class scope: spell Scale
auto q = round_to_tick<25>(p);                  // NTTP of the FUNCTION: spell it
auto r = midpoint(p, p);                        // deduced from arguments
p.rescale<100'000>();                           // member template: spell target
```

Rule of thumb: deduction works when the parameter appears in the function's
*argument types*; anything else (return-type-only parameters, NTTPs chosen
by the caller) must be explicit. When you implement `rescale`, note the
syntax wart if you ever call it through a dependent name: `obj.template
rescale<N>()` — worth meeting once so it doesn't ambush you in a template
someday.

### 3. `<=>`: six operators for one `= default`

Module 3 synthesized comparisons via CRTP and noted C++20 dissolved the
pattern. Here's the dissolution: declare

```cpp
auto operator<=>(const Fixed&) const = default;   // plus ==, also defaultable
```

and the compiler derives `<, <=, >, >=` from `<=>` (memberwise, here: one
int64 compare) and `!=` from `==`. The return type for integers is
`std::strong_ordering`. The skeleton ships these as *non*-defaulted stubs
so the tests are honestly red — your job includes discovering that the
entire correct implementation is the keyword `default`. (Interview aside:
when can't you default it? When ordering isn't memberwise-lexicographic in
declaration order — e.g. a02's `Version`-style types are fine, but a
best-bid ordering that ranks *descending* by price needs a hand-written
one.)

### 4. Cross-scale conversion is where the bugs live

`rescale<To>()` converts exactly when upscaling (multiply by `To/From`)
and **truncates toward zero** when downscaling (divide by `From/To`) —
the same deliberate rule as m01a02's VWAP. The tests pin both directions
plus negatives (integer division truncates toward zero in C++ — `-15/10 ==
-1`, not `-2`; this is spec since C++11 and a classic interview one-liner).
Guard rails: both scales are positive, and one must divide the other
(assert this at compile time with `static_assert` inside the function —
the error then names the actual offense).

## Assignment spec

Everything in `include/course/fixed.hpp` (header-only — it's a template;
THEORY in the module README says why). Implement the `// TODO` bodies:

- `Fixed<Scale>`: `from_ticks` / `from_units` factories, `ticks()`,
  `units_truncated()`, `operator+`/`operator-` (same scale only),
  `operator*`(int64 qty), `operator/`(int64 divisor, truncating),
  `operator==` and `operator<=>` (hint: THEORY §3),
  `rescale<ToScale>()`.
- Free functions: `midpoint(a, b)` (no int64 overflow for large operands —
  `a + (b-a)/2`, truncation toward `a`), `round_down_to_tick<TickTicks>(p)`
  (largest multiple of `TickTicks` ≤ p; negative prices round *down*,
  toward −∞ — this one is NOT plain truncation, and the tests know).
- Compile-time contracts already in the header (they pass in stub state;
  keep them passing): `sizeof(Fixed<S>) == 8`, cross-scale `+` does not
  compile (checked via a `requires` probe), trivially copyable.

**Acceptance criteria:** all `m04a01.*` tests pass in `debug` and `asan`.

## Hints

<details><summary>Hint 1 — the comparison stubs</summary>
Delete both stub bodies and write <code>= default</code> after each
signature instead. Then read the generated semantics back from the tests
and convince yourself memberwise comparison of <code>ticks_</code> is the
right ordering for a price.
</details>

<details><summary>Hint 2 — rescale in two branches</summary>
<code>if constexpr (ToScale &gt;= Scale)</code> multiply by
<code>ToScale / Scale</code>; else divide by <code>Scale / ToScale</code>.
Both ratios are integer constants at compile time. The
<code>static_assert</code> for divisibility sits above the branch.
</details>

<details><summary>Hint 3 — round_down_to_tick for negatives</summary>
Truncating division rounds toward zero; you need toward −∞. Compute
<code>q = t / tick</code>, and if <code>t % tick != 0 && t &lt; 0</code>,
subtract one from q. (Or use the euclidean-mod trick; either passes.)
</details>

## Further reading

- *A Tour of C++* ch. 7 (templates), 8 (concepts overview — next
  assignment).
- cppreference: [non-type template parameters](https://en.cppreference.com/w/cpp/language/template_parameters),
  [default comparisons](https://en.cppreference.com/w/cpp/language/default_comparisons).
- CppCon 2021, Bob Steagall, *"Back to Basics: Classic STL"* — for how the
  standard library thinks about templates.
- [P0515 Consistent comparison](https://wg21.link/p0515) — the spaceship
  paper; §1 is a readable design rationale.
