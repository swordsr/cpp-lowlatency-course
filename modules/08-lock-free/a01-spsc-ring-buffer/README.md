# a01 — SPSC Ring Buffer: The Queue HFT Actually Uses

**Estimated effort:** 6–9 hours.

## Learning objectives

- Implement the wait-free single-producer/single-consumer ring buffer —
  the workhorse that carries data between pipeline stages in
  every serious trading system (including your Module 11 capstone).
- Use monotonic indices + power-of-two masking, and say why they beat
  wrapped indices.
- Apply acquire/release exactly where needed and defend every ordering.
- Pad the cursors and *see* the false-sharing cost when you don't.

## THEORY

### 1. Ownership makes it wait-free

The ring: a fixed array of N slots plus two cursors. The producer is the
**only writer** of `tail_`; the consumer is the **only writer** of
`head_`. Nobody CASes anything — each side reads the other's cursor
(acquire), writes its own (release), and can never fail-and-retry:
every operation completes in a bounded number of steps. Wait-free, the
strongest guarantee, from a data structure simple enough to write on a
whiteboard — *if* you know the two rules:

```text
try_push:  read head_ (acquire)  → full if tail_ - head_ == N
           write slot[tail_ & (N-1)]
           tail_.store(tail_ + 1, release)     // publishes the slot
try_pop:   read tail_ (acquire)  → empty if head_ == tail_
           read slot[head_ & (N-1)]
           head_.store(head_ + 1, release)     // returns the slot
```

The release on `tail_` is the MailBox handoff from m07a02: it publishes
the slot write to the consumer's acquire read. The release on `head_`
publishes the *vacancy* back — the producer's acquire of `head_` must
see that the consumer is truly done with the slot before overwriting it.
Both directions matter; the tests hunt the second one specifically.

### 2. Monotonic indices: the fencepost slayer

Wrapped indices (`idx = (idx+1) % N`) force the classic "is full ==
is empty?" dilemma (solve by wasting a slot or adding a count — both
ugly). Monotonic `uint64` cursors never wrap in practice (2⁶⁴ pushes at
100M/s is 5,800 years); `tail_ - head_` is *exactly* the occupancy, full
is `== N`, empty is `== 0`, and the slot is `index & (N-1)` — which is
why N must be a power of two (masking is one AND; `%` with a non-const
divisor is ~20+ cycles of division).

### 3. Padding the cursors (m07a03, cashed in)

`head_` and `tail_` are each hammered by a different core. Adjacent,
they'd false-share: every producer publish invalidates the consumer's
line and vice versa — the m07a03 benchmark showed you that tax. Each
cursor gets `alignas(kAlign)` (128 on Apple Silicon). The layout test
pins this via `sizeof` — the object gets fat; that's the deal.

Production footnote (and stretch goal): real implementations (Rigtorp's
`SPSCQueue`, folly's `ProducerConsumerQueue`) also keep a *cached copy*
of the far cursor per side, refreshing it only when the queue looks
full/empty — dropping cross-core traffic to near zero in steady state.
Get green first; then try it and watch the bench.

## Assignment spec

`include/course/spsc_ring.hpp` (header-only):

`SpscRing<T, N>` — N a power of two (enforced by `static_assert`,
given). Exactly one producer thread and one consumer thread, ever.

- `bool try_push(T value)` — false when full; move `value` into the
  slot.
- `std::optional<T> try_pop()` — nullopt when empty; move out of the
  slot.
- `std::size_t size() const` — approximate occupancy (racy snapshot is
  fine and documented; the tests only check it single-threaded).
- `static constexpr std::size_t capacity()` — N.
- Move-only payloads must work (`unique_ptr` tests).
- Cursor padding pinned by a `sizeof` test (THEORY §3).

Storage note: `std::array<T, N>` (requires default-constructible T) is
the sanctioned simplification — real rings use raw storage + placement
new to drop that requirement; you own that machinery (m02a04) if you
want the stretch.

**Acceptance criteria:** `m08a01.*` green in `debug`, `asan`, and
`tsan`. The SPSC stress test moves 1M sequenced values across real
threads and fails within seconds (bounded) on ordering mistakes — on
your ARM Mac, a relaxed-instead-of-release publish genuinely loses.

**Benchmark:** `m08a01_bench` (release, complete harness): steady-state
throughput producer→consumer, and a ping-pong latency test (two rings,
round trip). Expect **50–150M items/s** throughput and **~100–300ns**
round trip on Apple Silicon once green. Then try the cached-cursor
stretch and watch throughput jump. Record before/after in a comment.

## Hints

<details><summary>Hint 1 — members first</summary>
<code>std::array&lt;T, N&gt; slots_;</code> plus two cursors:
<code>alignas(kAlign) std::atomic&lt;std::uint64_t&gt; head_{0};</code>
and the same for <code>tail_</code>. Both grow forever; all masking
happens at the slot access.
</details>

<details><summary>Hint 2 — orderings, side by side</summary>
Producer: <code>head_.load(acquire)</code> (am I full?),
<code>tail_.load(relaxed)</code> (my own cursor — nobody else writes
it), slot write, <code>tail_.store(t+1, release)</code>. Consumer is
the mirror. If tsan flags the slot access, one of the acquires is a
relaxed; if values arrive corrupted on your Mac only, it's the release.
</details>

<details><summary>Hint 3 — the stress test loses values at wraparound</summary>
Check the full condition: <code>tail - head == N</code>, not
<code>N-1</code>; and make sure try_pop moves OUT of the slot before
publishing head — publishing first hands the producer a slot you
haven't finished reading.
</details>

## Further reading

- Erik Rigtorp, [SPSCQueue](https://github.com/rigtorp/SPSCQueue) —
  read the README's design notes after you're green; it's this
  assignment plus the cache optimization.
- *C++ Concurrency in Action* 2e, ch. 7.5 (lock-free queue walkthrough).
- CppCon 2017, Charles Frasch, *"A lock-free SPSC ring buffer"*-style
  talks abound; watch one AFTER implementing, as review.
