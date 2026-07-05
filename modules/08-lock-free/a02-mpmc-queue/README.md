# a02 — MPMC Queue: Vyukov's Bounded Queue

**Estimated effort:** 8–12 hours. The hardest single data structure in
the course. Read THEORY until the algorithm feels inevitable, then
implement from understanding — not from a reference tab.

## Learning objectives

- Implement Dmitry Vyukov's bounded MPMC queue: per-cell sequence
  numbers coordinating competing producers and consumers.
- Use compare-exchange correctly (weak vs strong, the retry idiom).
- Explain ABA precisely and how sequence numbers make it structurally
  impossible here.

## THEORY

### 1. Why the SPSC trick stops working

With multiple producers, two threads can both believe slot `tail & mask`
is theirs. Some arbitration must pick ONE winner per slot — that's a
CAS (`compare_exchange`): "advance `tail_` from t to t+1 *iff it's
still t*". Losers retry with the new value. This is why MPMC is only
lock-free, not wait-free: an unlucky thread can lose CAS races forever
while others make progress.

But claiming the slot is only half the problem. The winner still has to
*write* the value — and a consumer arriving early must not read a
claimed-but-unwritten slot. The cursors alone can't express "claimed
but not ready". Each slot needs its own state.

### 2. The idea that makes it work: per-cell sequence numbers

Every cell carries `std::atomic<uint64> seq`, and the numbers encode a
rendezvous protocol between exactly one producer turn and one consumer
turn per lap:

```text
cell[i].seq == E   (enqueue ticket)  : empty, awaiting producer with tail==E
cell[i].seq == E+1                    : full, awaiting consumer with head==E
cell[i].seq == E+N (next lap's ticket): empty again, awaiting tail==E+N
```

Initially `cell[i].seq = i`. The dance:

```text
enqueue:                              dequeue:
  t = tail.load(relaxed)                h = head.load(relaxed)
  cell = &cells[t & mask]               cell = &cells[h & mask]
  s = cell->seq.load(acquire)           s = cell->seq.load(acquire)
  if s == t:                            if s == h + 1:
    if tail.CAS_weak(t, t+1, relaxed):    if head.CAS_weak(h, h+1, relaxed):
      cell->value = move(v)                 out = move(cell->value)
      cell->seq.store(t+1, release)         cell->seq.store(h+N, release)
      done                                  done
  elif s < t:  queue FULL               elif s < h + 1:  queue EMPTY
  else: retry (someone else won)        else: retry
```

Read it until each line answers a question: the acquire on `seq` pairs
with the release on the other side's `seq` store (the value handoff —
your MailBox again, one per cell). The CAS only *claims a ticket*; the
`seq` store is what *publishes the data*. A consumer that arrives at a
claimed-but-unwritten cell sees `seq == t`, not `t+1` — "not ready" —
and reports empty or retries. No thread ever waits on another's
critical section: if the winner stalls before its seq store, only that
one cell is stuck; everyone else proceeds around the ring. (Almost —
a full lap later, producers need that cell. Bounded queues bound your
grace.)

### 3. Why ABA can't happen here — and where it would

The naive lock-free structure (Treiber stack: CAS the head *pointer*)
breaks when memory recycles: pointer value A, freed, reallocated, CAS
sees "still A", swings — corruption (module README bank). Vyukov's
design CASes *tickets* — monotonically increasing integers that never
repeat — and validates each cell with `seq`, which also only counts
up. There is no value that can recur, so there is no ABA to defend
against. This is the generation-counter mitigation *built into the
algorithm* rather than bolted on. Say that sentence in an interview
and you're done with the ABA question.

`compare_exchange_weak` vs `strong`: weak may fail spuriously (no
reason) but is cheaper on ARM (LL/SC architectures); in a retry loop
you're looping anyway — always use weak in loops. Strong is for
one-shot attempts. Also note the failure ordering parameter defaults
sanely; explicit `relaxed` for the failure case is idiomatic here.

## Assignment spec

`include/course/mpmc_queue.hpp` (header-only):

`MpmcQueue<T>` — bounded, any number of producers/consumers:

- `explicit MpmcQueue(std::size_t capacity)` — capacity must be a power
  of two ≥ 2 (given check throws `std::invalid_argument`). Cell array
  allocated once; `seq` initialized to the cell's index (given).
- `bool try_push(T value)` — false when full (no blocking, no spinning
  beyond the algorithm's own retries).
- `std::optional<T> try_pop()` — nullopt when empty.
- `capacity()`, approximate `size()` (single-threaded exactness only).
- Move-only payloads work.
- Cursors padded (same `sizeof`-pinned deal as a01, both cursors AND
  the cell array pointer region).

**Acceptance criteria:** `m08a02.*` green in `debug`, `asan`, and
`tsan`. The stress tests: 4×4 threads, conservation of count and sum,
AND per-producer order preservation (each producer tags its values;
every consumer's view of any single producer's values must be
ascending — the queue is FIFO per producer by construction).

**Benchmark:** `m08a02_bench` (release, complete harness): throughput
at 1P/1C, 2P/2C, 4P/4C vs the mutex+`std::queue` baseline included in
the bench. Expect the mutex queue to win at 1P/1C (uncontended locks
are cheap!) and lose progressively at 2P/2C and 4P/4C — write ONE
sentence in the header comment about why that crossover is the whole
lock-free story.

## Hints

<details><summary>Hint 1 — the Cell and the members</summary>
<code>struct Cell { std::atomic&lt;std::uint64_t&gt; seq; T value; };</code>
+ <code>std::unique_ptr&lt;Cell[]&gt;</code> (you know why unique_ptr
now), mask, and two padded cursors. Constructor seeds
<code>cells[i].seq = i</code> — already given in the skeleton.
</details>

<details><summary>Hint 2 — transcribe THEORY §2 honestly</summary>
The pseudocode is complete; the bugs come from "simplifying" it. The
three-way branch on <code>s</code> vs the ticket matters: collapsing
"someone won, retry" into "full/empty" gives false fulls under
contention (a stress test counts unexpected refusals and will notice a
LOT of them).
</details>

<details><summary>Hint 3 — tsan points at cell->value</summary>
Then a seq load/store ordering is wrong: the acquire on seq is what
licenses touching value; the release on seq is what publishes it.
Every access to <code>value</code> must be sandwiched between its
acquire and its release — check both sides of both operations.
</details>

## Further reading

- Dmitry Vyukov, [Bounded MPMC queue](https://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue)
  — the primary source. Read AFTER your first honest attempt.
- *C++ Concurrency in Action* 2e, ch. 7 (whole chapter — this is its
  final boss).
- Maged Michael, *Hazard Pointers* (IEEE TPDS 2004) — what unbounded
  lock-free structures need that this bounded one dodges.
