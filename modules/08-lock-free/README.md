# Module 8 — Concurrency II: Lock-Free

**Time budget:** ~2 weeks. Three assignments — the hardest code of the
course before the capstones. Everything converges here: placement
storage (M2/M5), layout and padding (M3/M7), acquire/release (M7).

Why lock-free at all? Not raw speed — an uncontended mutex is ~20ns and
perfectly fine. Three reasons trading systems pay the complexity tax:
**tail latency** (a lock holder descheduled at the wrong moment stalls
everyone behind it; lock-free progress can't be held hostage by a
sleeping thread), **progress guarantees** under contention, and — for
SPSC especially — the structure is simply *faster than any lock could
be* because each side owns its half outright.

The vocabulary, precisely (interviews test the definitions):
**obstruction-free** (a thread alone makes progress), **lock-free**
(SOME thread always makes progress; individual threads may starve),
**wait-free** (EVERY thread finishes in bounded steps). Your a01 SPSC
ops are wait-free; a02's MPMC is lock-free; a03's seqlock is
wait-free for the writer, lock-free-ish for readers.

**tsan is still mandatory. So is the deadline discipline:** every stress
test in this module is bounded — a broken algorithm fails in seconds,
never hangs.

## Assignments

| # | Assignment | What you'll touch |
|---|------------|-------------------|
| 1 | [a01-spsc-ring-buffer](a01-spsc-ring-buffer/) | THE HFT queue: wait-free SPSC, monotonic indices, padded cursors |
| 2 | [a02-mpmc-queue](a02-mpmc-queue/) | Vyukov's bounded MPMC: per-cell sequence numbers, CAS, ABA |
| 3 | [a03-seqlock](a03-seqlock/) | read-mostly snapshots: sequence validation, torn-read protection |

## Interview questions

<details>
<summary>"Why is SPSC so much easier than MPMC?"</summary>

Ownership. In SPSC the producer is the only writer of `tail` and the
only reader that matters of `head` (and vice versa) — each cursor has
one writer, so plain release/acquire suffices and every operation is
wait-free: no CAS, no retries, no contention on the cursors. MPMC has
multiple writers *competing* for the same slot, which forces CAS loops
(threads can fail and retry — only lock-free) and opens the ABA door.
One-line version: SPSC is a memory-ordering problem; MPMC is a
consensus problem.
</details>

<details>
<summary>"Explain ABA and two real mitigations."</summary>

A CAS checks the VALUE, not the history: thread 1 reads pointer A,
stalls; meanwhile A is popped, freed, its memory recycled (hello, m05a02
pool) and pushed back as logical node A again; thread 1's CAS succeeds
against a world it misread — corrupting the structure. Mitigations:
(1) tag bits / generation counters packed with the pointer (CAS on
pointer+counter — the counter changes even when the address repeats);
(2) Vyukov's per-cell sequence numbers, which are that idea built into
the queue; (3) defer reclamation so addresses can't recycle while
anyone holds them — epochs/hazard pointers/RCU. Java dodges via GC;
C++ makes you choose.
</details>

<details>
<summary>"Seqlock vs reader-writer lock — when and why?"</summary>

RW locks make readers WRITE (the reader count / lock word) — under many
readers that word ping-pongs between cores and the "read-only" path
becomes a coherence hotspot. A seqlock's readers touch no shared
mutable state at all: read seq, read data, re-check seq, retry if a
write intervened. Readers scale perfectly; the single writer never
waits for them. The trade: readers can retry (unbounded under a write
storm), data must be small and trivially copyable (you copy it out),
and there's exactly one writer. Market-data snapshot = the canonical
fit: written once per tick, read by many strategies.
</details>

<details>
<summary>"Your lock-free queue benchmarks 10x faster than the mutex one. Ship it?"</summary>

The senior answer: which percentile, which contention level, which
pattern? Microbenchmark ping-pong flatters lock-free; real workloads
have bursts, and a mutex queue's WORST case (holder descheduled) is
what kills you, not its mean. Also: is it correct under tsan, on weak
memory (ARM), with move-only payloads, at wraparound, when full? The
answer they want is a testing story plus "mean is marketing, tails are
production".
</details>
