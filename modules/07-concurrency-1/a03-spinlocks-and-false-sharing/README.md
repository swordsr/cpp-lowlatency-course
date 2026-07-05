# a03 — Spinlocks & False Sharing: Coherence Is the Cost

**Estimated effort:** 5–7 hours.

## Learning objectives

- Implement TAS, TTAS, and backoff spinlocks and explain their
  cache-coherence traffic differences, not just their code differences.
- Give lock() acquire semantics and unlock() release semantics and say
  why those two words make the critical section work.
- Demonstrate false sharing, then fix it with alignment — and know the
  interference-size numbers for both your Mac and x86.

## THEORY

### 1. MESI in four sentences

Every cache line is in one of four states per core: **M**odified (mine,
dirty), **E**xclusive (mine, clean), **S**hared (read-only copies may
exist), **I**nvalid. To *write* a line, a core must own it (M/E), which
means broadcasting an invalidation and yanking it from every other cache
— a Read-For-Ownership. Cross-core line transfers cost ~20–60ns. All
multicore performance pathology — lock contention, false sharing,
ping-ponging counters — is this one protocol doing its job on a bad
access pattern.

### 2. TAS → TTAS → backoff: the same lock, decreasing rudeness

**TAS** (test-and-set): spin on `exchange(true, acquire)`. Every attempt
is a WRITE — every spinner RFOs the line every iteration, so the line
ping-pongs between N waiting cores, saturating the interconnect and —
the ugly part — slowing down the *owner* trying to unlock through the
same line.

**TTAS** (test-and-test-and-set): spin on a plain relaxed *load* until
the lock looks free, then attempt one exchange. Waiting spinners share
the line in S state — local L1 hits, zero traffic — until unlock
invalidates once, and everyone races with a single exchange each.

**Backoff:** TTAS still stampedes on release (N exchanges, one winner).
Add exponential backoff between failed attempts: contention collapses,
at the cost of unlock-to-acquire latency for the unlucky. (Production
locks add a queue — MCS/CLH — so each spinner waits on its OWN line;
know the name for interviews, build it another day.)

The orderings, and why: `lock()` must be **acquire** (nothing from the
critical section may float above the point you got the lock),
`unlock()` must be **release** (nothing may sink below the point you
gave it up). Together they make the next owner see everything the last
owner did — the same release→acquire edge as a02, wearing a lock
costume. The spin *reads* in TTAS can be relaxed; only the successful
exchange needs acquire.

### 3. False sharing: contention you never wrote

```cpp
struct Counters {
    std::atomic<std::int64_t> thread_a;  // same 128-byte line...
    std::atomic<std::int64_t> thread_b;  // ...as this one
};
```

Thread A increments `thread_a`, thread B increments `thread_b` — no
shared data, yet each write RFOs the line and invalidates the other
core's copy. Both threads run at line-transfer speed: typically **5–20×
slower** than with separated lines. Invisible in source, invisible in
big-O; visible only in layout (Module 3 pays off) and measurement
(Module 6 pays off).

Fix: keep hot per-thread data at least one *destructive interference
size* apart — `alignas(kDestructiveInterference)` per member. That's
128 bytes on Apple Silicon, 64 on mainstream x86; the header provides
the constant with a feature-test fallback since libc++ has been slow to
ship `std::hardware_destructive_interference_size`. Cost: memory (one
line per counter, used or not). Interview nuance: its sibling
`constructive_interference_size` is the *keep together* number — data
used together should share a line; the two sizes bound opposite sins.

## Assignment spec

`include/course/spinlocks.hpp` (header-only):

- `TasSpinLock` — `lock()` / `unlock()` / `try_lock()`, pure
  exchange-spin. Meets *Lockable* so `std::lock_guard` works.
- `TtasSpinLock` — same interface, load-spin then exchange.
- `BackoffSpinLock` — TTAS + exponential backoff (given helper
  `cpu_pause()`; double the pause count each failure, cap it).
- `UnpaddedCounters` (given, the bug) and `PaddedCounters` — same
  two-counter interface (`add_a/add_b/total_a/total_b`), with the
  members you lay out so the counters live on different lines. Layout
  is pinned by an address-distance test.

**Acceptance criteria:** `m07a03.*` green in `debug`, `asan`, and
**`tsan`** (tsan on a mutual-exclusion test is the real referee: a
too-weak ordering in lock() is a race on the protected counter).

**Benchmark:** `m07a03_bench` (release, complete harness): (1) 4 threads
incrementing one shared counter under mutex vs TAS vs TTAS vs backoff —
expect mutex respectable, TAS worst under contention, TTAS/backoff
between; (2) false sharing: two threads hammering
UnpaddedCounters vs PaddedCounters — expect **5–20×**. Record both
matrices in a comment in the header; note the numbers are
core-topology-dependent (P-cores vs E-cores on your machine — pin
nothing and expect variance, another honest lesson).

## Hints

<details><summary>Hint 1 — TAS is three lines</summary>
<code>while (flag_.exchange(true, std::memory_order_acquire)) {}</code>
for lock; <code>flag_.store(false, std::memory_order_release);</code>
for unlock; try_lock is the exchange, negated, once.
</details>

<details><summary>Hint 2 — TTAS's inner spin</summary>
Outer loop: attempt the exchange once; if it fails, INNER loop on
<code>flag_.load(std::memory_order_relaxed)</code> until it reads false,
then retry the exchange. The relaxed load is deliberate — argue why in
a comment (the exchange's acquire covers you).
</details>

<details><summary>Hint 3 — the padding test fails at 8 bytes apart</summary>
<code>alignas(kDestructiveInterference)</code> on EACH member (aligning
just the struct aligns only the first). Verify:
<code>offsetof</code>-style distance ≥ the constant. Yes, the struct
gets fat — that's the price and the point.
</details>

## Further reading

- *C++ Concurrency in Action* 2e, §7.3 (spinning) + ch. 8 (designing for
  concurrency).
- Paul McKenney, *Is Parallel Programming Hard...* — ch. on locking;
  free PDF, the systems-side bible.
- [1024cores.net](https://www.1024cores.net) (Dmitry Vyukov) — the
  MPMC-queue author's notes; Module 8 lives here.
- Folly's `CacheLocality.h` — production interference-size handling.
