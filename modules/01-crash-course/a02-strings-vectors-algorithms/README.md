# a02 — Strings, Vectors, Algorithms: A Trade Tape Analyzer

**Estimated effort:** 4–6 hours.

## Learning objectives

- Use `std::string_view` for zero-copy parsing and know exactly when it's
  safe.
- Use `std::vector` fluently and know its memory behavior (contiguity,
  growth, invalidation).
- Reach for `<algorithm>` (sort, accumulate, max_element…) instead of raw
  loops, with lambdas as predicates.
- Feel allocation cost in a benchmark for the first time.

## THEORY

### 1. `std::string` owns, `std::string_view` looks

`std::string` is a value type that owns a heap buffer (or, for short
strings, stores the bytes inline — the *small-string optimization*, ≤15
chars on the implementations you'll meet). Copying a long string allocates
and memcpys. In a parser that touches millions of lines, copying every field
means an allocation per field — this is the single most common performance
sin in naive C++ parsing code.

`std::string_view` is the fix: a (pointer, length) pair into bytes owned by
*someone else*. Substringing a view is arithmetic, not allocation:

```cpp
std::string line = read_line();          // owns the bytes
std::string_view sv{line};               // looks at them
std::string_view sym = sv.substr(16, 4); // still just pointer math
```

The contract you accept: **the view must not outlive the owner**, and
mutating/reallocating the owner invalidates all views. A function *taking*
`string_view` is almost always right (caller keeps ownership, any string-ish
thing binds to it). A function *returning* or *storing* one is a design
decision about lifetimes — our `Trade` struct stores `std::string symbol`
(owning) precisely so parsed trades can outlive the tape text. In Module 11
you'll make the opposite choice, and it'll be the right one there.

### 2. `std::vector` is THE data structure

One contiguous heap buffer, three pointers (begin/end/capacity). Contiguity
is why it dominates: sequential traversal is what caches are built for
(Module 6 makes you measure this). Two behaviors to internalize now:

- **Growth:** `push_back` beyond capacity allocates a bigger buffer
  (≈2×) and moves everything over — amortized O(1), worst-case O(n), and
  **every iterator/pointer/reference/`string_view` into it is invalidated**.
  If you know the size, `reserve()` up front.
- **Element type matters:** `vector<Trade>` stores the Trades themselves,
  in-line, back to back — not references to Trades scattered on a heap. This
  is value semantics again, and it's why sorting a vector physically moves
  objects.

### 3. `<algorithm>` — loops with names

`std::sort`, `std::stable_sort`, `std::accumulate` (in `<numeric>`),
`std::max_element`, `std::find_if`, `std::count_if`… Each takes an iterator
range and often a lambda:

```cpp
std::stable_sort(trades.begin(), trades.end(),
                 [](const Trade& a, const Trade& b) {
                     return a.timestamp_ns < b.timestamp_ns;
                 });
```

Interviewers read named algorithms as fluency the way they read `stream()`
chains in Java. Learn the comparator discipline now: strict weak ordering,
`<` not `<=` (a `<=` comparator is UB in `std::sort` — worth knowing *why*
by the end of this module's question bank). `stable_sort` preserves the
relative order of equal keys — required here, because exchanges preserve
time priority within a price (foreshadowing Module 10).

### 4. Integer VWAP

Volume-weighted average price = `Σ(price·qty) / Σ(qty)`. With prices in
ticks (Module 0) this is exact integer arithmetic; we truncate toward zero
on the final divide and note it in the interface. Real systems document
their rounding rule in the spec — off-by-one-tick reconciliation bugs are a
genuine industry pastime.

## Assignment spec

Types in `include/course/trade_tape.hpp`, implementations in
`src/trade_tape.cpp`. The tape format is one trade per `\n`-separated line:

```
timestamp_ns,symbol,price_ticks,quantity,side
1700000000123456789,AAPL,1551000,100,B
```

Implement:

- `split(std::string_view text, char sep) -> std::vector<std::string_view>`
  — zero-copy tokenizer. `"a,,b"` → `{"a","","b"}`; empty input → `{""}`.
- `parse_trade(std::string_view line) -> std::optional<Trade>` — exactly 5
  fields; timestamp/price/quantity are decimal integers (quantity > 0,
  timestamp/price ≥ 0); side is `B` or `S`. Anything else → `nullopt`.
- `parse_tape(std::string_view text) -> std::vector<Trade>` — split on
  `'\n'`, parse each non-empty line, **skip** malformed lines.
- `total_volume(const std::vector<Trade>&, std::string_view symbol) -> std::int64_t`
- `vwap(const std::vector<Trade>&, std::string_view symbol) -> std::optional<std::int64_t>`
  — ticks, truncated toward zero; `nullopt` if the symbol has no trades.
- `most_active_symbol(const std::vector<Trade>&) -> std::string` — by total
  quantity; ties → lexicographically smallest; empty tape → `""`.
- `sorted_by_time(std::vector<Trade> trades) -> std::vector<Trade>` — by
  timestamp, **stable** for equal timestamps. (Note it takes its parameter
  by value — think about why that's the right signature for sort-a-copy.)

Use `<algorithm>`/`<numeric>` where one fits; the hint ladder names them.

**Acceptance criteria:** all `m01a02.*` tests pass in `debug` and `asan`
(ASan matters here — off-by-one views are memory bugs, and ASan sees them).

**Benchmark:** `m01a02_bench` compares your `split` against a deliberately
naive copying tokenizer included in the bench file. Build `release`, run it,
and look at the gap — typically 5–15× on Apple Silicon once your version is
allocation-free except for the result vector. If your version is *not*
clearly faster, you're copying somewhere. Record both numbers in a comment
in `trade_tape.cpp`.

## Hints

<details><summary>Hint 1 — split</summary>
Loop with <code>find(sep, pos)</code>: each token is
<code>text.substr(pos, next - pos)</code>. Remember the final token after
the last separator — the <code>"a,,b"</code> and trailing-separator tests
catch fencepost errors.
</details>

<details><summary>Hint 2 — integer field parsing</summary>
You wrote this in m00a01. A tiny local helper — walk chars, validate
digits, accumulate <code>int64</code> — is fine (no '.' this time).
<code>std::from_chars</code> in <code>&lt;charconv&gt;</code> is the
standard-library route and worth meeting; either passes.
</details>

<details><summary>Hint 3 — which algorithms</summary>
<code>total_volume</code>: <code>std::accumulate</code> with a lambda.
<code>most_active_symbol</code>: aggregate into a
<code>std::map&lt;std::string, std::int64_t&gt;</code> (sorted keys make
the tie-break free), then <code>std::max_element</code> with a comparator
on <code>.second</code> — strictly less-than, so earlier (smaller) keys win
ties. <code>sorted_by_time</code>: <code>std::stable_sort</code>, then
return the by-value parameter directly.
</details>

## Further reading

- *A Tour of C++* ch. 9 (strings), 11 (containers), 12–13 (algorithms).
- [Abseil Tip #1: string_view](https://abseil.io/tips/1) — the lifetime
  contract in three minutes.
- CppCon 2018, Victor Ciura, *"Enough string_view to Hang Ourselves"*.
- cppreference: [algorithm](https://en.cppreference.com/w/cpp/algorithm) —
  skim the index; knowing what exists is half the skill.
