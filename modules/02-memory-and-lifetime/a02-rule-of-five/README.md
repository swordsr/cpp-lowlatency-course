# a02 — Rule of Five: A Dynamic Array

**Estimated effort:** 4–6 hours.

## Learning objectives

- Write all five special member functions for a resource-owning type and
  know the failure mode each one prevents.
- Handle self-assignment and moved-from states correctly.
- Explain move semantics mechanically: what an rvalue reference binds to,
  what a move constructor actually does, what state it leaves behind.
- Know when NOT to write any of this (Rule of Zero).

## THEORY

### 1. The compiler writes six functions — and for owners, they're wrong

For `class DynArray { double* data_; size_t size_; };` the compiler
generates a copy constructor, copy assignment, move constructor, move
assignment, destructor, and default constructor. The generated copies copy
*the pointer* — two objects then own one buffer; whichever destructs second
frees freed memory (a01 taught you what that means). The generated
destructor doesn't free at all. Memberwise semantics are correct for
*values*; your members here include a raw owning pointer, which is not a
value — it's a responsibility.

**Rule of Five:** if you need to hand-write ANY of {destructor, copy ctor,
copy assign, move ctor, move assign}, you almost certainly need to write
all five, deliberately. **Rule of Zero:** structure classes so members
manage themselves (string, vector, UniquePtr…) and write none. Production
code is overwhelmingly Rule-of-Zero; the handful of Rule-of-Five types
(this one, a03's, a04's) sit at the bottom making that possible.

### 2. The five, one failure mode each

```cpp
~DynArray()                                // absent: leak
DynArray(const DynArray& other)            // shallow: double free
DynArray& operator=(const DynArray& other) // + must free own buffer first: leak
                                           // + self-assignment: use-after-free
DynArray(DynArray&& other) noexcept        // absent: every "move" silently copies
DynArray& operator=(DynArray&& other) noexcept
```

Copy assignment is the treacherous one — it must (1) survive `a = a`,
(2) free the old buffer, (3) deep-copy the new. The classic implementation
orders these wrong and frees the buffer it's about to copy from.
Copy-and-swap (`operator=(DynArray other)` by value, then swap) gets all
three right by construction; implementing it that way is acceptable, and
understanding *why* it's self-assignment-safe is question-bank material.

### 3. Move semantics, mechanically

`DynArray&& ` is a reference that only binds to expiring objects —
temporaries, or things you've explicitly marked expiring with
`std::move`. A move constructor is just a constructor that exploits that
knowledge: *steal* the pointer, null the source.

```cpp
DynArray(DynArray&& other) noexcept
    : data_{std::exchange(other.data_, nullptr)},
      size_{std::exchange(other.size_, 0)} {}
```

No allocation, no copying — two pointer-sized writes. The moved-from object
must remain *destructible and assignable* (ours is empty: `size() == 0`,
`data() == nullptr`); beyond that, callers may not look at it.

Mark moves `noexcept`. It's not politeness: `std::vector` growth uses
`std::move_if_noexcept` — a throwing-move type gets *copied* on every
reallocation, silently. You'll prove this to yourself in a04.

### 4. Copy elision: why `return arr;` is free

`DynArray make() { DynArray a{...}; return a; }` performs zero copies and
zero moves in practice: the compiler constructs `a` directly in the
caller's storage (NRVO; guaranteed elision for `return DynArray{...}`).
Writing `return std::move(a);` *disables* NRVO and forces a real move —
a pessimization compilers warn about. Rule: return locals by value, plainly.

## Assignment spec

Implement `DynArray` (an owning, fixed-length array of `double`) in
`include/course/dyn_array.hpp` / `src/dyn_array.cpp`:

- `DynArray()` — empty (nullptr, size 0).
- `explicit DynArray(std::size_t n)` — n zeros.
- `DynArray(std::initializer_list<double>)`.
- The full Five: deep copy ctor/assign; move ctor/assign (`noexcept`);
  destructor. Self-assignment (copy AND move) must be safe. Moved-from ==
  empty.
- `size()`, `data()`, `operator[]` (const + non-const, unchecked),
  `begin()`/`end()` (raw pointers — this makes range-for and
  `<algorithm>` work).

**Acceptance criteria:** all `m02a02.*` tests pass in `debug` and `asan` —
the double-free and leak mistakes only announce themselves under `asan`.

## Hints

<details><summary>Hint 1 — where to start</summary>
Destructor and the two constructors first, then copy ctor, then the moves
(<code>std::exchange</code>), copy assignment LAST — it's the one with
traps. Run the asan preset after each.
</details>

<details><summary>Hint 2 — copy assignment traps</summary>
Either guard <code>this == &other</code> then free-allocate-copy, or use
copy-and-swap. If <code>SelfAssignmentIsSafe</code> fails under asan with
heap-use-after-free, you freed before copying.
</details>

<details><summary>Hint 3 — initializer_list</summary>
It has <code>.size()</code>, <code>.begin()</code>, <code>.end()</code>;
allocate then <code>std::copy</code>. Note
<code>DynArray{3}</code> vs <code>DynArray(3)</code> select different
constructors — a real C++ gotcha, and one of the tests pins it.
</details>

## Further reading

- *Effective Modern C++* Items 17, 23–25, 29, 41.
- Howard Hinnant, [Everything You Ever Wanted to Know About Move Semantics](https://www.slideshare.net/ripplelabs/howard-hinnant-accu2014)
  — from the person who designed it.
- [GotW #11: Object Identity](https://herbsutter.com/gotw/_011/) (self-assignment).
- cppreference: [copy elision](https://en.cppreference.com/w/cpp/language/copy_elision).
