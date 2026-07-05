# a02 — Memory Ordering: Your CPU Reorders; Your Mac Admits It

**Estimated effort:** 6–9 hours. The hardest theory of the course —
budget reading time.

## Learning objectives

- Explain why reordering exists (compiler AND hardware) and what
  happens-before / synchronizes-with mean, precisely.
- Use `memory_order_acquire`/`release` for the publish/consume handoff
  and defend every ordering you write.
- Run litmus experiments on your own machine and SEE relaxed reordering
  — which x86 colleagues cannot.
- Implement the interview classic: correct double-checked lazy
  initialization.

## THEORY

### 1. Nobody executes your program in order

Two conspirators reorder your memory operations: the **compiler**
(hoisting, sinking, caching values in registers — an optimization is
just a reordering with a diploma) and the **CPU** (store buffers,
out-of-order execution, speculation). Single-threaded, the "as-if" rule
hides all of it. The moment a second thread watches the same memory, the
illusion collapses — and what it sees depends on the *memory model*.

**x86 (TSO):** stores leave in order, loads leave in order; the one
visible relaxation is a store's result being delayed past a younger load
(the store buffer). Strong enough that most missing-barrier bugs pass
undetected for years. **ARM64 (your Mac):** weakly ordered — independent
loads and stores reorder nearly freely unless you say otherwise. The bug
your x86 CI never catches fires on your laptop in seconds. In this
assignment, that's a feature: run the litmus bench and watch.

### 2. The contract: happens-before

`std::atomic` buys you two separate things — **atomicity** (no torn
reads/writes) and, per your chosen `memory_order`, **ordering**. The
orderings build one relation: *happens-before*. The rule that matters:

> A **release store** that is **read by** an **acquire load** creates
> synchronizes-with: everything sequenced before the store is visible
> to everything sequenced after the load.

That single sentence is the message-passing pattern, the mutex-release/
acquire pattern, and 90% of correct lock-free code:

```cpp
// producer                          // consumer
payload = 42;                        if (flag.load(std::memory_order_acquire))
flag.store(true,                         use(payload);   // guaranteed 42
           std::memory_order_release);
```

With `relaxed` on both: atomicity yes, ordering no — the consumer can see
`flag == true` yet stale `payload`. On x86 the hardware happens to order
it anyway (the compiler still may not!); on your ARM Mac it visibly
breaks. This is a02's MailBox, and the tests + tsan hold you to it.

The six orderings, honestly ranked for practice: `seq_cst` (the default;
adds a single global order over all seq_cst ops — correct first, then
relax), `acquire`/`release`/`acq_rel` (the handoff workhorses),
`relaxed` (counters and statistics only — atomicity without ordering),
`consume` (never — deprecated in practice, no compiler implements it as
specified).

### 3. Litmus tests: the lab notebook of memory models

Tiny two-thread programs whose forbidden/allowed outcomes characterize a
memory model. The two classics, both runnable in this assignment's bench:

- **Store buffering (SB):** T1: `x=1; r1=y` / T2: `y=1; r2=x`. Can
  `r1==r2==0`? Sequential consistency says no. *Both* x86 and ARM say
  yes with relaxed (store buffers) — this one reorders everywhere.
- **Message passing (MP):** T1: `data=1; flag=1` / T2: `r1=flag;
  r2=data`. Can `r1==1 && r2==0`? x86: no (TSO). ARM relaxed: **yes** —
  run the bench, see nonzero counts, feel the model. With release/
  acquire: zero, guaranteed, everywhere.

Numbers to expect on an M-series chip: thousands of MP violations per
million iterations with relaxed; exactly zero with acquire/release.

### 4. Double-checked locking: the graduation exercise

Lazy-create a singleton without paying a mutex on every access:

```cpp
T* p = ptr.load(acquire);            // fast path: no lock
if (!p) {
    lock_guard g{m};
    p = ptr.load(relaxed);           // re-check under the lock
    if (!p) { p = new T(); ptr.store(p, release); }
}
return *p;
```

Why each ordering: the release store publishes the *fully constructed*
object; the acquire load ensures a fast-path reader sees the
construction, not just the pointer. This pattern, broken (plain pointer,
no atomics), shipped in every Java and C++ codebase of the 2000s and is
still a top-five interview question. (C++11's static locals do this for
you — `static T t;` is thread-safe — but you must be able to build it by
hand and say why each barrier exists.)

## Assignment spec

`include/course/ordering.hpp` (header-only). The skeletons have the
*algorithms* stubbed AND every ordering set to `relaxed` with `// TODO`
— choosing the correct minimal orderings is the assignment:

- **`MailBox`** — single-producer/single-consumer, single-slot:
  `bool try_post(std::int64_t payload)` (false while the box is full;
  otherwise write payload, then publish via flag),
  `std::optional<std::int64_t> take()` (nullopt unless published;
  consumes the publication, freeing the slot for the next try_post).
  Orderings: THEORY §2.
- **`LazyBox<T>`** — `T& get_or_create(auto&&... args)` double-checked
  locking exactly as THEORY §4; `created()` count for the tests (must be
  exactly 1 under any concurrency).
- **`RelaxedCounter`** — `add(n)`, `total()`: the one legitimately
  relaxed structure here (per-thread event counting; no ordering
  needed). It exists so you practice *arguing why relaxed is enough*,
  in a comment you write above it.

**Acceptance criteria:** `m07a02.*` green in `debug`, `asan`, and
**`tsan`** — tsan WILL flag a relaxed MailBox as a race on the payload;
that's the spec working. Then run the litmus bench on your Mac and
record the MP-violation counts (relaxed vs acq/rel) in a comment in the
header.

## Hints

<details><summary>Hint 1 — MailBox members</summary>
<code>std::atomic&lt;bool&gt; full_{false};</code> and a plain (NOT
atomic) <code>std::int64_t payload_;</code>. The payload being plain is
the point: the FLAG's ordering is what makes the payload handoff legal.
Making the payload atomic too would hide your ordering mistake from
tsan — and from your understanding.
</details>

<details><summary>Hint 2 — take() consumes</summary>
Acquire-load the flag; if set: read payload, then
<code>full_.store(false, release)</code> so the producer may post again
(that store publishes "the slot is yours" — same pattern, other
direction). If your SPSC stress test loses values, the consume step's
ordering is wrong or missing.
</details>

<details><summary>Hint 3 — LazyBox's second load really is relaxed</summary>
Under the mutex, the mutex's own acquire/release already orders you
against the other locker's store. The re-check only needs atomicity.
Saying THIS sentence out loud is the interview answer.
</details>

## Further reading

- *C++ Concurrency in Action* 2e, ch. 5 — read §5.3 twice.
- Jeff Preshing: *"Acquire and Release Semantics"*, *"Memory Barriers
  Are Like Source Control Operations"* — the best informal explanations
  in existence.
- Herb Sutter, *"atomic<> Weapons"* (C++ and Beyond 2012, 2 parts) —
  the deep dive; part 1 before Module 8.
- [Diy/litmus7 tool suite](https://diy.inria.fr/) — real litmus testing;
  browse the ARM catalogue you just reproduced by hand.
