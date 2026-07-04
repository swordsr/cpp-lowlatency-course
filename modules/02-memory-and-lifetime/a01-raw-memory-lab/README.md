# a01 — Raw Memory Lab

**Estimated effort:** 4–6 hours.

## Learning objectives

- Allocate and free with `new`/`delete`/`new[]`/`delete[]` and know exactly
  what each does.
- Do pointer arithmetic consciously: `p + i`, `last - first`, half-open
  ranges.
- Name the memory-bug taxonomy (leak, use-after-free, overflow, double
  free, mismatched delete) and recognize each in code *and* in an ASan
  report.
- Treat "the tests pass" and "the code is correct" as different claims.

## THEORY

### 1. Two allocations, two lifetimes

```cpp
void f() {
    int a = 7;             // automatic: dies at }
    int* p = new int(7);   // dynamic: dies at delete p — wherever that is
    delete p;
}
```

Automatic (stack) storage costs one stack-pointer adjustment for the whole
frame and frees itself. Dynamic (heap) storage is a negotiation with the
allocator: `new int(7)` calls `operator new(4)` — free lists, size classes,
maybe an `mmap` — then constructs an `int` there. You now hold the only
record of that memory's existence. Lose the pointer: leak. Free it twice:
heap corruption. Use it after freeing: anything at all.

That "anything at all" is **undefined behavior**, and it's worth being
precise about what it means: the standard places *no requirements* on the
program. Not "returns garbage" — the optimizer is entitled to assume UB
never happens and transform your code on that assumption. UB is a
compile-time contract violation with runtime symptoms that appear far from
the cause. This is why sanitizers exist: they turn "anything at all" into a
deterministic, pinpointed report.

### 2. Arrays: `new[]`, `delete[]`, and pointer arithmetic

```cpp
double* xs = new double[n]();   // () value-initializes: all zeros
xs[i]                            // == *(xs + i): moves i*sizeof(double) bytes
double* end = xs + n;            // one-past-the-end: valid to POINT at,
                                 // UB to dereference
delete[] xs;                     // [] is not optional
```

C++ ranges are half-open `[first, last)` — you met this with iterators in
Module 1; pointers into arrays *are* iterators, the original ones.
`last - first` is the element count. `new T[n]` records n somewhere
(typically a header before the block) so that `delete[]` can destroy n
elements and free the true block start; plain `delete` on it is UB, not a
style issue.

One more rule that surprises people: pointer arithmetic is only defined
*within* (or one-past) a single array object. Comparing or subtracting
pointers into unrelated allocations is UB, not just meaningless.

### 3. Reading an ASan report

Run the `asan` preset on a use-after-free and you'll get three stack
traces: **where the bad access happened**, **where the memory was freed**,
and **where it was allocated**. That triple answers almost every memory-bug
investigation. Learn the vocabulary now: `heap-use-after-free`,
`heap-buffer-overflow`, `alloc-dealloc-mismatch`, `attempting double-free`
— the same names an interviewer will use.

> **macOS note:** LeakSanitizer isn't available on Apple Silicon macOS, so
> `bugs.cpp`'s leak is caught *functionally* — the test counts live objects
> via a static counter. On Linux you'd get leaks for free:
> `ASAN_OPTIONS=detect_leaks=1` and a report at exit. Remember that
> difference; it's a fair interview trivium.

### 4. Why you'll never write this style for real

After this assignment, raw owning pointers go in a drawer: a02–a04 build
the RAII wrappers that make these bugs *unrepresentable*. The lab exists
because (a) wrappers are built out of exactly these primitives, (b) you'll
read decades of production code written in this style, and (c) interviewers
probe the primitives, not the wrappers.

## Assignment spec

Two parts. Part 1 you implement; Part 2 you fix.

**Part 1 — `include/course/raw.hpp` + `src/raw.cpp`:**

- `std::int64_t sum(const int* first, const int* last)` — sum of the
  half-open range; empty range → 0.
- `class RawBuffer` — owns `new char[n]` (zero-filled):
  - `explicit RawBuffer(std::size_t n)`; `size()`; `data()` (const and
    non-const overloads).
  - `resize(std::size_t new_n)` — new zero-filled allocation, copies the
    first `min(old, new)` bytes, frees the old one.
  - Non-copyable, non-movable (declared for you); destructor frees.

**Part 2 — fix `src/bugs.cpp`.** Five short functions, each with a classic
bug, each covered by tests. Your job: make all tests pass **in the `asan`
preset**. Do not change function signatures or `bugs.hpp`. Several pass in
plain `debug` right now — that's the lesson. Run both:

```sh
ctest --preset debug -R m02a01   # some bugs hide here
ctest --preset asan  -R m02a01   # none hide here
```

**Acceptance criteria:** all `m02a01.*` tests pass in `debug` **and**
`asan`. Before fixing each bug, run it under `asan` and read the report —
identifying the bug class from the report is the skill being trained.

## Hints

<details><summary>Hint 1 — RawBuffer::resize</summary>
Allocate-copy-swap-free, in that order: <code>new char[new_n]()</code>,
<code>std::memcpy</code> of <code>std::min(size_, new_n)</code> bytes,
<code>delete[]</code> the old pointer, then update members. Doing the
<code>delete[]</code> first is the use-after-free you'll fix in Part 2.
</details>

<details><summary>Hint 2 — identifying the five bugs</summary>
One missing deallocation, one access past the end of an allocation, one
read of freed memory, one <code>delete</code>/<code>delete[]</code>
mismatch, one path that frees twice. The ASan report names each class in
its first line.
</details>

## Further reading

- *A Tour of C++* ch. 5.2.1; *Effective Modern C++* intro to Items 18–22
  (read fully in a03).
- [AddressSanitizer wiki](https://github.com/google/sanitizers/wiki/AddressSanitizer)
  — skim "How it works": shadow memory, redzones, quarantine.
- John Regehr, [A Guide to Undefined Behavior in C and C++](https://blog.regehr.org/archives/213)
  — the classic three-parter; part 1 is required reading.
