# a02 — Matching Engine

**Estimated effort:** 8–12 hours. **Builds on YOUR a01** — this target
links `m10a01`; get that fully green first.

## Learning objectives

- Implement price-time priority matching: the algorithm at the center of
  every equity/futures exchange.
- Generate correct trade records (maker/taker, resting price, partial
  fills) through multi-level sweeps.
- Own the engine's postcondition — the book is never crossed after a
  submit — and the conservation invariant behind every fill.

## THEORY

### 1. The matching loop

An arriving limit order is an *aggressor* while it crosses:

```text
submit(id, side, limit, qty):
    while qty > 0 and book has an opposite best that crosses limit:
        level = opposite best price
        maker = book.front_order(opposite, level)     // oldest = first
        fill  = min(qty, book.quantity_of(maker))
        emit Trade{maker, taker=id, price=level, fill}
        book.reduce(maker, fill)                      // consumes FIFO head
        qty -= fill
    if qty > 0: book.add(id, side, limit, qty)        // remainder rests
```

"Crosses" is side-dependent: a bid crosses while `limit >= best_ask`; an
ask while `limit <= best_bid`. Everything else is bookkeeping — but the
bookkeeping IS the exchange, so the tests are merciless about it:

- **Price:** every trade prints at the MAKER's level (module README:
  price improvement). An aggressive bid at 102 sweeping asks at 100 and
  101 produces trades at 100 and 101 — never 102.
- **Time:** within a level, fills consume the FIFO front. Your a01's
  `front_order`/`reduce` were built for exactly this loop.
- **Termination:** the remainder rests only when nothing crosses —
  which restores `best_bid < best_ask`. That postcondition (the book is
  uncrossed after every submit) is checked after every operation in the
  scenario tests.

### 2. Trades are the audit trail

`Trade{maker_id, taker_id, price, qty}` — the maker is the resting
order, the taker the aggressor. Downstream (clearing, positions, your
Module 11 pipeline) reconstructs everything from trades, so the
CONSERVATION law must hold to the share: for any submit,
`submitted_qty == sum(fills) + rested_remainder`, and every maker's
resting quantity decreases by exactly its fills. The randomized scenario
test audits this the way a clearing firm would.

### 3. What this engine deliberately doesn't do

Real matchers also handle: market orders (limit = ∞ — trivially
expressible here), IOC/FOK time-in-force (don't rest / all-or-nothing),
self-trade prevention (same participant on both sides), auctions, and
odd lots. Each is a small delta on the loop above — worth five minutes
of thought each, because "how would you add IOC?" is the standard
follow-up interview question to this exact project. (Answer for IOC:
skip the final `add`. That's it. Say it confidently.)

## Assignment spec

`include/course/matching_engine.hpp` + `src/matching_engine.cpp`:

- `struct Trade { OrderId maker_id; OrderId taker_id; Price price;
  Qty qty; };` (given).
- `class MatchingEngine` owning an `OrderBook` (given member):
  - `std::vector<Trade> submit(OrderId, Side, Price limit, Qty)` —
    the loop from THEORY §1. Trades in execution order. Rejected input
    (duplicate id, qty 0) returns an empty vector and changes nothing.
  - `bool cancel(OrderId)` / `bool reduce(OrderId, Qty)` — passthroughs
    (reduce keeps queue position; a03's differential tests depend on it).
  - `const OrderBook& book() const` — the tests inspect the book after
    every call.

**Acceptance criteria:** all `m10a02.*` tests green in `debug` and
`asan`, with your a01 green underneath.

## Hints

<details><summary>Hint 1 — the crossing predicate</summary>
Write it once: for a Bid aggressor the opposite best is
<code>book_.best_ask()</code> and crossing is <code>limit &gt;=
*best</code>; for an Ask, <code>best_bid()</code> and <code>&lt;=</code>.
A small lambda returning <code>std::optional&lt;Price&gt;</code> ("the
level I may trade at, if any") keeps the while-loop readable.
</details>

<details><summary>Hint 2 — the FIFO-priority test fails</summary>
You're probably reducing some order at the level instead of THE FRONT
one, or re-reading front_order after reduce removed it mid-level.
Sequence: front → quantity_of → fill → reduce; only then re-ask for the
(possibly new) front.
</details>

<details><summary>Hint 3 — the sweep test's trade list is wrong at the boundary</summary>
Off-by-one in the crossing check (<code>&gt;</code> vs
<code>&gt;=</code>): a bid AT the ask price crosses — equality trades.
Check both sides' comparisons; the tests probe both boundaries.
</details>

## Further reading

- CME Globex matching algorithm docs (public) — read the FIFO section;
  recognize your loop, then skim what pro-rata changes.
- Budish, Cramton & Shim, *"The High-Frequency Trading Arms Race"* (QJE
  2015) §2 — the market-design view of continuous FIFO matching.
- Your m10a01 README's reading list — still the canon for this stage.
