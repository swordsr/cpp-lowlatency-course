# a03 — UniquePtr: Ownership as a Type

**Estimated effort:** 5–7 hours.

## Learning objectives

- Implement the full semantics of `std::unique_ptr`: sole ownership,
  transfer by move, `release`/`reset`, custom deleters.
- Demonstrate that the abstraction costs nothing: same size as a raw
  pointer, same generated code.
- Meet class templates and `[[no_unique_address]]`/EBO ahead of their
  formal treatment (Modules 3–4).

## THEORY

### 1. Ownership belongs in the type system

a01 and a02 taught you the discipline: every heap allocation has exactly
one owner responsible for freeing it. `unique_ptr` moves that discipline
out of your head and into the compiler:

```cpp
UniquePtr<Order> a{new Order{}};
UniquePtr<Order> b = a;             // does not compile — copying is deleted
UniquePtr<Order> c = std::move(a);  // explicit, visible transfer
```

A codebase using `unique_ptr` everywhere makes ownership *audit-able by
grep*: `std::move` marks every transfer, function signatures document
intent (`UniquePtr<T>` parameter = "I take ownership"; `T*`/`T&` = "I just
use it"). This is the vocabulary-type idea: `unique_ptr` says nothing about
*what* the object is and everything about *whose problem* it is.

### 2. Zero-overhead: prove it, don't trust it

`sizeof(UniquePtr<int>)` must equal `sizeof(int*)`, and the generated code
for construct/use/destroy must match hand-written new/use/delete. When your
implementation is green, paste it into Compiler Explorer (ARM64 clang,
`-O2`) next to the raw-pointer version and diff the assembly — they should
be instruction-identical. "What does `unique_ptr` cost over a raw
pointer?" is a stock interview question; the strong answer is "nothing, and
I've checked" (plus the one footnote: as a *by-value function parameter*
the ABI passes it in memory rather than a register because it has a
non-trivial destructor — trivia, but it impresses).

### 3. The deleter is a policy — and it must be free when empty

`UniquePtr<T, D>` carries a deleter: called with the raw pointer instead of
`delete`. That's how one template wraps `fclose`-owned files, arena-owned
objects (Module 5), or anything with a custom release protocol. The default
deleter is an *empty class* — no data members — but C++ says every member
subobject occupies ≥1 byte, so the naive `T* ptr_; D deleter_;` layout is
16 bytes, not 8. Two escapes:

- **`[[no_unique_address]]`** (C++20): tells the compiler this member may
  share an address with others if it's empty. The modern answer.
- **Empty Base Optimization**: inherit from the deleter instead of storing
  it — empty *bases* were always allowed to occupy zero bytes. The
  pre-C++20 idiom; you'll find it inside every real standard library.

The `SizeIsOnePointer` test fails until you apply one of them. Module 3
digs into why the 1-byte rule exists at all.

### 4. API surface, quickly

`release()` hands you the raw pointer and *disowns* it (no destroy — the
caller now owns). `reset(p)` destroys the current pointee, takes `p`.
`swap` exchanges. `operator bool` tests emptiness. These small semantics
are exactly what the tests pin down — read them as the reference manual.

## Assignment spec

Everything in `include/course/unique_ptr.hpp` (header-only template — note
that templates live in headers; Module 4 explains why). The skeleton
provides the members and the trivial accessors (`get`, `*`, `->`,
`operator bool`); you implement the ownership machinery:

- `DefaultDelete<T>::operator()` — `delete`.
- Constructors (`explicit` from `T*`; from `T*` + deleter), destructor.
- Move constructor / move assignment (`noexcept`; self-move-safe;
  moved-from is empty). Copying is deleted (given).
- `release()`, `reset(T* p = nullptr)`, `swap(UniquePtr&)`.
- `MakeUnique<T>(args...)` — construct a `T` from forwarded arguments
  (recipe in Hint 2; perfect forwarding gets its full treatment in
  Module 4).
- Layout: `sizeof(UniquePtr<int>) == sizeof(int*)` (Hint 3).

**Acceptance criteria:** all `m02a03.*` tests pass in `debug` and `asan`.
Then do the Compiler Explorer diff from THEORY §2 — seriously, do it; it's
the payoff of the whole assignment.

## Hints

<details><summary>Hint 1 — destruction discipline</summary>
Every path that drops a pointee goes through the deleter exactly once:
destructor, reset, move-assignment (the OLD pointee). release() is the one
that doesn't. The Tracker tests count destructor calls and will catch any
imbalance either way.
</details>

<details><summary>Hint 2 — MakeUnique</summary>
<pre>template &lt;typename T, typename... Args&gt;
UniquePtr&lt;T&gt; MakeUnique(Args&&... args) {
    return UniquePtr&lt;T&gt;{new T(std::forward&lt;Args&gt;(args)...)};
}</pre>
Take it on faith this round; Module 4 makes you derive it.
</details>

<details><summary>Hint 3 — the 16-byte problem</summary>
Read THEORY §3 again; the one-line fix is
<code>[[no_unique_address]] D deleter_;</code>. Verify with
<code>static_assert</code> locally, then check what happens if the deleter
HAS state (a lambda with a capture) — size grows, correctly.
</details>

## Further reading

- *Effective Modern C++* Items 18, 21, 22.
- [CppCoreGuidelines R.20–R.37](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#r-resource-management)
  — the ownership rules production code follows.
- cppreference: [no_unique_address](https://en.cppreference.com/w/cpp/language/attributes/no_unique_address).
- CppCon 2019, Chandler Carruth, *"There Are No Zero-cost Abstractions"* —
  the by-value-parameter ABI caveat, from the horse's mouth.
