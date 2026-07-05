# a03 — constexpr & Variadics: Work the Compiler Does For You

**Estimated effort:** 5–7 hours.

## Learning objectives

- Write `constexpr` functions that build lookup tables the binary ships
  ready-made — zero startup cost, zero runtime construction.
- Write a table-driven `constexpr` parser and force its evaluation at
  compile time.
- Use parameter packs and fold expressions fluently.
- Build a compile-time-dispatched message router — the skeleton of Module
  11's feed handler.

## THEORY

### 1. constexpr: one function, two worlds

A `constexpr` function is ordinary code that is *allowed* to run during
compilation. Call it with constants in a context that needs a constant
(initializing a `constexpr` variable, an array bound, a template argument)
and the compiler interprets it then and there; call it with runtime values
and it's a normal function. Same source, two execution times:

```cpp
constexpr int square(int x) { return x * x; }
constexpr int a = square(9);   // compile time: '81' is in the binary
int b = square(read_int());    // runtime: ordinary call
```

Inside a compile-time evaluation the interpreter is strict: no UB (signed
overflow, OOB indexing and use of uninitialized values become *compile
errors* — constexpr is accidentally a tiny sanitizer), no
`reinterpret_cast`, no calls to non-constexpr functions. Since C++20 you
even get transient `new`/`vector` — but anything allocated must be freed
before evaluation ends; the compile-time heap can't leak into the binary.

### 2. Tables in .rodata: the low-latency angle

The classic pattern this assignment drills:

```cpp
constexpr std::array<std::uint8_t, 256> kDigit = make_digit_table();
```

`make_digit_table()` runs *in the compiler*; the 256 result bytes are
baked into the read-only data segment. Compare the alternatives you'd
write on the JVM: a static initializer that runs at class-load (startup
cost, ordering hazards) or computing per call. Trading systems are full of
these tables — character classes for parsers, precomputed powers of ten,
CRC tables, price-level bucket maps — because .rodata is warm, shared
across processes, and free at runtime. (`static const` locals with dynamic
init also exist — and cost a thread-safe initialization check per call;
`constexpr` makes the guard provably unnecessary. Interview-grade nuance.)

### 3. Parameter packs and folds

```cpp
template <typename... Ts>            // Ts...: a pack of TYPES
constexpr auto sum_all(Ts... xs) {   // xs...: a pack of VALUES
    return (xs + ... + 0);           // fold: ((x1 + x2) + x3) ... + 0
}
```

A pack isn't a container — it's a compile-time list the compiler unrolls.
You can't index it or loop over it at runtime; you *expand* it
(`pattern(xs)...`) or *fold* it (`(xs op ...)`). The four fold shapes are
worth memorizing; the `+ 0` above is an init value making the empty pack
legal. Folds over `&&`/`||` short-circuit — which is exactly the trick the
dispatcher needs.

### 4. The variadic dispatcher: closed-set routing with no vtable

A feed handler routes messages by a one-byte type tag: `'A'` → add order,
`'E'` → execution… The OO answer is a map of handler objects behind an
interface — a hash lookup plus a virtual call per message. The
metaprogramming answer stamps the routing out at compile time:

```cpp
Dispatcher d{AddHandler{}, ExecHandler{}, CancelHandler{}};
d.dispatch('E', payload);   // compiles to: three inlined tag compares
```

Each handler declares its tag as `static constexpr char kType` and is
stored *by value* in a `std::tuple`. `dispatch` walks the tuple —
`std::apply` hands you the handlers as a pack, an `||`-fold tries each and
short-circuits on the first match. No allocation, no indirection; with
single-digit handler counts, a compare-chain beats a hash map hollow.
(Class template argument deduction — CTAD — is what lets you write
`Dispatcher d{...}` without spelling the handler types.)

## Assignment spec

Everything in `include/course/ctdispatch.hpp` (header-only). All functions
must be usable at compile time — several tests evaluate them in
`constexpr` variables.

1. **`make_digit_table()`** → `std::array<std::uint8_t, 256>`: `'0'..'9'`
   map to 0–9, every other byte maps to 255. (`kDigitTable` is already
   declared from it.)
2. **`make_pow10_table()`** → `std::array<std::int64_t, 19>`: 10⁰…10¹⁸.
3. **`parse_uint(std::string_view)`** → `std::optional<std::int64_t>`:
   non-negative decimal via `kDigitTable` (no `'-'`, no dot); `nullopt`
   for empty input, any non-digit byte, or overflow past
   `int64_t` max. Overflow checks must be UB-free — at compile time UB is
   a hard error, so a sloppy check means the `constexpr` tests won't even
   run (THEORY §1).
4. **`sum_all(xs...)`** → `std::int64_t`: fold warm-up; empty call is 0.
5. **`Dispatcher<Handlers...>`**: constructor stores the handlers;
   `dispatch(char type, std::string_view payload)` invokes the *stored*
   handler whose `kType` matches (they're stateful — mutations must
   persist across calls) and returns whether anyone matched. Duplicate
   tags: first match wins, later ones never fire.

**Acceptance criteria:** all `m04a03.*` tests pass in `debug` and `asan`.
Afterwards, one Compiler Explorer errand: paste your `parse_uint` plus
`constexpr auto v = parse_uint("1234567");` and confirm the assembly
contains only the literal `1234567` — the entire parser evaporated.

## Hints

<details><summary>Hint 1 — building the tables</summary>
Declare the array, loop over it with an index (yes, ordinary loops — this
is C++20 constexpr, not the C++11 single-return museum), assign, return.
<code>make_pow10_table</code>: element 0 is 1, each next is
<code>prev * 10</code>.
</details>

<details><summary>Hint 2 — constexpr-safe overflow check</summary>
Same discipline as m00a01: test BEFORE the multiply —
<code>if (acc &gt; (max - d) / 10) return std::nullopt;</code>. If a
<code>constexpr auto v = parse_uint("9223372036854775808")</code> test
errors at COMPILE time instead of yielding nullopt, your check runs after
the damage.
</details>

<details><summary>Hint 3 — dispatch shape</summary>
<pre>return std::apply(
    [&](auto&... hs) {
        return ((hs.kType == type ? (hs(payload), true) : false) || ...);
    },
    handlers_);</pre>
Take <code>auto&...</code> (by reference — the stateful-handler test
catches <code>auto...</code> copies), and let the <code>||</code>-fold
short-circuit. If "first match wins" fails you folded right instead of
left.
</details>

## Further reading

- cppreference: [constexpr](https://en.cppreference.com/w/cpp/language/constexpr),
  [fold expressions](https://en.cppreference.com/w/cpp/language/fold),
  [CTAD](https://en.cppreference.com/w/cpp/language/class_template_argument_deduction).
- CppCon 2017, Ben Deane & Jason Turner, *"constexpr ALL the Things!"* —
  a compile-time JSON parser; your parse_uint is its first act.
- Jason Turner, C++ Weekly ep. 340 *"Fold Expressions"* — ten minutes,
  all four shapes.
