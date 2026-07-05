# Module 6 — Hardware & Benchmarking

**Time budget:** ~2 weeks. Three assignments.

Everything so far claimed things were fast. This module is where you stop
claiming and start *measuring* — correctly, which is harder than it looks —
and where you learn the two hardware mechanisms that explain 90% of
single-threaded performance mysteries: the cache hierarchy and the branch
predictor.

1. **a01** — measurement itself: benchmark pitfalls (dead-code
   elimination, warm-up, variance, coordinated omission) and the
   **latency histogram** you'll bolt onto both capstones;
2. **a02** — the cache lab: watch your own machine's L1/L2 cliffs appear
   in a working-set sweep, and see why "arrays beat lists" is physics;
3. **a03** — the branch lab: mispredictions, branchless idioms, and the
   famous "why is the sorted array faster?" — answered with assembly.

**A note on your hardware:** Apple Silicon differs from the x86 servers
HFT runs on in ways this module makes explicit — cache lines are 128
bytes (x86: 64), there's no L3 (a big shared SLC instead), and the
performance/efficiency core split makes pinning matter. Each handout has
a Linux/x86 sidebar with the `perf` commands you'd use there; learn both
vocabularies.

## Assignments

| # | Assignment | What you'll touch |
|---|------------|-------------------|
| 1 | [a01-benchmark-methodology](a01-benchmark-methodology/) | DCE, DoNotOptimize, percentiles, the LatencyHistogram |
| 2 | [a02-cache-lab](a02-cache-lab/) | working-set sweeps, stride effects, pointer chasing |
| 3 | [a03-branch-lab](a03-branch-lab/) | misprediction, branchless idioms, csel/cmov |

## Interview questions

<details>
<summary>"Latency numbers every programmer should know — go."</summary>

Order of magnitude, 2020s hardware: L1 hit ~1ns (3–4 cycles), L2 ~3–5ns,
LLC ~10–20ns, DRAM ~60–100ns, NVMe read ~10–100µs, same-DC network RTT
~50–500µs, disk seek ~ms. The deltas are the point: DRAM is ~100× L1, so
one cache miss can cost more than a hundred arithmetic ops. Branch
mispredict ~15–20 cycles. Mutex lock/unlock uncontended ~15–25ns.
Syscall ~100ns–1µs. Know them cold; every systems interview touches this.
</details>

<details>
<summary>"What actually happens on a cache miss?"</summary>

The load checks L1 (miss), L2 (miss), LLC (miss), then a memory
controller queues a DRAM transaction that returns a full cache line
(64B x86 / 128B Apple Silicon), which is installed up the hierarchy,
evicting victims per the replacement policy. The core meanwhile tries to
keep executing out-of-order — a modern core can have dozens of misses in
flight — which is why *dependent* (pointer-chasing) misses are so much
worse than independent ones: they serialize the round trips.
</details>

<details>
<summary>"Why does sorting the input make this branchy loop 5× faster?"</summary>

The branch `if (x > threshold)` on random data is unpredictable: ~50%
mispredicts, ~15–20 cycles of flushed pipeline each. Sorted input makes
the branch outcome constant for long runs — the predictor locks on and
the penalty vanishes. Fixes for the random case: branchless arithmetic
(`sum += (x > t)`), which compiles to csel/cmov and can't mispredict; or
don't have the branch at all (partition, SoA + masks, SIMD).
</details>

<details>
<summary>"Why do latency-sensitive systems report p99/p99.9 instead of the mean?"</summary>

The mean hides tails, and tails are where the money dies: one 100µs GC-ish
stall in a million 1µs operations barely moves the mean but is exactly the
event that misses the market. Also: latencies aren't normally distributed
(multi-modal: cache-hit path vs miss path vs page-fault path), so
mean±stddev describes a distribution that doesn't exist. Histograms
(HdrHistogram-style log buckets) capture the whole shape cheaply.
</details>

<details>
<summary>"What is coordinated omission?"</summary>

The classic benchmark lie: you issue request, wait for reply, issue next —
so when the system stalls, you *stop sending load*, and the stall shows up
as ONE slow sample instead of the hundreds that would have queued behind
it in production. Fix: measure against the intended send schedule
(record latency from when the request *should* have been sent), or use a
load generator that doesn't back off. Gil Tene's talk is the canonical
reference — and quoting it by name is interview shorthand for "I've been
burned by real benchmarks".
</details>
