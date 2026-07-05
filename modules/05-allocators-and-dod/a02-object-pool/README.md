# a02 — Object Pool: Recycling in O(1)

**Estimated effort:** 5–7 hours.

## Learning objectives

- Implement a fixed-capacity object pool with an intrusive free list —
  O(1) acquire and release, zero per-object memory overhead.
- Use a union to overlay two lifetimes on one piece of storage, legally.
- Understand LIFO recycling as a cache-warmth strategy.
- Know exactly where this pattern sits in a trading system (order state,
  session objects — anything born and dying at market speed).

## THEORY

### 1. When the arena isn't enough

a01's arena frees everything at once — perfect for per-packet scratch,
useless for objects with *independent* lifetimes. An order object is born
when the strategy sends it and dies at fill or cancel, seconds or hours
later, in no particular order. You need individual `release` — but `new`/
`delete` per order is the tail-latency lottery you just escaped.

The pool: allocate N slots up front, hand them out and take them back in
O(1). Same-size objects only — that one constraint deletes fragmentation,
size classes, and searching, which is everything that made `malloc` slow.

### 2. The intrusive free list

Where do you keep the list of free slots? A `std::vector<int>` of free
indices works — and costs a second allocation, a second cache line per
operation, and bookkeeping. The intrusive answer: a free slot contains no
live object, so *the slot itself* can store the link:

```cpp
union Slot {
    Slot* next_free;                              // when free
    alignas(T) unsigned char storage[sizeof(T)];  // when live
};
```

Free slots form a singly linked list threaded through the pool's own
memory. `acquire` pops the head (one read, one write); `release` pushes
(one write). The union makes the overlay explicit and legal: a union
member's lifetime begins when you write it, and C++ blesses reading the
member you last wrote. You placement-new a `T` into `storage` when the
slot goes live, destroy it and write `next_free` when it dies — never
both alive at once. (This is also your first meeting with the pattern
behind `std::variant`, minus the tag.)

### 3. LIFO is a feature

`release` pushes to the head; `acquire` pops from the head — so the slot
you get is the one most recently returned. Deliberate: its cache lines
are the warmest of any free slot. A FIFO queue of free slots would
round-robin through the whole pool, touching cold memory on every cycle.
The tests pin LIFO as spec, and the benchmark's churn workload (acquire/
release in a tight loop) is exactly where it pays.

### 4. The failure-mode catalogue (interview gold)

- **Double release**: pushes one slot onto the free list twice → two
  future `acquire`s return the same address → state corruption at a
  distance. Debug builds of real pools add a checked mode; ours documents
  it as a precondition (and ASan catches the use-after-destroy half).
- **Release a foreign pointer**: corrupts the list head. Real pools
  assert `owns(p)`; ours provides `owns()` so you *can*.
- **Exhaustion policy**: nullptr (ours — caller decides), grow (a pool of
  pools; now allocation is back), or block (a different tool entirely).
- **The ABA shadow**: a slot released and re-acquired reuses an address a
  stale pointer may still name. Single-threaded, that's a dangling-pointer
  bug; multi-threaded with lock-free lists it becomes the classic ABA
  problem — Module 8 meets it properly. Remember this union when you get
  there.

## Assignment spec

`include/course/pool.hpp` (header-only):

- `explicit ObjectPool(std::size_t capacity)` — one upfront slot array
  (`new Slot[n]`), all slots threaded onto the free list. Non-copyable,
  non-movable (given).
- `template <typename... Args> T* acquire(Args&&...)` — pop a slot,
  placement-new a `T` (forwarding args); `nullptr` when exhausted.
- `void release(T* p)` — destroy `*p`, push the slot. Precondition: `p`
  came from this pool and is live (double-release is the caller's crime).
- `in_use()`, `capacity()`, `bool owns(const void* p) const`.
- Destructor frees the slot array. Live objects are NOT destroyed —
  precondition: release everything first (the Tracker tests enforce the
  balance from the caller side).
- **LIFO**: acquire returns the most recently released slot (pinned by
  pointer-equality tests).

**Acceptance criteria:** all `m05a02.*` tests pass in `debug` and `asan`.

**Benchmark:** `m05a02_bench` (release, complete harness): churn —
acquire/release pairs at pool sizes 64 and 4096 vs `new`/`delete` of the
same object. Expect 3–10× mean improvement and a much tighter tail; also
note the pool's numbers barely move between sizes 64 and 4096 (the LIFO
head stays hot) while `new`'s wander.

## Hints

<details><summary>Hint 1 — building the initial free list</summary>
After <code>new Slot[capacity]</code>, walk backwards:
<code>slots_[i].next_free = head_; head_ = &slots_[i];</code> for i from
capacity-1 down to 0 — forward slot order for the first acquires, one
line of loop body.
</details>

<details><summary>Hint 2 — acquire and release are four lines each</summary>
acquire: if <code>head_ == nullptr</code> return nullptr; pop
(<code>Slot* s = head_; head_ = s-&gt;next_free;</code>); placement-new
into <code>s-&gt;storage</code>; count and return. release: cast
<code>T*</code> back to <code>Slot*</code> — the storage array is at
offset 0, so <code>reinterpret_cast&lt;Slot*&gt;(p)</code> is exact;
<code>p-&gt;~T()</code>; push; count.
</details>

<details><summary>Hint 3 — asan says heap-use-after-free inside release</summary>
Order of operations: destroy FIRST, then write <code>next_free</code>.
Writing the link first scribbles on a live object's bytes; if T's
destructor reads its own members (Tracker does), you just read your own
scribble. The union means the two lifetimes may not overlap by a single
instruction.
</details>

## Further reading

- Boost.Pool documentation — "Simple Segregated Storage" is this
  assignment with a beard.
- [cppreference: union](https://en.cppreference.com/w/cpp/language/union)
  — the active-member rules you're relying on.
- CppCon 2016, Herb Sutter, *"Leak-Freedom in C++... By Default"* — where
  pools sit in the ownership story.
- Game Programming Patterns, ch. *Object Pool* (free online) — same
  pattern, different latency-critical industry.
