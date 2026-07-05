# a03 — Performance Pass: The Same Engine, Built to Be Measured

**Estimated effort:** 12–18 hours. The payoff assignment: Modules 5 and
6 were training for exactly this.

## Learning objectives

- Rebuild the book+engine with a flat-array price ladder, pooled order
  nodes, and intrusive FIFO queues — the production shape.
- Prove behavioral equivalence with **differential tests against your
  own a01/a02** (the reference implementation is code YOU trust).
- Hit throughput and tail-latency targets, measured with your m06a01
  histogram — and know which change bought which nanoseconds.

## THEORY

### 1. Where the baseline bleeds

Profile your a02 under the bench's op mix and three hotspots appear
(predicted by Modules 5-6; now you get to see them):

1. **`std::map` nodes** — every new price level heap-allocates; every
   ladder walk chases red-black pointers (cache misses, m06a02). And
   erase/rebalance costs appear exactly during bursts.
2. **`std::deque` FIFO + O(width) cancel scan** — cancels are ~45% of
   flow, and each one searches its level.
3. **`unordered_map` per-op hashing** — fine, but its buckets are
   another pointer chase; reserve() and a better layout help.

### 2. The production shape

- **Flat ladder:** prices are integer ticks in a band. `std::vector<Level>
  levels_[2]` indexed by `price - band_min` gives O(1) level access,
  contiguous scans for "next best", zero allocation after construction.
  Track best indices; on emptying a best level, scan toward the far end
  (the scan is over a dense array — m06a02 says that's nearly free).
- **Pooled orders + intrusive FIFO:** each order lives in your m05a02-
  style pool; the level's FIFO is a doubly-linked list THROUGH the order
  nodes (m05's intrusive lesson). Cancel = O(1) unlink, no scan.
- **Open-addressed id map** (or `unordered_map` with `reserve` +
  `max_load_factor` tuning — measure both; the id map is rarely the
  bottleneck once the rest is flat).

The band: construction takes `(band_min, band_max)`; ops outside the
band are rejected (`add` returns false). Real feed handlers do the same
with a re-centering mechanism when the market drifts — out of scope,
worth one sentence in an interview.

### 3. Differential testing: your a02 is the oracle

The equivalence tests replay identical randomized op streams into your
a02 engine and your `FastEngine`, comparing best bid/ask, level
quantities, trade streams, and order counts after every op. This is how
real matching engines are regression-tested (replay + compare against
the previous build). It also means: **if a01/a02 are wrong, a03
inherits the wrong** — the differential test proves equivalence, not
truth. Truth lives in a01/a02's spec tests. Keep them green.

### 4. Measure like m06a01 taught you

The bench drives a realistic mix (55% add near the touch, 40% cancel,
5% aggressive cross) and prints throughput plus YOUR LatencyHistogram's
p50/p99/p99.9 per op class. Targets on Apple Silicon (release):

- **Throughput ≥ 1M ops/s** on the mixed stream (baseline a02 typically
  lands 200-500k).
- **p99 submit ≤ 2µs, p99 cancel ≤ 1µs**, and — the real prize —
  **p99.9 within ~3× of p99** (a flat tail means nothing on the hot
  path allocates or rebalances; a fat one means a hidden malloc).

Record your before (a02) / after (FastEngine) table in the header
comment, with one line per optimization naming its measured win.

## Assignment spec

`include/course/fast_engine.hpp` + `src/fast_engine.cpp`:

`FastEngine` — a02's exact observable behavior, new spine:

- `FastEngine(Price band_min, Price band_max)`.
- `std::vector<Trade> submit(OrderId, Side, Price, Qty)` — same
  semantics as a02 (trades in order, maker price, FIFO, remainder
  rests). Out-of-band price: empty vector, no-op. (`Trade` comes from
  m10a02's header — same type, same meaning.)
- `bool cancel(OrderId)`, `bool reduce(OrderId, Qty)`.
- Queries: `best_bid/best_ask/qty_at/depth/order_count/quantity_of` —
  a01's signatures.
- **No heap allocation on the no-trade path** (adds, cancels, reduces —
  ~95% of real flow) after construction: pool pre-sized, ladder flat.
  The bench hooks global `operator new` and will tattle. Crossing
  submits may allocate their returned `Trade` vector — noting how you'd
  remove even that (caller-provided buffer / SBO vector) is question-
  bank material, not required work.

**Acceptance criteria:** all `m10a03.*` green in `debug` and `asan`
(differential suites included), THEN the bench targets met in release.
Green-but-slow is half done; the README table is the deliverable.

## Hints

<details><summary>Hint 1 — the intrusive node</summary>
<pre>struct OrderNode {
    OrderId id; Qty qty; Price price; Side side;
    OrderNode* prev; OrderNode* next;   // FIFO links through the pool
};</pre>
Level = head/tail pointers + total_qty. Unlink is the 4-pointer dance
you know from m05; drawing it once on paper saves an hour of asan.
</details>

<details><summary>Hint 2 — best tracking without a sorted structure</summary>
Keep <code>best_bid_idx_</code>/<code>best_ask_idx_</code>. On add:
best = max/min(best, idx) — one compare. On emptying the best level:
walk the dense array toward worse prices until a non-empty level (or
none). The walk LOOKS O(band) but is amortized tiny and touches
contiguous memory — measure before you "fix" it; that measurement is
question-bank material.
</details>

<details><summary>Hint 3 — the differential test diverges on trades</summary>
Trade ORDER matters (execution order). If quantities match but maker
ids differ, your FIFO unlink relinked something wrong; if prices
diverge on sweeps, your best-walk skipped a level that became non-empty
mid-sweep. Replay the failing seed's prefix — the harness prints it.
</details>

## Further reading

- The "How to Build a Fast Limit Order Book" post from a01 — reread it
  now; every optimization it names is one you just built.
- CppCon 2017, Carl Cook — rewatch §"free lists / hot path" with your
  own numbers on the table. Different talk the second time.
- David Gross (Optiver), *"Trading at light speed: designing low
  latency systems in C++"* (Meeting C++ 2022) — the modern sequel.
