# a03 — Branch Lab: The Predictor Giveth and Taketh Away

**Estimated effort:** 4–6 hours.

## Learning objectives

- Explain pipeline speculation and what a misprediction actually costs.
- Implement branchy and branchless versions of the same kernels and
  measure both on predictable and unpredictable data.
- Recognize `csel` (ARM64) / `cmov` (x86) in compiler output, and know
  when the compiler already made your branch disappear.
- Answer the "sorted array" question with mechanism, not folklore.

## THEORY

### 1. Speculation: the CPU bets on your ifs

A modern core doesn't wait to know which way a branch goes — at 10+
instructions in flight per cycle it can't afford to. It *predicts* the
direction (per-branch history tables, pattern recognition; modern
predictors are TAGE-like and eerily good), keeps executing down the
guessed path, and commits results only when the branch resolves. Right
guess: free. Wrong guess: the pipeline's speculative work is flushed and
the correct path refetched — **~15–20 cycles**, call it 5ns, per miss.

The arithmetic that decides everything: a branch taken randomly 50/50
mispredicts half the time → ~2.5ns *average* tax per execution. A branch
that's 99.9% one way costs ~nothing. Same code, 100× different cost,
depending only on the *data's* predictability. This is why you cannot
review performance from source code alone.

### 2. Going branchless: trading control flow for data flow

```cpp
if (x > t) count++;            // branchy: control depends on data
count += (x > t) ? 1 : 0;      // branchless: DATA depends on data
```

The second form compiles to a comparison + conditional-select
(`csel` on ARM64, `cmov`/`setcc` on x86) — an ordinary instruction with a
fixed, small cost and *nothing to predict*. On unpredictable data,
branchless wins big (no 2.5ns tax). On predictable data, branchless can
LOSE: you always pay the select, while the predicted branch was free —
and a branch also lets the compiler skip whole blocks of work, which a
select must compute both sides of. Rule: branchless for unpredictable
hot branches with cheap sides; measured, not assumed.

The compiler knows all this and often converts simple ternaries to
`csel` on its own — part of this assignment is checking whether *your*
branchy version secretly compiled branchless (then the benchmark shows
no gap, and the assembly tells you why). `-O2`, Compiler Explorer, look
for `csel`/`ccmp` vs `b.gt`-style conditional jumps.

> **Linux/x86 sidebar:** `perf stat -e branches,branch-misses ./bench`
> gives you the misprediction *rate* directly — the number this whole lab
> infers from timing. Also the predictor's patterns differ: x86 `cmov`
> has its own cost model (Linus has opinions; find the mailing-list rant).

### 3. The famous interview question, dissected

"Why does sorting the array make the loop faster?" — with the pieces you
now have: unsorted, `x > threshold` at the median is a coin flip, ~50%
misses, ~10 wasted cycles per element. Sorted, the branch is false for
the first half (predictor locks on within a handful of iterations), true
for the second, with ONE miss at the crossover. Not "cache" — the data
and access pattern are identical; the *branch history* is what changed.
Follow-ups you should now handle: "how would you fix the unsorted case?"
(branchless / SIMD masks / partition first), "when would sorting first be
worth it?" (when you'll scan many times), and "what if the threshold is
at the 99th percentile?" (the branch is already 99% predictable —
leave it alone).

## Assignment spec

`include/course/branches.hpp` + `src/branches.cpp`:

- `std::int64_t count_above_branchy(const std::int64_t* data,
  std::size_t n, std::int64_t threshold)` — literal `if` in the loop.
- `std::int64_t count_above_branchless(...)` — same result, no
  conditional control flow in the loop body (comparison-as-integer or
  ternary; both fine).
- `std::int64_t sum_above_branchy(...)` / `sum_above_branchless(...)` —
  Σ of elements > threshold. The branchless sum wants a mask or select:
  `total += (x > t) ? x : 0;` — note this one has real work in the
  "taken" side, making the branchy version's win on predictable data
  visible.

The tests pin equality of results between the pairs (including empty,
all-above, all-below, and INT64 extremes). The benchmark provides the
same data sorted and shuffled at threshold = median — the 2×2 matrix
(branchy/branchless × sorted/random) from THEORY.

**Acceptance criteria:** `m06a03.*` green in `debug` and `asan`; then run
the bench and record the 2×2 matrix in a comment in `branches.cpp` —
plus one sentence: did your compiler make the branchy count_above
branchless already? (Check the asm before you answer.)

**Benchmark:** `m06a03_bench` (release, complete harness). Expected shape
on Apple Silicon: branchy/random clearly worst on the SUM kernel
(~2–5ns/elem penalty); branchy/sorted ≈ branchless/sorted; branchless
insensitive to order (that insensitivity is the whole point). If
branchy ≈ branchless on the COUNT kernel everywhere, read §2's last
paragraph and go look at the assembly.

## Hints

<details><summary>Hint 1 — "no conditional control flow" means</summary>
No <code>if</code>, no <code>&&</code>/<code>||</code> short-circuits on
the data (those are branches in disguise). <code>(x &gt; t)</code> as an
integer 0/1, or a ternary whose both sides are values (not statements),
compile to compare+select. The loop condition itself is fine — it's
perfectly predicted.
</details>

<details><summary>Hint 2 — results differ at the extremes</summary>
Check <code>threshold = INT64_MAX</code> (nothing is above) and negative
values: a mask trick like <code>x & -(x &gt; t)</code> is correct for the
COUNT but wrong for SUM with negative x if you got the mask backwards.
The ternary form has no such trap.
</details>

## Further reading

- The legendary Stack Overflow question: *"Why is processing a sorted
  array faster than processing an unsorted array?"* — now answer it
  without reading the answers.
- Agner Fog, *The microarchitecture of Intel, AMD and VIA CPUs* — ch. 3
  (branch prediction). The reference.
- Matt Godbolt, *"What Has My Compiler Done for Me Lately?"* (CppCon
  2017) — if you skipped it in Module 0, the bill is due.
- Daniel Lemire's blog, *"Mispredicted branches can multiply your running
  times"* — short, empirical, quotable.
