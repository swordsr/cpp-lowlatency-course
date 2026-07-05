# a02 — Book Builder: From Feed to Truth (and Back After a Gap)

**Estimated effort:** 8–12 hours. Links your m11a01 AND your m10a01 —
the feed handler meets the order book.

## Learning objectives

- Reconstruct the market's book by mirroring feed events into your
  m10a01 `OrderBook`.
- Implement the stale/recovery lifecycle: gap → stop trusting → buffer
  → snapshot → filtered replay → live.
- Internalize the risk rule that shapes the design: **a stale book
  must be unusable by accident** — consumers must have to CHECK.

## THEORY

### 1. Mirroring is easy; the mapping is exact

Feed events map onto your m10a01 interface with no ceremony: `MdAdd` →
`book.add(id, side, price, qty)`; `MdExecute{id, qty}` →
`book.reduce(id, qty)` (execution consumes resting quantity — your
reduce already removes at zero); `MdDelete` → `book.cancel(id)`. Note
what's ABSENT: no matching. The exchange already matched; the feed is
the *result*. Your m10a02 engine and this builder are the two ends of
the same pipe — one produces events like these, one consumes them.
Events referencing unknown ids (can happen mid-recovery) are counted
(`dropped()`) and skipped — never a crash, never a corrupt book.

### 2. The stale lifecycle

```text
LIVE --on_gap--> STALE --load_snapshot--> replay buffer --> LIVE
                  |  ^                         |
                  |  '--- gap during replay ---'
                  '--- while STALE: buffer datagrams, apply nothing
```

The moment a gap is reported the book is a lie — some adds/deletes you
never saw. The builder: (1) flips `is_stale()` true (and consumers —
a03's strategy — must check it: quoting off a stale book is the
career-limiting bug). The gap CARRIER needs care: a01 reports `on_gap`
and then delivers that datagram's events in the same call — the builder
must IGNORE those events (the book can't absorb post-gap data) and
buffer the carrier datagram itself, so its messages aren't lost: they
come back through the replay, where the sequence filter decides their
fate. (2) While stale, buffer every raw datagram (copies — correctness
path, allocation allowed). (3) On `load_snapshot(orders, next_seq)`:
rebuilds a FRESH book from the snapshot, calls your a01's
`reset_sequence(next_seq)`, replays the buffered datagrams through the
session — whose stale/overlap filtering (a01's state machine!) now
discards everything the snapshot already covers — and goes live if the
replay was contiguous. A gap DURING replay re-arms the whole cycle:
still stale, keep the tail buffered. This compositional design — the
recovery filter is just the normal sequencing filter after a reset —
is why a01 made you build `reset_sequence`.

### 3. Snapshots, honestly simplified

Real venues run a separate snapshot channel (ITCH: Spin servers / GLIMPSE)
you subscribe to on demand; the snapshot arrives WITH its sequence
number, which is the splice point. CMD's version: the test hands you
`std::vector<SnapshotOrder>` + `next_seq` directly — the transport is
faked, the splice logic (the actual hard part) is fully real.

## Assignment spec

`include/course/book_builder.hpp` (header-only):

- `struct SnapshotOrder { OrderId id; Side side; Price price; Qty
  qty; };` (given).
- `class BookBuilder` — implements the `FeedHandler` concept (that's a
  `static_assert` in the tests) and owns a `FeedSession` + an
  `OrderBook`:
  - `bool on_datagram(std::string_view)` — LIVE: route through the
    session (events mutate the book). STALE: buffer the datagram,
    return true (buffering ≠ failure). Malformed: false either way.
  - `bool is_stale() const` — false until the first gap; a fresh
    builder is live.
  - `void load_snapshot(const std::vector<SnapshotOrder>&,
    std::uint64_t next_seq)` — THEORY §2's splice.
  - `const OrderBook& book() const`, `std::uint64_t dropped() const`
    (events on unknown ids).
  - The handler callbacks (`on_add` etc.) are public so the concept
    sees them; document that they're the session's to call.

**Acceptance criteria:** all `m11a02.*` green in `debug` and `asan`,
with a01 and m10a01 green underneath (three layers of your own code —
welcome to systems work).

## Hints

<details><summary>Hint 1 — the book can't be reset? Replace it</summary>
m10a01's book is deliberately non-copyable and has no clear(). Hold it
as <code>std::unique_ptr&lt;OrderBook&gt;</code> and make a fresh one
in load_snapshot — m02a03's vocabulary type doing exactly its job.
</details>

<details><summary>Hint 2 — replay is a loop over your own on_datagram?</summary>
Almost — but on_datagram buffers while stale, so replaying through it
double-buffers forever. Split: a private <code>apply(dgram)</code> that
always routes to the session; on_datagram = stale ? buffer : apply;
load_snapshot drains the buffer through apply. Watch the re-gap case:
if apply reports a gap mid-replay, STOP draining and stay stale, with
the remainder — INCLUDING the datagram that just gapped (it's a carrier;
same rule as live) — still buffered for the next snapshot.
</details>

<details><summary>Hint 3 — the mid-recovery duplicate test fails</summary>
The splice depends on reset_sequence happening BEFORE the drain, and
the buffer preserving arrival order. If order 2 appears twice in the
book, your a01 stale-filtering has an off-by-one (its OverlapDelivers
test should be failing too — fix it THERE).
</details>

## Further reading

- ITCH "Spin" / GLIMPSE snapshot protocol docs (public) — the real §3.
- Your m10a01/m11a01 handouts — this assignment is deliberately mostly
  composition; the reading is the two things being composed.
