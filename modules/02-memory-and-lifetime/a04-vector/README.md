# a04 — Vector: Uninitialized Memory Done Right

**Estimated effort:** 8–12 hours. The capstone of this module — budget a
weekend.

## Learning objectives

- Separate *allocating storage* from *constructing objects* — placement
  `new`, explicit destructor calls, and why `new T[n]` can't build a vector.
- Implement amortized-O(1) growth and reason about the invalidation rules
  it forces on users.
- Use move-vs-copy correctly during reallocation and see why `noexcept`
  moves matter (`std::move_if_noexcept`).
- Survive the classic aliasing trap: `v.push_back(v[0])`.

## THEORY

### 1. Storage and objects are different things

`new T[cap]` allocates AND constructs `cap` objects. A vector with capacity
16 and size 3 has three objects and thirteen slots of *raw storage* — no
constructors have run there; running destructors there would be UB. So a
real vector needs the two halves separately:

```cpp
// storage, no objects:
T* p = static_cast<T*>(::operator new(cap * sizeof(T)));
// object #i, constructed into existing storage ("placement new"):
new (p + i) T(args...);
// end object #i's life without freeing storage:
(p + i)->~T();
// free storage that holds no objects:
::operator delete(p);
```

Those four lines are the entire magic. Everything else in this assignment
is bookkeeping about which slots currently hold objects — the invariant is
"exactly `[data_, data_ + size_)` are alive". Every member function you
write should be checked against that invariant on entry and exit, including
mid-function when something throws. (The standard library wraps these
operations as `std::construct_at` / `std::destroy_at` / allocator traits —
after building it raw once, you're allowed to know that.)

### 2. Growth: why doubling, exactly

Appending into a full buffer means: allocate bigger, transfer n elements,
destroy the old n, free. That transfer is O(n) — so `push_back` can't
always be O(1). But if capacity *doubles*, a sequence of n pushes does at
most n + n/2 + n/4 + … ≈ 2n element-transfers total: **amortized O(1)**,
worst-case O(n). Growing by a constant (+16 each time) gives amortized
O(n) per push — quadratic total; a favorite interview trap. This course's
spec: `new_cap = max(1, 2 * cap)`, and `reserve(n)` allocates exactly `n`.

The price of relocation: **every pointer, reference, and iterator into the
vector is invalidated** by growth. You wrote that rule in Module 1 as a
user; now you're the one breaking the pointers.

### 3. Move if noexcept — the subtle center of this assignment

During reallocation you transfer elements to the new buffer. Moving is
fast; but if a *move constructor can throw* and throws halfway, the old
buffer has already had k elements gutted — you can't roll back, and the
vector's "strong guarantee" (a failed push_back leaves the vector
untouched) is broken. The standard's compromise, `std::move_if_noexcept`:
move when the move ctor is `noexcept`, otherwise fall back to copying
(copies leave the source intact, so a throw mid-transfer can be rolled
back by destroying the partial copies). Use it in your relocation loop.
The test suite instruments a type and counts: noexcept-movable elements
must be *moved* (zero copies) during growth. Now you know why every move
constructor you've written since a02 says `noexcept` — and why forgetting
it silently turns container growth into deep-copying. That's a
production-grade performance bug interviewers love.

### 4. The aliasing trap

```cpp
v.push_back(v[0]);   // must work — and your first version won't
```

If the vector is full, the reference `v[0]` points into the buffer you're
about to free. Construct-into-new-buffer *first*, then destroy/free the
old one. `std::vector` guarantees this works; so does this course's spec.

## Assignment spec

`Vector<T>` in `include/course/vector.hpp` (header-only). Members and
signatures are given; every body is yours. Semantics:

- Invariant: objects alive exactly in `[data_, data_+size_)`; raw storage
  through `data_+capacity_`. Allocation via `::operator new`/`::operator
  delete`; construction via placement new; destruction explicit.
- `push_back` (copy and move overloads), `emplace_back(args...) -> T&`
  (constructs in place, zero copies/moves of T), `pop_back`, `clear`
  (destroys, keeps capacity), `reserve` (exact, never shrinks), `size`,
  `capacity`, `empty`, `operator[]` (unchecked), `front`/`back`,
  `begin`/`end`, full Rule of Five, `swap`.
- Growth `max(1, 2*cap)`; relocation uses `std::move_if_noexcept`;
  `push_back(v[0])` on a full vector works.
- Must support move-only element types (`Vector<std::unique_ptr<int>>` —
  the tests instantiate it; lazy template instantiation is your friend and
  the handout question bank asks why).

**Acceptance criteria:** all `m02a04.*` tests pass in `debug` and `asan`.
The Item-counting tests are strict: exact constructor/destructor balance,
zero copies where moves are required.

**Benchmark:** `m02a04_bench` (release): (1) 1M `push_back` with vs
without `reserve` — expect the reserve version ~1.5–3× faster and the gap
to be entirely allocator+relocation cost; (2) your `Vector<int64_t>` vs
`std::vector<int64_t>` — you should be within ~10–20%; if you're 2× off,
look at your growth loop's inner branch. Record both in a comment.

## Hints

<details><summary>Hint 1 — write reallocate() once, use it everywhere</summary>
A private <code>reallocate(new_cap)</code>: new storage, loop
<code>new (dst+i) T(std::move_if_noexcept(data_[i]))</code>, destroy old
elements, free old storage, update members. reserve and the push_back
growth path both call it.
</details>

<details><summary>Hint 2 — the aliasing test keeps failing</summary>
In <code>push_back(const T&)</code> on a full vector: construct the copy
into the NEW buffer before destroying the old one — i.e. don't call
<code>reallocate()</code> then copy; fold the append into the relocation,
or copy the element to a stack temporary first (correct, one extra move,
also accepted by the tests).
</details>

<details><summary>Hint 3 — destructor order bugs under asan</summary>
<code>clear()</code> then free is the safe destructor recipe. If asan
reports heap-use-after-free inside a Tracker-ish destructor, you freed
storage before destroying elements — or destroyed twice after a
pop_back/clear interaction. The invariant from THEORY §1 pinpoints it.
</details>

## Further reading

- *Effective Modern C++* Item 14 (noexcept), Item 41.
- [std::move_if_noexcept](https://en.cppreference.com/w/cpp/utility/move_if_noexcept)
  and [vector::push_back](https://en.cppreference.com/w/cpp/container/vector/push_back)
  exception guarantees — read the fine print you now implement.
- CppCon 2016, Chandler Carruth, *"High Performance Code 201: Hybrid Data
  Structures"* — where this goes next (Module 5's SmallVector).
- Folly's [FBVector docs](https://github.com/facebook/folly/blob/main/folly/docs/FBVector.md)
  — a production vector's war stories about growth factors and relocation.
