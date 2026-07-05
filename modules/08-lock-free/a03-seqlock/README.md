# a03 — Seqlock: Snapshots Without Stopping the Writer

**Estimated effort:** 6–8 hours.

## Learning objectives

- Implement a sequence lock: optimistic, retrying readers over a single
  never-blocked writer.
- Place memory fences deliberately — the first code in this course where
  standalone `atomic_thread_fence` is the right tool.
- Know the failure modes (torn reads, reader starvation) and the exact
  workload where a seqlock crushes every alternative.

## THEORY

### 1. Read-mostly, tiny, hot: the market-data snapshot problem

The pattern this exists for: ONE thread updates the "current top of
book" (a few dozen bytes) millions of times a second; MANY strategy
threads read it. A mutex serializes the readers *and* the writer; an
RW lock makes every reader WRITE the shared lock word (m08 README bank
— the coherence hotspot). The seqlock inverts the deal:

- The writer **never waits. For anyone. Ever.** It bumps a sequence
  counter to odd (meaning: under construction), writes the payload,
  bumps back to even.
- Readers are **optimists**: read seq; if odd, a write is in flight —
  spin. If even, copy the payload out, then read seq *again*. Same
  value → the copy is a consistent snapshot; changed → a write raced
  us, throw the copy away and retry.

Readers do zero writes to shared state — they scale without limit.
The price: a reader can retry indefinitely under a write storm
(starvation is real; measure your write rate), and the payload must be
small + trivially copyable (you're copying it every read).

### 2. Why the naive version is broken, and the fence choreography

The subtle part isn't the counter — it's keeping the *payload* accesses
from drifting across the counter accesses. Plain relaxed word copies
could be hoisted above the first seq read or sunk below the second —
validating a snapshot you haven't finished taking. The correct shape:

```text
writer:  seq.store(s+1, relaxed)          reader:  s1 = seq.load(acquire)
         atomic_thread_fence(release)              if s1 odd: retry
         write payload words (relaxed)             copy payload words (relaxed)
         seq.store(s+2, release)                   atomic_thread_fence(acquire)
                                                   s2 = seq.load(relaxed)
                                                   if s1 != s2: retry
```

Walk it: the writer's release *fence* orders the odd-store before the
payload writes (a release *store* alone wouldn't order what comes
AFTER it); the final release store publishes the payload before the
even value. The reader's acquire load pins the copies below `s1`; the
acquire *fence* pins them above `s2`. Every arrow accounted for — this
is the assignment where fences stop being folklore.

One more honesty note: the classic C-style seqlock reads the payload
as plain memory — a formal data race (UB) that engineers wave at with
"benign". This course's version stores the payload as **an array of
relaxed atomic words** instead: same machine code on ARM and x86, zero
UB, tsan-clean. (P1478's `atomic_load_per_byte_memcpy` is the future
standard fix; know the name.)

### 3. The invariant the tests hunt

A snapshot must be *all one write*: if the writer stores
`{x, x+1, x+2, x+3}` and a reader ever returns `{5, 6, 7, 9}` — a mix
of two writes — the seqlock is broken (usually a missing fence, visible
on your ARM Mac and invisible on x86). The stress test does exactly
this, with both threads deadline-bounded.

## Assignment spec

`include/course/seqlock.hpp` (header-only):

`SeqLock<T>` — `T` trivially copyable, `sizeof(T)` a multiple of 8
(both `static_assert`ed, given). Single writer, any number of readers.

- `void store(const T& value)` — writer only; wait-free (no loops).
- `T load() const` — readers; retries until it wins a consistent
  snapshot.
- `std::uint64_t version() const` — the current (even) sequence /2 …
  i.e. how many stores have completed; given as spec: `store` bumps it
  by exactly one.

Storage (given in the skeleton): `std::array<std::atomic<std::uint64_t>,
sizeof(T)/8>` + memcpy-in/out of a stack `T` — you write the protocol,
not a serializer.

**Acceptance criteria:** `m08a03.*` green in `debug`, `asan`, and
`tsan` — tsan-clean is non-negotiable here; it's the whole point of the
atomic-words design.

**Benchmark:** `m08a03_bench` (release, complete harness): read cost
with a quiet writer vs a 1MHz writer vs mutex-protected struct at 1/2/4
reader threads. Expect: seqlock reads ~10-30ns flat as readers scale;
mutex reads degrading with reader count (the lock word ping-pong);
seqlock under write storm showing retries (higher mean, watch p99).

## Hints

<details><summary>Hint 1 — store(), five lines</summary>
Load seq (relaxed — you're the only writer), store seq+1 relaxed,
<code>std::atomic_thread_fence(std::memory_order_release)</code>, memcpy
the T into a local word array then store each word relaxed (or memcpy
via a loop of relaxed stores), store seq+2 release. The only writer
means no CAS anywhere.
</details>

<details><summary>Hint 2 — load(), and the retry shape</summary>
Loop: s1 = seq.load(acquire); if odd, continue; copy words out
(relaxed loads into a local array, memcpy into a T);
<code>atomic_thread_fence(acquire)</code>; s2 = seq.load(relaxed); if
s1 == s2 return the T. If the torn-read stress fails on your Mac but
passes here on x86 — you dropped one of the two fences; THEORY §2 has
the arrow diagram.
</details>

<details><summary>Hint 3 — version()</summary>
The completed-store count is just <code>seq/2</code> for the last EVEN
value you observe (acquire). If a reader can call it mid-write, decide
what "current" means and document it — the test only checks it from
quiet states.
</details>

## Further reading

- Hans Boehm, ["Can seqlocks get along with programming language memory
  models?"](https://dl.acm.org/doi/10.1145/2247684.2247688) (MSPC 2012)
  — THE paper on exactly your fence choreography.
- [P1478: Byte-wise atomic memcpy](https://wg21.link/p1478) — the
  standard's coming answer to this assignment's atomic-words workaround.
- Linux kernel `Documentation/locking/seqlock.rst` — the original,
  30 years in production.
- *C++ Concurrency in Action* 2e, §5.3.5–5.3.6 (fences).
