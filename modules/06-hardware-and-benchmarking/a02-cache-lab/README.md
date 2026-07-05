# a02 — Cache Lab: Watching the Hierarchy Appear

**Estimated effort:** 5–7 hours, mostly running and interpreting sweeps.

## Learning objectives

- Implement four access-pattern kernels and predict their relative speed
  *before* running them.
- Read a working-set sweep: find your machine's L1/L2/SLC boundaries in
  the data.
- Explain hardware prefetching, why it loves sequences and dies on
  pointer chases, and what that means for data structures.

## THEORY

### 1. The numbers under everything

A load that hits L1 costs ~1ns. A load that goes to DRAM costs ~100ns.
Everything else in this module is corollary. Your M-series Mac, concretely:
128-byte cache lines (x86: 64 — check yours: `sysctl hw.cachelinesize`),
~128KB L1d per performance core, a big shared L2 (check: `sysctl
hw.perflevel0.l2cachesize`), then a system-level cache (SLC) in front of
DRAM. Loads move whole lines: touch one byte, pay for 128.

Two kinds of locality make caches work: **spatial** (you'll touch the
neighbor next — arrays) and **temporal** (you'll touch this again soon —
hot working sets). Every data-structure argument you'll ever have in a
trading-systems interview is secretly about these two words.

### 2. The prefetcher: free speed for the predictable

The memory system watches your misses. Sequential or constant-stride
streams get detected after a few accesses, and the prefetcher starts
pulling lines *before you ask* — a sequential scan of DRAM-resident data
can run near memory bandwidth, hundreds of GB/s on your machine, hiding
nearly all of the 100ns latency. What defeats it: dependent loads. A
linked-list hop can't even be *issued* until the previous node arrives —
`p = p->next` serializes the full memory latency, every hop. That's the
entire "vector vs list" religious war settled in one sentence: same
big-O, ~100× different constant when the list's nodes are cold and
scattered.

Your four kernels make this quantitative:

| kernel | pattern | what bounds it |
|--------|---------|----------------|
| `sum_sequential` | dense scan | bandwidth (prefetcher wins) |
| `sum_strided` | every k-th element | line utilization: stride ≥ line/8 wastes the rest of each line |
| `sum_gather` | data[idx[i]], random idx | latency, but *independent* misses overlap |
| `chase` | p = p->next | latency, *dependent* — nothing overlaps; the worst case |

The gather-vs-chase gap is the subtle one: both are "random access", but
the out-of-order core keeps a dozen independent gathers in flight while
the chase is one at a time. Independent-random vs dependent-random is a
distinction most candidates don't know exists.

### 3. Reading a working-set sweep

The bench sweeps buffer sizes from 16KB to 64MB. Plot ns/element (or just
read the table): flat while the set fits in L1, step up when it spills to
L2, again at SLC/DRAM. The random kernels step *hard*; sequential barely
moves (prefetch). You are literally reading your cache sizes out of
timing data — do the sweep once on the Mac and remember the shape; on any
new machine (or in any interview whiteboard moment), you can re-derive
the hierarchy from first principles.

> **Linux/x86 sidebar:** `perf stat -e cache-misses,cache-references,
> L1-dcache-load-misses ./bench` attributes misses directly;
> `perf c2c` finds contended lines (relevant in Module 7). On macOS,
> Instruments' "CPU Counters" template exposes a subset; the sweep
> technique above needs no counters at all — which is why we teach it.

## Assignment spec

`include/course/kernels.hpp` + `src/kernels.cpp`:

- `std::int64_t sum_sequential(const std::int64_t* data, std::size_t n)`.
- `std::int64_t sum_strided(const std::int64_t* data, std::size_t n,
  std::size_t stride)` — sum of elements at indices 0, stride, 2·stride, …
  (< n). Precondition: stride ≥ 1.
- `std::int64_t sum_gather(const std::int64_t* data,
  const std::uint32_t* idx, std::size_t n)` — Σ data[idx[i]].
- `Node` (given: `{ std::int64_t value; Node* next; }`) and
  `std::int64_t chase(const Node* head)` — walk to null, summing values.

All four are memory-pattern kernels: single loop, no cleverness — the
pattern IS the assignment. The tests check pure correctness (all kernels
agree on small inputs); the *benchmark* is where the learning happens.

**Acceptance criteria:** `m06a02.*` tests green in `debug` and `asan`;
then run the bench and **write the sweep table into a comment in
`kernels.cpp`** with one sentence marking where you believe L1 and L2 end.

**Benchmark:** `m06a02_bench` (release, complete harness): (1) working-set
sweep 16KB→64MB for sequential vs gather vs chase; (2) stride sweep 1, 2,
4, 8, 16, 32 at fixed 32MB. What good looks like on an M-chip: sequential
nearly flat throughout (~0.2–0.5 ns/elem); gather stepping to ~5–15
ns/elem at DRAM; chase stepping to ~60–100 ns/hop; strides ≥16 (128B/8B)
converging on one-line-per-element cost.

## Hints

<details><summary>Hint 1 — these are five-line functions</summary>
Truly. A plain indexed loop each. If you're tempted to unroll or
prefetch manually, resist — the point is measuring what PLAIN code does;
optimizations come after measurement, and here the compiler's -O2 output
IS the subject.
</details>

<details><summary>Hint 2 — the sweep looks flat everywhere</summary>
You're probably compiling the bench in debug (numbers 10x too slow hide
the steps) or your buffer never leaves cache (check the sweep actually
reaches 64MB). Also close Chrome: SLC is shared and a busy system
flattens the upper steps.
</details>

## Further reading

- Ulrich Drepper, *What Every Programmer Should Know About Memory* —
  §3 (caches) and §6.2 (access patterns). The module's spine.
- [Latency numbers every programmer should know](https://colin-scott.github.io/personal_website/research/interactive_latency.html)
  — interactive, year-adjustable.
- Scott Meyers, *"CPU Caches and Why You Care"* (code::dive 2014) — the
  best single talk on this material.
- Apple, *Apple Silicon CPU Optimization Guide* — the M-series specifics
  (cache geometry, prefetchers) from the source.
