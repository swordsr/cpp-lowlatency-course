# Module 10 — Capstone I: Limit Order Book & Matching Engine

**Time budget:** ~3 weeks. Three stages, strictly ordered — each links
against the previous one's code:

1. **a01** — the book: price levels, FIFO order queues, O(log levels)
   maintenance, best bid/ask. Correctness above all; ~20 tests.
2. **a02** — the matching engine on top of YOUR a01 book: price-time
   priority, price improvement, partial fills, multi-level sweeps.
3. **a03** — the performance pass: same behavior, rebuilt with your
   Module 5 machinery (pools, intrusive lists, flat arrays), verified by
   *differential testing against your own a01/a02*, and held to
   throughput/latency targets with your m06a01 histogram.

This is THE interview project. "Design a limit order book" is asked, in
some form, at effectively every HFT shop — usually twice: once as data
structure design (a01/a03's material) and once as matching semantics
(a02's). By the end you'll have built it, tested it, made it fast, and
measured it — which is four different interview answers.

## The domain, in one screen

A **limit order** says "buy (bid) or sell (ask) up to `qty` at price no
worse than `price`". Orders that don't immediately trade **rest** in the
book, queued at their price level, oldest first. The book is two sorted
ladders of levels: bids descending (best = highest), asks ascending
(best = lowest). `best_bid < best_ask` always holds *after* matching —
the spread. An arriving order that **crosses** (bid ≥ best ask, or ask ≤
best bid) trades immediately against resting orders: best price level
first, oldest order first within a level (**price-time priority**), at
the *resting* order's price (**price improvement** for the aggressor),
until it's filled or nothing crosses; any remainder rests. Cancels
remove resting orders; reduces shrink them (partial cancel, keeping
queue position — that's why it's reduce, not modify-up: increasing qty
would be queue-jumping, and real exchanges make you cancel/replace).

## Assignments

| # | Assignment | What you'll touch |
|---|------------|-------------------|
| 1 | [a01-order-book-core](a01-order-book-core/) | the two ladders, level FIFO queues, id lookup, book invariants |
| 2 | [a02-matching-engine](a02-matching-engine/) | price-time priority, trades, sweeps, the uncrossed postcondition |
| 3 | [a03-performance-pass](a03-performance-pass/) | pooled orders, intrusive queues, flat levels, differential tests, p99 targets |

## Interview questions

<details>
<summary>"Design a limit order book. Go."</summary>

Structure the answer in layers: (1) operations and their frequencies —
add/cancel dominate (often 95%+; mostly near the touch), executions
next, top-of-book queries constant; (2) baseline: two
`std::map<price, Level>` (sorted ladders) + `unordered_map<id, Order*>`
+ FIFO queue per level — O(log L) adds, O(1) cancels with back-pointers,
O(1) best; (3) the HFT refinement: prices are integers in a narrow band,
so replace maps with a flat array/vector of levels indexed by price
offset (O(1) everything, cache-friendly), pool the orders, make the
FIFO queues intrusive. Close with what you'd measure (p99 per op class,
under a realistic add/cancel mix). That's a03's exact arc.
</details>

<details>
<summary>"Why price-TIME priority? What breaks without the time part?"</summary>

Time priority rewards posting liquidity early: the first quote at a
price is the first filled, so market makers compete to be first, which
tightens spreads. Without it (pro-rata allocation — some futures do use
it), size gets rewarded instead: participants post inflated quantities
to capture allocation share, then cancel the excess — different game,
fatter books, different pathologies. Knowing both models exist (and
that queue position is *the* asset a market maker owns) reads as real
market-structure literacy.
</details>

<details>
<summary>"Why does the trade print at the RESTING order's price?"</summary>

The resting order set the terms; the aggressor accepted no-worse terms.
Executing at the resting price gives the aggressor any surplus ("price
improvement") and — the deeper reason — makes the book's displayed
prices honest: what you see is what you'd pay. Executing at the
aggressor's limit would let displayed quotes fill better than displayed,
rewarding stale quotes and breaking every participant's model of the
book.
</details>

<details>
<summary>"Your book benchmarks at 5M ops/s mean. The interviewer looks unimpressed. Why?"</summary>

Means again (m06a01): a book that does 200ns typical but 50µs when a
level allocates or a map rebalances is a bad book — the 50µs hits
exactly when the market is busiest, because bursts are what cause
structure churn. Report p99/p99.9 per operation class under a bursty,
realistic mix; show the tail is flat because nothing on the hot path
allocates (pools), rebalances (flat arrays), or chases cold pointers
(intrusive nodes). a03's bench prints precisely this.
</details>
