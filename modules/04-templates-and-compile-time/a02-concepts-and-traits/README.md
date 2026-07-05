# a02 — Concepts & Traits: Constraining the Duck

**Estimated effort:** 5–7 hours.

## Learning objectives

- Write a type trait with the pre-C++20 detection idiom (`void_t`) and
  understand the SFINAE machinery it rides on.
- Write real C++20 concepts with compound `requires` expressions.
- Constrain function templates three ways (`requires` clause, trailing
  `requires`, constrained `auto`) and let concepts steer overload
  resolution.
- Map this onto what you know: structural vs nominal typing, Scala
  typeclasses, Go interfaces.

## THEORY

### 1. Templates are duck-typed — that's the problem being solved

`template <typename T> auto spread(const T& q) { return q.ask() - q.bid(); }`
compiles against *anything* until instantiated. Pass a type without
`.bid()` and pre-C++20 you got 200 lines of error novel pointing INSIDE
the template, at the wrong altitude. Worse, the *interface* of `spread` is
implicit — nothing in its signature says what T must provide. Traits and
concepts move the requirement into the signature, where callers (and error
messages, and overload resolution) can see it.

Coming from your stack: Java interfaces are *nominal* (a type participates
by declaring `implements`); C++ concepts are *structural* (a type
participates by having the right shape — closest to Scala's structural
types or Go interfaces, and to typeclasses in how they select overloads).
No registration, no inheritance, no vtable: satisfaction is checked at
compile time, per instantiation.

### 2. The old way: SFINAE and the detection idiom

Substitution Failure Is Not An Error: if substituting `T` into a
candidate's *signature* produces an invalid type, the candidate silently
leaves the overload set. `std::void_t` weaponizes this into a
yes/no detector:

```cpp
template <typename T, typename = void>
struct has_bid : std::false_type {};                    // fallback

template <typename T>
struct has_bid<T, std::void_t<decltype(std::declval<const T&>().bid())>>
    : std::true_type {};                               // wins iff .bid() is valid
```

The partial specialization is *more specialized*, so it's preferred — but
only when `decltype(declval<const T&>().bid())` substitutes successfully.
`declval<T>()` is the "pretend I have a value of T" tool, usable only in
unevaluated contexts. You will write one of these by hand, once, so that
(a) legacy code doesn't read like runes and (b) you feel the difference
when you then delete it and write the concept.

### 3. The new way: requires-expressions

```cpp
template <typename T>
concept Quotable = requires(const T& q) {
    { q.bid() } -> std::convertible_to<std::int64_t>;   // compound: expr + type
    { q.ask() } -> std::convertible_to<std::int64_t>;
    typename T::symbol_type;                            // type requirement
    requires !std::is_polymorphic_v<T>;                 // nested requirement
};
```

Each line is a requirement: *simple* (the expression must compile),
*compound* (`{ expr } -> constraint` also constrains the result type —
note the constraint takes the expression's type as its silent first
argument), *type* (`typename T::foo` must name a type), *nested*
(`requires bool-expr` must be true, evaluated — this is the line people
forget `requires` on, silently turning a check into "does this expression
compile", a classic bug the tests probe). Concepts get you readable
one-line diagnostics ("constraint not satisfied ... because q.ask() would
be invalid") and, unlike a bare `static_assert`, they compose into
overload resolution: a more-constrained overload beats a less-constrained
one (subsumption).

### 4. Three spellings of the same constraint

```cpp
template <Quotable Q> int64_t spread(const Q&);                    // terse
template <typename Q> requires Quotable<Q> int64_t spread(const Q&); // clause
int64_t spread(const Quotable auto& q);                            // abbreviated
```

All equivalent for this course's purposes; the codebases you'll join mix
them freely, so the tests don't care which you use. What they *do* care
about: constraint-driven dispatch — `describe(T)` below must pick its
answer via concepts/`if constexpr`, not runtime flags.

## Assignment spec

Everything in `include/course/quotable.hpp` (header-only). A `SimpleQuote`
fixture type and several deliberately-broken quote types live in the test
file — read them first; they define exactly where the boundaries are.

1. **`has_bid<T>` / `has_bid_v<T>`** — the hand-rolled `void_t` detector
   from THEORY §2: true iff `const T` has a callable `.bid()`. Ships as
   `false_type` only; you add the detecting specialization.
2. **`concept Quotable`** — `const T` provides `bid()` and `ask()`, both
   `convertible_to<std::int64_t>`. Ships as `= false`.
3. **`concept TightlyQuotable`** — refines `Quotable`: additionally
   `qty_at_bid()`/`qty_at_ask()` convertible to `std::int32_t`. Ships as
   `= false`. Must SUBSUME Quotable (define it as `Quotable<T> && ...` so
   the compiler can rank overloads).
4. **`spread(q)`** — constrained on `Quotable`; `ask - bid` as int64.
5. **`weighted_mid(q)`** — two overloads, and this is the point of the
   assignment: a `Quotable` version returning the plain midpoint
   `(bid+ask)/2`, and a `TightlyQuotable` version returning the
   quantity-weighted mid `(bid*qty_ask + ask*qty_bid)/(qty_bid+qty_ask)`
   (all integer math, truncating). A `TightlyQuotable` argument must
   select the weighted overload *by subsumption* — no casts, no tags.
   Stubs return 0.

**Acceptance criteria:** all `m04a02.*` tests pass in `debug` and `asan`.
The concept tests are `EXPECT_TRUE(Quotable<Good>)` /
`EXPECT_FALSE(Quotable<Broken>)` at runtime so the stubs stay buildable —
once green, promote a few to `static_assert` in your own code for the
real-world habit.

## Hints

<details><summary>Hint 1 — the detector's second parameter</summary>
The primary template's <code>typename = void</code> is the slot the
specialization competes on: <code>void_t&lt;...&gt;</code> collapses to
<code>void</code> exactly when its contents substitute. If your
specialization never wins, check you wrote <code>std::declval&lt;const
T&&gt;()</code> — plain <code>T&</code> misses const-callability, and one
fixture type has a non-const <code>bid()</code> to catch it.
</details>

<details><summary>Hint 2 — compound requirement syntax</summary>
<code>{ q.bid() } -&gt; std::convertible_to&lt;std::int64_t&gt;;</code> —
no <code>()</code> after the concept: the expression's type is passed as
its implicit first argument. Writing
<code>convertible_to&lt;std::int64_t&gt;()</code> or forgetting the
braces are the two standard syntax faceplants.
</details>

<details><summary>Hint 3 — the overload pair</summary>
Write both with the SAME name and argument shape, one constrained
<code>Quotable</code>, one <code>TightlyQuotable</code>. Subsumption only
works if TightlyQuotable is literally defined as
<code>Quotable&lt;T&gt; && (more)</code> — restating bid()/ask()
requirements inline breaks the subsumption relation and the call becomes
ambiguous. If you get an ambiguity error on the weighted test, that's
what happened.
</details>

## Further reading

- *A Tour of C++* ch. 8; *C++ Templates: The Complete Guide* (Vandevoorde)
  ch. 19 (traits) if you want the deep end.
- [Abseil Tip #227](https://abseil.io/tips/227) — designing good concepts.
- Walter Brown, *"Modern Template Metaprogramming: A Compendium"*
  (CppCon 2014, parts I–II) — where void_t came from; still the best
  explanation of the detection idiom.
- cppreference: [constraints and concepts](https://en.cppreference.com/w/cpp/language/constraints).
