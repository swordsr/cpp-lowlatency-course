# a03 — SmallVector & SoA: Layout Is the Optimization

**Estimated effort:** 6–9 hours.

## Learning objectives

- Implement small-buffer optimization: a vector whose first N elements
  live *inside the object*, no heap in sight.
- Transform an array-of-structs into a struct-of-arrays and measure why.
- Recognize data-oriented design as your data-infra intuition (row store
  vs column store) applied at cache-line scale.

## THEORY

### 1. Small-buffer optimization: the heap you don't visit

Module 1 met SSO — `std::string` keeping short strings inline.
Generalized, it's SBO, and you'll now build the container form:
`SmallVector<T, N>` embeds `N` elements' worth of raw storage in the
object itself. Sizes ≤ N: zero allocations, perfect locality (the data is
*inside* the object you already have in cache). Size N+1: spill to the
heap like a normal vector and never look back.

Why trading code loves it: the distribution of real container sizes is
brutally skewed. Price levels per order-book update: usually 1–4. Fills
per execution report: usually 1. Choose N to cover the p99 case and the
heap becomes a rounding error. (`boost::container::small_vector`,
LLVM's `SmallVector` — every serious codebase has one.)

The costs, so you choose N and not vibes: `sizeof(SmallVector<T, N>)`
grows by `N * sizeof(T)` whether used or not (bloats whatever contains
it); moves can no longer just steal a pointer when inline (elements must
be moved one by one — so moved-around containers want *small* N); and the
`is_inline()` branch sits on every `data()` call (predictable, but real).

### 2. AoS vs SoA — you already know this as row store vs column store

Your data-infra life: scanning one column of a Parquet file doesn't read
the other forty. Same physics, smaller scale:

```cpp
struct OrderAoS { int64 price; int32 qty; /* + 48 bytes of cold fields */ };
OrderAoS orders[100'000];                    // "row store"

struct OrderColumns {                        // "column store"
    std::vector<std::int64_t> prices;
    std::vector<std::int32_t> qtys;
    /* cold fields in their own columns, far away */
};
```

Summing `price * qty` over the AoS drags all 64 bytes of every order
through the cache to use 12: **19% of every cache line is payload**, so
the memory system does 5× the work. The SoA version reads two dense
streams: 100% payload, plus the compiler can autovectorize (NEON on your
M-chip) because the values are contiguous. The benchmark makes you watch
both effects; Module 6 explains the machinery underneath.

What SoA costs: writing one logical record scatters across arrays
(insert/erase get clumsy), "give me order #i" now gathers from every
column, and referential identity (`Order*`) disappears — there is no
object, only an index. Hot-loop fields columnar, everything else
row-shaped, is the standard armistice.

### 3. Choosing between them is a *measurement*, not a taste

The handout question this assignment answers: "when does SoA win?" —
when the traversal touches a small fraction of each record
(arithmetic-intensity argument), the dataset exceeds cache, and the loop
is the hot one. AoS wins when you touch whole records, when N is tiny, or
when insert/erase dominates. Interviewers don't want the slogan; they
want the ratio ("we use 12 of 64 bytes, so ~5× memory traffic") and a
benchmark you've actually run.

## Assignment spec

Part 1 — **`include/course/small_vector.hpp`**: `SmallVector<T, N>`.

- Inline storage: `alignas(T)` byte array for N elements inside the
  object (given as a member); spill to heap at N+1 with 2× growth from
  there. `is_inline()` reports which mode you're in.
- `push_back` (copy + move), `emplace_back`, `pop_back`, `size`,
  `capacity` (== N while inline), `operator[]`, `begin`/`end`, `clear`,
  destructor. Copy and move are **deleted** (given): you proved you can
  write them in m02a04; here the storage duality is the lesson.
- Elements must be moved from inline to heap on spill (Tracker-counted),
  and every constructed object destroyed exactly once.

Part 2 — **`include/course/soa.hpp`**: both layouts + the traversals.

- `OrderAoS` (given): hot fields + 48 bytes of explicit cold ballast.
- `sum_notional_aos(const std::vector<OrderAoS>&)` — Σ price·qty.
- `OrderColumns` — parallel `prices`/`qtys`/`cold` columns with
  `push_back(price, qty)`, `size()`, and `sum_notional()`. Invariant:
  all columns same length, order i is the same order in every column.
- `from_aos(const std::vector<OrderAoS>&)` — the transform.

**Acceptance criteria:** all `m05a03.*` tests pass in `debug` and `asan`
(the spill tests are where use-after-free lives — moving FROM the inline
buffer you're about to stop using, not TO it).

**Benchmark:** `m05a03_bench` (release, complete harness): notional sum
over 100k orders, AoS vs SoA — expect **3–6×** on Apple Silicon once your
code is green (both stubs return 0 instantly; ignore until then). Also
`SmallVector<int64,8>` push_back×4 vs `std::vector` — the no-allocation
regime, expect ~2–5×. Record both ratios in a comment in `soa.hpp`, plus
one sentence: which effect (line utilization vs vectorization) do you
think dominates, and how would you check? (Module 6 gives you the tools;
write the guess down now.)

## Hints

<details><summary>Hint 1 — SmallVector storage plumbing</summary>
<code>data()</code> is the single source of truth:
<code>is_inline() ? reinterpret_cast&lt;T*&gt;(inline_storage_) :
heap_;</code>. Every other member goes through it. Inline mode is simply
<code>heap_ == nullptr</code>.
</details>

<details><summary>Hint 2 — the spill, step by step</summary>
Allocate <code>::operator new(2 * N * sizeof(T))</code>; placement-move
the N inline elements over (<code>std::move</code> each, m02a04 loop);
destroy the N originals in the inline buffer; set <code>heap_</code>,
<code>capacity_</code>. THEN construct the new element. If the
PushBackOwnElement-style aliasing occurs to you — good instinct, but it's
deliberately not in this spec; the m02a04 scar is enough.
</details>

<details><summary>Hint 3 — SoA sum won't vectorize?</summary>
It will if you let it: plain index loop over
<code>prices[i] * qtys[i]</code>, accumulate int64, no branches. Check
with Compiler Explorer (<code>-O2</code>): look for NEON <code>smull/
smlal</code> or x86 <code>pmuludq</code>-family in the loop body. If you
see scalar code, something in your loop carries a dependency it
shouldn't.
</details>

## Further reading

- Mike Acton, *"Data-Oriented Design and C++"* (CppCon 2014) — the talk
  that named the movement. Watch it this week.
- LLVM Programmer's Manual, [SmallVector section](https://llvm.org/docs/ProgrammersManual.html#llvm-adt-smallvector-h)
  — design notes from the most-used SBO container alive.
- [What Every Programmer Should Know About Memory], §3.3 (Drepper) — the
  bandwidth math behind §2; full treatment next module.
- CppCon 2016, Chandler Carruth, *"High Performance Code 201: Hybrid Data
  Structures"* — SmallVector and friends, from LLVM's maintainers.

[What Every Programmer Should Know About Memory]: https://people.freebsd.org/~lstewart/articles/cpumemory.pdf
