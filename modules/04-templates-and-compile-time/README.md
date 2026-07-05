# Module 4 — Templates & Compile-Time

**Time budget:** ~2 weeks. Three assignments.

You've *used* templates since Module 1 (`vector<T>`, your own
`UniquePtr<T,D>`, CRTP). This module makes you fluent in *writing* them —
and in the C++ superpower Java/Scala genuinely don't have: running real
computation at compile time and shipping only the results.

The mental shift from the JVM: **Java generics erase; C++ templates
stamp.** `List<Integer>` and `List<String>` share one bytecode body with
casts at the edges. `Vector<int>` and `Vector<Order>` are two unrelated
types with separately generated, separately optimized machine code —
monomorphization. That's why templates can hold `int` by value, why
`sort<MyComparator>` inlines the comparator, and why there's a price
(code size, header exposure, compile times, error novels).

1. **a01** — class templates + non-type template parameters: a `Fixed<Scale>`
   price type where the *scale is part of the type*;
2. **a02** — constraining templates: the old way (traits + detection) and
   the C++20 way (concepts);
3. **a03** — computing at compile time: `constexpr` tables and a variadic
   message dispatcher.

## Assignments

| # | Assignment | What you'll touch |
|---|------------|-------------------|
| 1 | [a01-generic-fixed-point](a01-generic-fixed-point/) | class/function templates, NTTPs, defaulted `<=>` |
| 2 | [a02-concepts-and-traits](a02-concepts-and-traits/) | type traits, detection idiom, concepts, `requires` |
| 3 | [a03-constexpr-and-variadics](a03-constexpr-and-variadics/) | constexpr functions/tables, parameter packs, folds |

## Interview questions

<details>
<summary>"Compare Java generics and C++ templates."</summary>

Erasure vs monomorphization. Java: one compiled body, type checks at the
boundary, primitives must box, no `new T()`, constraints via bounded types
(`T extends Comparable`). C++: one instantiation per argument set — values
stored inline, everything inlinable, non-type parameters allowed, arbitrary
compile-time computation; costs are code bloat, definitions in headers, and
(pre-concepts) inscrutable errors. Bonus point: C++ templates are
Turing-complete and checked *structurally* at instantiation, closer to
Scala typeclasses/duck typing than to Java interfaces.
</details>

<details>
<summary>"Why must template definitions live in headers?"</summary>

The compiler generates code per instantiation, so it needs the full
definition wherever an instantiation is triggered — and TUs compile
independently (Module 0). The linker then deduplicates the identical
instantiations (inline/COMDAT). Explicit instantiation in one TU is the
escape hatch for controlling bloat and compile time.
</details>

<details>
<summary>"What is SFINAE, in one breath, and what replaced it?"</summary>

"Substitution Failure Is Not An Error": when substituting template
arguments into a candidate's *signature* fails, the candidate is silently
dropped from overload resolution rather than erroring — which pre-C++20
code exploited (`enable_if`, `void_t` detection) to switch templates on
type properties. C++20 concepts express the same constraints directly,
with readable diagnostics and overload ranking via subsumption.
</details>

<details>
<summary>"<code>constexpr</code> vs <code>const</code>?"</summary>

`const` = "I won't mutate this after initialization" (runtime property).
`constexpr` on a variable = "initialized with a compile-time constant, in
.rodata, usable as an array bound / template argument"; on a function =
"*may* be evaluated at compile time when the arguments are constants —
and still callable at runtime otherwise". `consteval` forces compile-time.
Follow-up they like: what can't constexpr code do? (Pre-C++20: allocation,
virtual calls; the list keeps shrinking each standard.)
</details>

<details>
<summary>"Where do trading systems actually use this machinery?"</summary>

Fixed-point types with scale/tick-size as template parameters (dimensional
analysis for money — wrong-scale arithmetic doesn't compile); protocol
parsers with per-message-type code stamped from one template (Module 11);
compile-time lookup tables replacing startup initialization; CRTP
strategies (Module 3); `if constexpr` pruning cold branches out of hot
paths entirely.
</details>
