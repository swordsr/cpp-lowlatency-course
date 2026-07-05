# Module 5 — Allocators & Data-Oriented Design

**Time budget:** ~2 weeks. Three assignments.

Module 2 taught you what `new` does. This module teaches you why hot paths
refuse to call it — and what they do instead. The low-latency allocation
doctrine in one line: **allocate everything up front, recycle in O(1),
lay data out for the traversal you actually do.**

1. **a01** — the arena: bump-pointer allocation, reset-don't-free, and the
   standard allocator adapter that lets `std::vector` live in your arena;
2. **a02** — the object pool: fixed slots, an intrusive free list threaded
   through the free slots themselves, LIFO reuse for cache warmth;
3. **a03** — layout as an optimization: a `SmallVector` that dodges the
   heap entirely, and the AoS→SoA transform that turns your data-infra
   instincts (columnar storage!) into nanoseconds.

From here on, benchmarks stop being a ritual and start being the point:
each assignment's bench harness exists to make a specific effect visible,
and the handout tells you what number to look for.

## Assignments

| # | Assignment | What you'll touch |
|---|------------|-------------------|
| 1 | [a01-arena-allocator](a01-arena-allocator/) | bump allocation, alignment math, std allocator adapter |
| 2 | [a02-object-pool](a02-object-pool/) | intrusive free lists, unions & lifetime, LIFO recycling |
| 3 | [a03-small-vector-and-soa](a03-small-vector-and-soa/) | inline storage (SBO), AoS vs SoA, data-oriented design |

## Interview questions

<details>
<summary>"Why is <code>new</code> banned on your hot path?"</summary>

Three reasons, in interview order: (1) *latency* — the general-purpose
allocator walks free lists / size-class caches and may take a lock or a
syscall (`mmap`/`sbrk`); tens of ns typical, unbounded worst case; (2)
*determinism* — the same call can cost 20ns or 20µs depending on heap
state; tail latency is what kills trading systems; (3) *locality* —
scattered allocations fragment the working set. The fix is not "a faster
malloc", it's changing the contract: preallocate, then bump or recycle.
</details>

<details>
<summary>"Arena vs pool vs free list — when do you use which?"</summary>

Arena (bump): allocations with a COMMON lifetime — build everything for
one cycle/message/request, then `reset()` frees all of it in one pointer
write. Individual free is impossible by design. Pool: same-sized objects
with INDEPENDENT lifetimes — O(1) acquire/release by threading a free
list through the idle slots. General free list / size classes: what
malloc already is; if you need it on the hot path, redesign. Classic
combo: pools for orders, arenas for per-packet scratch.
</details>

<details>
<summary>"How do you give a <code>std::vector</code> your custom memory?"</summary>

The allocator template parameter: a minimal C++17 allocator is
`value_type`, `allocate(n)`, `deallocate(p, n)`, equality, and a
rebinding constructor (the container allocates internal node types by
converting your `Alloc<T>` to `Alloc<Node>`). Two follow-ups they probe:
allocator *equality* semantics decide whether containers can swap/steal
buffers, and `std::pmr` is the modern type-erased spelling
(`memory_resource` + `polymorphic_allocator`) that avoids the template
infection at the cost of a virtual call per allocation.
</details>

<details>
<summary>"What is an intrusive free list and why put it INSIDE the objects?"</summary>

The pool's free slots hold no live object — so their bytes are free real
estate. Store the next-free pointer in the slot itself and the free list
costs zero extra memory and zero extra cache lines: pop = read head, push
= write one pointer. A union of {next-pointer, object storage} makes the
overlay explicit. LIFO order is deliberate: the most recently released
slot is the most likely to still be in L1.
</details>

<details>
<summary>"Explain AoS vs SoA like I'm a database person."</summary>

AoS is a row store, SoA is a column store — same tradeoff, nanosecond
scale. Summing `price*qty` over an array of 64-byte order structs drags
every field of every order through the cache to use 12 bytes: ~19% of
each cache line is useful. Store prices and quantities as separate
contiguous arrays and every fetched line is 100% payload — plus the
compiler can vectorize the loop. Cost: field scatter on insert, worse
locality when you DO touch whole records, and object identity gets
awkward. Games and HFT both converged on "hot fields columnar, cold
fields elsewhere".
</details>
