# a01 — Benchmark Methodology & the Latency Histogram

**Estimated effort:** 5–7 hours (plus the reading — don't skip it here).

## Learning objectives

- Name and defeat the four classic benchmark lies: dead-code elimination,
  cold-start, frequency/thermal drift, and coordinated omission.
- Read a Google Benchmark report critically (iterations, repetitions,
  CV, aggregates).
- Implement the log-bucketed **LatencyHistogram** that both capstones
  will use to report tick-to-trade percentiles.

## THEORY

### 1. The compiler is trying to delete your benchmark

`-O2` exists to remove work whose result isn't observed. A benchmark is
*precisely* work whose result isn't observed:

```cpp
for (auto _ : state) {
    auto r = parse_price("155.10");   // result unused...
}                                      // ...so -O2 deletes the call. 0.3ns!
```

Sub-nanosecond results mean the optimizer won: nothing that touches memory
finishes in 0.3ns. The antidotes: `benchmark::DoNotOptimize(r)` (compels
the compiler to consider `r` used — it "escapes" the value) and
`benchmark::ClobberMemory()` (compels it to consider all memory
read/written — flushes pending stores). The bench harness in this
assignment shows the same loop measured wrong and right; run it first and
look at the gap. Rule for the rest of the course: **any benchmark number
under ~1ns is a bug in the benchmark until proven otherwise.**

### 2. Warm-up, drift, and the shape of noise

First iterations pay cold caches, cold branch predictors, page faults on
first-touch, and (on your M-series Mac) possible scheduling on an
efficiency core before the scheduler notices you're hot. Google Benchmark
runs until timings stabilize, which handles most of it — but *you* handle:
closing the browser, plugging in power, and using
`--benchmark_repetitions=10 --benchmark_report_aggregates_only=true` for
anything you'll quote. The number to watch is the CV (coefficient of
variation): >2–3% on a microbenchmark means the environment, not the code,
is talking.

> **Linux/x86 sidebar:** on a server you'd also pin the benchmark to a
> core (`taskset -c 3`), disable turbo/frequency scaling (`cpupower
> frequency-set -g performance`), and isolate the core from the scheduler
> (`isolcpus`). macOS gives you none of these knobs — one more reason
> production benchmarking happens on production-like Linux.

### 3. Percentiles, histograms, and why log buckets

Means lie by omission (module README question bank). Trading systems
report p50/p99/p99.9/max, and computing percentiles exactly means storing
every sample — at millions of events/sec, no. The standard trick
(HdrHistogram): bucket by order of magnitude. This assignment's spec —
**bucket i holds values whose bit-width is i** (bucket 0: value 0;
bucket 1: 1; bucket 2: 2–3; bucket 3: 4–7; … bucket 63 caps out) — gives
64 counters covering 0 to int64-max with ≤2× relative error, O(1) record
(one `bit_width`, one increment), O(64) percentile queries, and trivial
merging across threads (add the arrays). ≤2× error on a latency is
nothing: the difference between 800ns and 1.3µs is a rounding error next
to the difference between 1µs and the 40µs outlier you're hunting.

Percentile semantics, pinned by the tests: `percentile(p)` returns the
**upper bound** of the bucket containing the sample of rank
`ceil(p/100 · count)` (rank 1 = smallest). `min()`/`max()` are tracked
exactly on the side — the tails are too important to round.

### 4. Coordinated omission (read this twice)

If you measure a loop of `start = now(); op(); record(now() - start)`,
a 10ms stall in `op()` records as ONE bad sample — but in production,
requests would have kept *arriving* during those 10ms, and every one of
them would have eaten the stall. Your benchmark under-reports the tail by
exactly the factor that matters. When the capstone measures tick-to-trade,
you'll timestamp from the packet's *arrival*, not from when the handler
got around to reading it — that's the fix in its natural habitat. For now:
know the name, spot it in others' benchmarks, expect it in interviews.

## Assignment spec

`include/course/latency_histogram.hpp` (header-only):

- `void record(std::int64_t ns)` — O(1); negatives clamp to bucket 0.
- `std::int64_t count()`, `std::int64_t min()`, `std::int64_t max()` —
  min/max exact; 0/0/0 when empty.
- `std::int64_t percentile(double p)` — THEORY §3 semantics; `p` in
  (0, 100]; returns 0 when empty. `percentile(100)` == the max bucket's
  upper bound (not the exact max — the tests distinguish deliberately).
- `void merge(const LatencyHistogram& other)` — counters add, min/max
  fold; the per-thread aggregation story.
- `class TimedSection` — RAII: records elapsed `steady_clock` ns into a
  histogram at scope exit (your m01a03 ScopeTimer, now with a purpose).

**Acceptance criteria:** all `m06a01.*` tests pass in `debug` and `asan`.

**Benchmark (run it BEFORE implementing — it's a lesson, not a
harness):** `m06a01_bench` shows: (1) a DCE'd loop vs `DoNotOptimize` —
the wrong version reports ~0.3ns, the right one ~5–15ns; (2) record()
cost — target: **≤5ns** per record at -O2, because a histogram that
costs more than the thing it measures is an observer effect with
delusions; (3) the same benchmark with and without repetitions, to see
run-to-run spread. Quote your record() number in a comment in the header
when green.

## Hints

<details><summary>Hint 1 — the bucket index is one intrinsic</summary>
<code>std::bit_width(static_cast&lt;std::uint64_t&gt;(ns))</code>
(&lt;bit&gt;, C++20) is exactly "index of the highest set bit + 1" —
value 0 → 0, 1 → 1, 2–3 → 2, 4–7 → 3… matching the spec's buckets
directly. Bucket upper bound: <code>(1LL &lt;&lt; i) - 1</code> for
i ≥ 1.
</details>

<details><summary>Hint 2 — percentile rank walk</summary>
<code>rank = ceil(p / 100.0 * count)</code> — use integer-safe ceiling:
<code>(p * count + 99...) </code> is fiddly with doubles; simplest correct:
<code>rank = static_cast&lt;int64&gt;(std::ceil(p / 100.0 * count))</code>,
clamp to ≥1. Then walk buckets accumulating counts until cumulative ≥
rank; return that bucket's upper bound.
</details>

<details><summary>Hint 3 — record() is too slow in the bench</summary>
The hot path must be: clamp, bit_width, increment, two comparisons for
min/max. No branches on bucket count, no doubles, no function calls. If
you see 20ns, look for an accidental modulo or a virtual-ish indirection;
if 0.5ns, the compiler deleted it — where's your DoNotOptimize?
</details>

## Further reading

- Gil Tene, *"How NOT to Measure Latency"* (Strange Loop 2015) — THE
  coordinated-omission talk. Required.
- [HdrHistogram](http://hdrhistogram.org/) — the design yours is a
  64-bucket sketch of.
- [Google Benchmark user guide](https://github.com/google/benchmark/blob/main/docs/user_guide.md)
  — the flags and aggregates §2 leans on.
- Chandler Carruth, *"Tuning C++: Benchmarks, and CPUs, and Compilers!
  Oh My!"* (CppCon 2015) — DoNotOptimize from its author.
