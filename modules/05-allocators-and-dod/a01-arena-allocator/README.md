# a01 — Arena Allocator: Allocation as a Pointer Bump

**Estimated effort:** 5–7 hours.

## Learning objectives

- Implement bump-pointer allocation with correct alignment handling.
- Internalize the arena contract: common lifetime, `reset()` instead of
  per-object free, destructors traded away knowingly.
- Write a standard allocator adapter and plug it into `std::vector`.
- Measure the gap between `new` and a bump in a benchmark you can defend.

## THEORY

### 1. What `new` really costs — and what an arena refuses to pay

`operator new` solves a hard general problem: any size, any order of
allocation and free, from every thread, forever, without fragmenting. The
machinery that buys — size-class bins, free-list searches, thread caches,
occasional `mmap` — is why a single allocation is "tens of nanoseconds,
usually" with a fat tail. Trading systems care about the *tail*: p50 of
20ns doesn't matter if p999 is 20µs and your tick-to-trade budget is 2µs.

The arena deletes the problem instead of solving it. If a group of
allocations shares one lifetime — everything parsed from one packet,
everything computed in one strategy cycle — then:

```text
allocate(n):  aligned_up(cursor); cursor += n        // ~2 instructions
reset():      cursor = start                          // 1 write, frees ALL
```

No per-object free. No free list. No fragmentation, ever, because there is
no hole to fragment. Allocation is deterministic to the instruction. This
is the single highest-leverage allocation pattern in low-latency code —
per-message arenas that reset after each packet.

### 2. The two prices you pay

**No individual deallocation.** `deallocate` on an arena is a no-op by
*design*. If object lifetimes aren't actually shared, an arena is the
wrong tool (that's a02's pool).

**No destructors on reset.** `reset()` rewinds the cursor; it does not —
cannot — know what objects live in the buffer. Arena-allocated objects
must be trivially destructible *or* the owner must run destructors before
resetting. Your `create<T>()` helper placement-news into the arena
(m02a04 skills); the tests pin that `reset()` does NOT destroy — this is
a contract to document loudly, not a bug.

### 3. Alignment: the part everyone gets wrong once

Your Module 3 `aligned_up` comes home. `allocate(size, align)` must return
a pointer whose address is a multiple of `align` — the bump cursor after a
13-byte string allocation is misaligned for the `int64` that comes next;
handing it out anyway is UB (and on ARM64, occasionally a real fault for
atomics/SIMD, not just slow). Bump the cursor to `aligned_up(cursor,
align)` *first*, then carve. Waste from alignment gaps is accepted and
untracked — arenas trade bytes for cycles all day.

### 4. The allocator adapter: renting your arena to std::vector

Standard containers take an allocator template parameter — a tiny policy
class the container calls instead of `new`/`delete`:

```cpp
course::Arena arena{1 << 20};
std::vector<int, course::ArenaAllocator<int>> v{ArenaAllocator<int>{arena}};
v.reserve(1000);   // memory carved from YOUR arena
```

The C++17 minimum: `value_type`, `allocate(n)` (n *objects*, not bytes —
multiply), `deallocate` (here: no-op), a converting constructor from
`ArenaAllocator<U>` (containers rebind to allocate their internal types),
and `operator==` (true iff same arena — this tells containers whether two
allocators can free each other's memory). Throw `std::bad_alloc` on
exhaustion in the adapter: containers speak exceptions, and a nullptr
would just crash them less politely.

> **Production note:** `std::pmr::monotonic_buffer_resource` IS this
> assignment, standardized (type-erased via virtual calls instead of a
> template parameter). Build yours, then read its docs and feel seen.

## Assignment spec

`include/course/arena.hpp` (header-only):

- `Arena(std::size_t capacity_bytes)` — one upfront heap block
  (`::operator new`), freed in the destructor. Non-copyable, non-movable
  (given).
- `void* allocate(std::size_t size, std::size_t align)` — aligned bump;
  `nullptr` when it doesn't fit (never throws — the raw interface is
  hot-path style). `align` is a power of two.
- `template <typename T, typename... Args> T* create(Args&&...)` —
  allocate + placement-new; `nullptr` if out of space.
- `reset()` — cursor to start; no destructors (THEORY §2).
- `used()`, `capacity()`, `bool owns(const void* p) const` — pointer
  inside the buffer?
- `ArenaAllocator<T>` — the adapter from THEORY §4; throws
  `std::bad_alloc` when the arena is exhausted.

**Acceptance criteria:** all `m05a01.*` tests pass in `debug` and `asan`.

**Benchmark:** `m05a01_bench` (release, complete harness): (1) building
1000-node linked structures — arena `create` vs `new`/`delete`, expect
5–15× on Apple Silicon and, more importantly, a *much* tighter
distribution (run with `--benchmark_repetitions=10` and compare stddevs);
(2) `std::vector` growth through your adapter vs the default allocator —
smaller gap (vector already amortizes); note in a comment WHY the two
gaps differ.

## Hints

<details><summary>Hint 1 — allocate, in four lines</summary>
Cursor as <code>std::size_t offset_</code>, not a pointer — the math
reads better: <code>aligned = aligned_up(offset_, align)</code>; if
<code>aligned + size &gt; capacity_</code> return nullptr; result is
<code>buffer_ + aligned</code>; <code>offset_ = aligned + size</code>.
</details>

<details><summary>Hint 2 — exhaustion off-by-one</summary>
The check is <code>aligned + size &gt; capacity_</code>, not
<code>&gt;=</code> — an allocation may END exactly at capacity. The
<code>FillsToExactCapacity</code> test is aimed at this fencepost, and at
overflow: with huge <code>size</code>, <code>aligned + size</code> can
wrap; test <code>size &gt; capacity_ - aligned</code> instead.
</details>

<details><summary>Hint 3 — the adapter's rebind constructor</summary>
<pre>template &lt;typename U&gt;
ArenaAllocator(const ArenaAllocator&lt;U&gt;& other) : arena_(other.arena_) {}</pre>
plus a <code>friend</code> declaration (or a public getter) so different
instantiations can see each other's <code>arena_</code>. Forgetting this
constructor produces a wall of vector-internals errors — read them once
for sport, then add it.
</details>

## Further reading

- [std::pmr::monotonic_buffer_resource](https://en.cppreference.com/w/cpp/memory/monotonic_buffer_resource)
  — the standard's arena.
- CppCon 2017, John Lakos, *"Local ('Arena') Memory Allocators"* — the
  case, with data.
- CppCon 2015, Andrei Alexandrescu, *"std::allocator Is to Allocation
  what std::vector Is to Vexation"* — allocator design beyond the minimum.
- [Abseil Tip #176](https://abseil.io/tips/176) — prefer return values;
  relevant to your create<T> API thinking.
