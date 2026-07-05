# a01 — The CMD Feed Decoder: Sequencing, Gaps, Heartbeats

**Estimated effort:** 8–12 hours. Links your m09a03 (`ByteReader` comes
back to work).

## Learning objectives

- Decode a sequenced, packetized market-data protocol — ITCH's shape at
  course scale.
- Implement the sequencing state machine: in-order, gap, duplicate,
  partial-overlap, heartbeat.
- Deliver events through a compile-time handler concept (m04a02's
  machinery earning its keep on a hot path).

## THEORY

### 1. The CMD protocol (all integers little-endian)

```text
datagram := u64 first_seq, u16 count, then `count` framed messages
message  := u16 len, payload         (same framing as your CTP)
payload  := type u8, fields...
  'A' MdAdd     : u64 order_id, u8 side('B'/'S'), u32 qty, i64 price
  'E' MdExecute : u64 order_id, u32 qty      (trade against that order)
  'D' MdDelete  : u64 order_id
heartbeat := count == 0                      (seq unchanged; "I'm alive")
```

Every *message* has a sequence number; the datagram carries the first
one plus a count, so packet N+1's `first_seq` should equal packet N's
`first_seq + count`. This is MoldUDP64's design, simplified: sequencing
lives in the transport header, message semantics in the payloads. Note
what the feed describes: **other participants' orders**. `MdAdd` is
"someone's order joined the book" — a02 will mirror them into your
m10a01 book to reconstruct the market.

### 2. The sequencing state machine

Given `expected` (next sequence you haven't processed) and an incoming
`(first, count)`:

| case | condition | action |
|------|-----------|--------|
| in order | `first == expected` | decode all, `expected += count` |
| heartbeat | `count == 0` | valid; nothing advances |
| gap | `first > expected` | report `{expected, first}`; decode anyway; `expected = first + count` |
| stale | `first + count <= expected` | drop silently (retransmit/dup) |
| overlap | `first < expected < first + count` | skip the first `expected - first` messages, deliver the rest |

Decode-anyway on gaps is deliberate and standard: the messages after a
gap are real; it's the BOOK that becomes untrustworthy, and that's
a02's problem (`on_gap` tells it). The overlap case is what
retransmissions look like when they race the live feed — off-by-ones
here are the classic feed-handler bug, and the tests probe both edges.

### 3. All-or-nothing packets

A malformed message (bad length, unknown type, trailing bytes) rejects
the WHOLE datagram: deliver nothing, advance nothing, return false.
Half-applied packets are how books silently corrupt — validate first,
then deliver. (Cost: you decode into a small local buffer before
dispatching. An arena from m05a01 would remove that allocation; note
it, don't build it.)

### 4. The handler is a concept, not an interface

```cpp
template <typename H>
concept FeedHandler = requires(H h, ...) { h.on_add(...); ... };
template <FeedHandler H> bool on_datagram(std::string_view, H&);
```

Virtual dispatch per market-data message is the textbook place NOT to
pay for runtime polymorphism (m03a02's benchmark said why): the handler
type is known at build time, the calls inline, and the concept gives
readable errors. This is the m03→m04 material's production payoff.

## Assignment spec

`include/course/feed.hpp` (header-only; the encoder in
`include/course/md_encode.hpp` is GIVEN — it is the exchange's side of
the wire, i.e. your test fixture):

- Event structs + `GapInfo{expected, got}` (given).
- `concept FeedHandler` — `on_add(const MdAdd&)`, `on_execute(const
  MdExecute&)`, `on_delete(const MdDelete&)`, `on_gap(const GapInfo&)`
  (given).
- `class FeedSession` — `template <FeedHandler H> bool
  on_datagram(std::string_view dgram, H& handler)` implementing §2 + §3
  (use your m09a03 `ByteReader`); `next_expected()`, counters
  (`packets()`, `messages()`, `gaps()`); `reset_sequence(std::uint64_t
  next)` — a02's snapshot recovery needs it.
- First packet: a fresh session accepts ANY `first_seq` as its starting
  point (feeds start mid-day; you join where you join).

**Acceptance criteria:** all `m11a01.*` tests green in `debug` and
`asan` (asan is load-bearing — every malformed-packet test is a bounds
check).

## Hints

<details><summary>Hint 1 — the decode-then-deliver shape</summary>
Parse header; run the §2 table to get <code>skip</code> (messages to
drop) and whether to proceed; decode ALL messages into a local
<code>std::vector&lt;std::variant&lt;...&gt;&gt;</code> validating as
you go (any failure → return false, deliver nothing); then dispatch
the ones past <code>skip</code> and advance <code>expected</code>.
</details>

<details><summary>Hint 2 — the overlap off-by-one</summary>
<code>skip = expected - first</code> messages, and afterwards
<code>expected = first + count</code> (NOT expected + delivered). The
OverlapDeliversOnlyUnseen test hits both edges: overlap-by-one and
everything-but-one-stale.
</details>

<details><summary>Hint 3 — heartbeats aren't in-order packets</summary>
Handle <code>count == 0</code> BEFORE the gap check: a heartbeat
carries the NEXT expected seq (nothing new), and must neither gap nor
advance. The HeartbeatDuringGap test checks you don't "recover" off a
heartbeat.
</details>

## Further reading

- Nasdaq [MoldUDP64](https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/moldudp64.pdf)
  spec — 6 pages; you just implemented ~4 of them.
- TotalView-ITCH 5.0 (m09a03's reading) §4.3–4.5 — real add/execute/
  delete, one indirection richer than CMD.
- Your m04a02 and m03a02 handouts — the dispatch-choice rationale §4
  compresses.
