# Module 2 — Memory & Object Lifetime

**Time budget:** ~2½ weeks. Four assignments, in strict order.

This is the module where C++ stops being "Java with different syntax." You
take over the two jobs the JVM did silently: deciding **where objects live**
and **when they die**. The arc:

1. **a01** — raw `new`/`delete` and the ways it goes wrong (with ASan as
   your safety net while you build intuition);
2. **a02** — encapsulating ownership: the Rule of Five on a dynamic array;
3. **a03** — ownership as a *vocabulary type*: implement `UniquePtr`;
4. **a04** — the real thing: a `Vector` that separates allocation from
   construction, the pattern behind every container and pool you'll build
   for the rest of the course.

After this module you should never again write a raw owning `new` outside a
constructor — and you'll know exactly what the wrappers cost (nothing).

## Assignments

| # | Assignment | What you'll touch |
|---|------------|-------------------|
| 1 | [a01-raw-memory-lab](a01-raw-memory-lab/) | new/delete, pointer arithmetic, UB taxonomy, ASan |
| 2 | [a02-rule-of-five](a02-rule-of-five/) | copy/move ctors & assignment, destructors, self-assignment |
| 3 | [a03-unique-ptr](a03-unique-ptr/) | ownership transfer, custom deleters, zero-overhead abstraction |
| 4 | [a04-vector](a04-vector/) | placement new, uninitialized memory, exception safety, growth |

## Interview questions

<details>
<summary>"What's the difference between the stack and the heap in C++, and what does allocation actually cost?"</summary>

Stack: bump of the stack pointer at function entry — effectively free,
scoped lifetime, limited size (~8MB default). Heap: `new` calls into the
allocator (free-list/size-class machinery, possibly a syscall to grow the
heap), returns storage with dynamic lifetime you must end explicitly.
Costs: allocation itself (tens of ns, unbounded worst case), *and* the
indirection/cache miss every later access pays, *and* eventual `delete`.
Low-latency systems allocate up front and never on the hot path (Module 5).
</details>

<details>
<summary>"Rule of Zero / Three / Five — state them and say which you aim for."</summary>

If a class owns a resource, the destructor, copy constructor, and copy
assignment must all be user-defined (Three); C++11 adds move constructor
and move assignment (Five). If a class owns nothing raw — members are
strings/vectors/smart pointers — define none of them (Zero): the compiler-
generated memberwise versions are correct by construction. Aim for Zero;
Five belongs in the small number of resource-wrapper types like the ones in
this module.
</details>

<details>
<summary>"What actually happens when you write <code>new Widget(42)</code>?"</summary>

Two things, in order: `operator new(sizeof(Widget))` obtains raw storage
(throws `std::bad_alloc` on failure), then the `Widget(42)` constructor runs
in that storage. `delete p` runs them in reverse: destructor, then
`operator delete`. `malloc`/`free` do only the storage half — no
constructors/destructors — which is why they're not interchangeable and why
`delete`-ing a `malloc`'d pointer is UB.
</details>

<details>
<summary>"Why are <code>delete p</code> and <code>delete[] p</code> different?"</summary>

`new T[n]` typically stashes the element count (often just before the
returned address) so `delete[]` can run n destructors and free the true
block start. Plain `delete` on that pointer runs one destructor and hands
the wrong address/bookkeeping to the allocator: UB. ASan reports it as
alloc-dealloc-mismatch.
</details>

<details>
<summary>"What does <code>std::move</code> do?"</summary>

Nothing, at runtime — it's a cast to rvalue reference. It *permits* the
compiler to select move overloads, which are what actually pilfer the
guts. A moved-from standard object is left valid-but-unspecified; your own
types should leave a cheap, destructible, assignable state (usually
"empty"). Follow-ups to be ready for: `std::move` on a `const` object
silently copies; returning a local by value doesn't need `std::move`
(elision/NRVO — and writing it can *pessimize*).
</details>

<details>
<summary>"How does AddressSanitizer work, roughly, and what does it catch?"</summary>

Compiler instruments every load/store to check shadow memory (1 byte of
shadow per 8 app bytes) marking valid/invalid regions; allocator quarantines
freed blocks and poisons redzones around allocations. Catches heap/stack
buffer overflows, use-after-free/-return/-scope, double free,
alloc-dealloc mismatch — at ~2× slowdown, so it runs in CI, not production.
It does NOT catch uninitialized reads (MSan) or races (TSan). (On this
course's macOS setup, leak detection is unavailable — we test for leaks
functionally instead.)
</details>
