# a01 — Order Book Core

**Estimated effort:** 8–12 hours. Capstone rules: the interface is
fixed, the internals are YOUR design (the hints sketch the standard
baseline; deviate knowingly).

## Learning objectives

- Design and build the two-ladder book: sorted price levels, FIFO order
  queues, O(1) id lookup — the data structure of this industry.
- Maintain aggregate invariants (level totals, counts, emptiness) under
  add/cancel/reduce.
- Defend a complexity budget per operation.

## THEORY

### 1. What the book must answer, and how often

| query/op | typical frequency | your budget |
|----------|-------------------|-------------|
| add resting order | ~50% of messages | O(log levels) or better |
| cancel by id | ~45% | O(1) after lookup |
| reduce by id | few % | O(1) after lookup |
| best bid/ask | after every change | O(1) |
| qty at level | constant (strategies stare at it) | O(1)-ish |

Two ladders: bids sorted descending (best first), asks ascending.
Within a level, strict FIFO — arrival order IS queue priority, and a02
will consume it from the front. Every order is also reachable by id in
O(1) (cancels arrive as "cancel #91847", not "cancel the third order
at 155.10").

### 2. The baseline representation (and where it creaks)

```cpp
std::map<Price, Level, std::greater<>> bids_;   // descending
std::map<Price, Level> asks_;                   // ascending
std::unordered_map<OrderId, OrderInfo> orders_; // id -> side/price/qty
struct Level { std::uint32_t total_qty; std::deque<OrderId> fifo; };
```

`std::map` gives sorted levels and O(log L) everything; `begin()` is
the best level, O(1). The deque keeps time priority. This is the
CORRECT baseline — build it, pass the tests, and *notice* the sins
you'll confess in a03: every map node is a heap allocation (add of a
new level allocates on the hot path), cancel does an O(level-width)
deque scan to find the id, and pointer-chasing a red-black tree is a
cache-miss parade (Module 6 taught you the price). Correct first.

### 3. Crossed books are LEGAL here

This book stores; it does not match. `add(bid @ 102)` with an ask
resting at 100 simply rests — `best_bid() > best_ask()`, a crossed
book, and that's fine *in a01*. Keeping storage and matching separate
is deliberate layering: a02's engine will own the "never rest a
crossing order" rule, and its uncrossed-postcondition tests enforce it
THERE. (Real venues do the same split: the book is a dumb structure;
the matcher is policy.)

### 4. `front_order` and `quantity_of`: the matching hooks

Two accessors exist purely as a02's consumption interface: the oldest
order id at a price (queue front), and any order's remaining quantity.
Get their semantics exact — a02's FIFO-priority tests will interrogate
your a01 through them.

## Assignment spec

`include/course/order_book.hpp` (types + interface given, private
section EMPTY — your design) + `src/order_book.cpp`:

- `bool add(OrderId, Side, Price, Qty)` — rests the order. False (and
  no-op): duplicate live id, or qty 0.
- `bool cancel(OrderId)` — removes entirely. False: unknown/dead id.
- `bool reduce(OrderId, Qty by)` — shrinks by `by`, KEEPING queue
  position; `by >=` remaining removes the order. False: unknown id or
  `by == 0`.
- `std::optional<Price> best_bid() / best_ask()`.
- `Qty qty_at(Side, Price)` — level aggregate; 0 if no level.
- `std::size_t depth(Side)` — number of non-empty levels.
- `std::size_t order_count()` — live orders.
- `std::optional<OrderId> front_order(Side, Price)` — oldest at level.
- `std::optional<Qty> quantity_of(OrderId)` — remaining qty.

Empty levels must disappear (depth and best track that). IDs are never
reused by the tests; you may assume it.

**Acceptance criteria:** all `m10a01.*` tests green in `debug` and
`asan`. The finale is a 10k-op randomized differential test against a
naive aggregate model — if your invariants drift ANYWHERE, it names the
op number.

## Hints

<details><summary>Hint 1 — cancel needs a back-reference</summary>
The id map should store enough to reach the order's level without
searching the ladder: side + price gets you the level in O(log L); the
deque scan inside the level is the accepted a01 cost (a03 kills it with
an intrusive list). Keep <code>total_qty</code> maintained on every
mutation — qty_at must not recompute by summation.
</details>

<details><summary>Hint 2 — the two ladders differ by ONE template argument</summary>
<code>std::map&lt;Price, Level, std::greater&lt;&gt;&gt;</code> for
bids means <code>begin()</code> is the best price on BOTH ladders —
every "which side am I on" branch collapses to picking the right member.
A <code>template &lt;typename Cmp&gt;</code> private helper or just two
small lambdas keeps add/cancel single-sourced. (Or store asks with
negated prices in one map type. Pick one; say why in a comment.)
</details>

<details><summary>Hint 3 — the randomized test fails at op #6371</summary>
Re-run is deterministic (fixed seed). Add a <code>dump()</code> of your
ladders, binary-search the op stream by replaying prefixes, and find
the single op where your book and the model diverge. This debugging
motion — bisecting a deterministic replay — is exactly how real book
bugs get found; consider the hour it takes a course fee, not a tax.
</details>

## Further reading

- [How to Build a Fast Limit Order Book](https://web.archive.org/web/20110219163448/http://howtohft.wordpress.com/2011/02/15/how-to-build-a-fast-limit-order-book/)
  — the classic blog post; a01 is its baseline, a03 is its punchline.
- Nasdaq TotalView-ITCH spec (m09a03's reading) — §4's message types
  are exactly your add/cancel/reduce, from the horse's mouth.
- CppCon 2017, Carl Cook, *"When a Microsecond Is an Eternity"* — watch
  it now; it narrates this module's whole arc.
