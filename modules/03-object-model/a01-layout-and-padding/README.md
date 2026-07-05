# a01 — Layout & Padding: What sizeof Isn't Telling You

**Estimated effort:** 4–6 hours.

## Learning objectives

- Explain why alignment requirements exist and predict, by hand, the exact
  byte layout the compiler will produce for any struct.
- Implement the layout algorithm yourself (`aligned_up`,
  `packed_struct_size`) and validate it against the real compiler.
- Shrink a wasteful struct by reordering members — and explain why the
  compiler is forbidden from doing this for you.
- Use the layout toolbox: `sizeof`, `alignof`, `offsetof`,
  `-fdump-record-layouts`.
- Make an empty member cost zero bytes with `[[no_unique_address]]`, and
  know the EBO idiom it replaced.

## THEORY

### 1. Alignment, and the padding it forces

Every type has an *alignment requirement*: addresses at which objects of
that type are allowed to live. `alignof(double)` is 8, so a `double` lives
only at addresses divisible by 8. Why? Hardware and ABI. A modern CPU loads
a `double` in one instruction *if* it doesn't straddle the wrong boundary —
misaligned access is slower on x86 (split loads, worse across cache lines),
can fault outright for SIMD and atomics, and was a hard crash on older ARM.
So the platform ABI (the contract that lets separately compiled code
interoperate — AAPCS64 on your Mac, System V on x86-64 Linux) simply
declares natural alignment for every fundamental type, and the compiler
guarantees it. In C++ it's undefined behavior to access a misaligned
object, full stop — even on hardware that would tolerate it.

That guarantee has a cost inside structs. The layout algorithm — which you
will implement in this assignment — is:

1. Members are laid out **in declaration order** (mostly; see §2).
2. Each member is placed at the next offset that is a multiple of its own
   `alignof`. The gap skipped over is **padding** — dead bytes.
3. The struct's alignment is the max of its members' alignments, and its
   `sizeof` is rounded up to a multiple of that (**tail padding**) — so
   that element `i+1` of an array is aligned whenever element 0 is.

Worked example, LP64 (your Mac and any Linux box you'll deploy to):

```cpp
struct Tick {
    char  flag;    // offset 0            (1 byte)
    double price;  // wants 8 → offset 8  (7 bytes of PADDING at 1..7)
    char  side;    // offset 16           (1 byte)
};                 // 17 bytes of layout, alignof == 8
                   // → sizeof rounded up to 24 (7 bytes tail padding)
```

As a byte diagram (`.` = padding):

```
offset: 0        8        16       24
        F.......PPPPPPPPS.......
        ^flag    ^price   ^side  ^end (sizeof == 24)
```

10 bytes of data, 24 bytes of struct. In an array of a million ticks,
that's 14 MB of nothing.

### 2. The fix is reordering — and only YOU are allowed to do it

Padding appears where a small member is followed by a bigger-aligned one.
The heuristic that almost always reaches the minimum: **declare members
largest-alignment first**. Re-run the algorithm on the reordered `Tick`:

```cpp
struct Tick {
    double price;  // offset 0
    char  flag;    // offset 8
    char  side;    // offset 9
};                 // 10 → rounded to 16. Was 24.
```

Descending alignment means every member lands exactly where the previous
one ended — interior padding vanishes; only tail padding can remain.

Why doesn't the compiler just do this? Because it's *not allowed to*. For
standard-layout types, C++ guarantees members are laid out in declaration
order (earlier members at lower addresses), the first member sits at offset
zero, and two standard-layout structs with a common initial sequence of
members can have that sequence read through either type. Every C interop
header, every `memcpy`-based serializer, every network protocol struct
depends on declaration order *being* the layout. A compiler that reordered
behind your back would break all of it — so the standard forbids it, and
member order becomes a design decision you own. (Once a class stops being
standard-layout — e.g. mixed access specifiers — the compiler gets some
freedom in theory; in practice, real ABIs still don't reorder.)

### 3. The toolbox: interrogating layout instead of guessing

Three operators answer every layout question:

```cpp
sizeof(T)              // total bytes, padding included
alignof(T)             // the alignment requirement
offsetof(Tick, price)  // byte offset of a member (<cstddef>;
                       // well-defined for standard-layout types)
```

While your `aligned_up`/`packed_struct_size` are under construction, check
your hand-computations against the compiler's own dump:

```sh
clang++ -std=c++20 -Xclang -fdump-record-layouts -fsyntax-only tick.cpp
```

which prints, for each record, every member's offset plus the final
`sizeof`/`alignof` — the ground truth this assignment's tests hold you to.
Use it whenever a test disagrees with your mental model.

> **Linux/x86-64 sidebar:** fundamental-type sizes are the same on ARM64
> macOS and x86-64 Linux (both LP64: pointers/`long`/`std::size_t` are 8
> bytes), so every number in this assignment holds on both. The classic
> divergence is `long double`: `alignof(long double)` is **8** on Apple
> Silicon (it's just an IEEE double there) but **16** on x86-64 Linux (80-bit
> x87 extended precision, padded to 16). A struct containing one has a
> different size per platform — one reason wire formats (§5) spell out
> `std::int64_t` and friends and never use `long double`. `char` signedness
> also flips (signed on x86-64, unsigned on ARM64) — not a layout issue,
> but the same lesson: "it worked on my Mac" is not a spec.

### 4. Empty classes: one byte, unless you say otherwise

```cpp
struct PriceTag {};
static_assert(sizeof(PriceTag) == 1);   // not 0 — by design
```

Why one byte for zero data? Because every object must have a distinct
address. If empty objects were size 0, an array `PriceTag tags[8]` would
put all eight at the same address, `&tags[0] == &tags[1]`, and pointer
arithmetic and object identity would collapse. So the language pads empty
classes to size 1 — and inside a struct, that byte then attracts alignment
padding of its own: a `double` plus an empty tag is 16 bytes, not 9.

Two escapes:

- **Empty Base Optimization (EBO):** empty *base classes* may occupy zero
  bytes (a base subobject needs no distinct address of its own). The
  pre-C++20 idiom — inherit from the empty policy instead of storing it —
  and it's all over libstdc++/libc++ internals. It works, but contorts
  your design: "has a tag" becomes "is-a tag".
- **`[[no_unique_address]]`** (C++20): mark the *member*, and if its type
  is empty the compiler may overlap it with other members — zero bytes,
  no inheritance gymnastics. You met it in m02a03's deleter; here you
  apply it yourself. (Portability footnote: MSVC ignores the standard
  spelling and needs `[[msvc::no_unique_address]]` — irrelevant for this
  course's clang/gcc world, worth knowing for interviews.)

The `TaggedValue` part of this assignment ships failing at 16 bytes; the
fix is yours to find.

### 5. Why an HFT codebase sweats these bytes

Two previews of where this module is heading:

- **Wire formats.** Exchange protocols (ITCH, OUCH, binary order entry)
  define messages as byte-exact layouts. The tempting move is to declare a
  struct and `memcpy` the packet into it — which only works if your
  struct's layout *provably* matches the wire: fixed-width member types,
  padding accounted for or crushed out, offsets pinned by `offsetof`
  checks. Teams commit those checks as `static_assert`s so a stray member
  edit fails the build, not the trading session. (This assignment keeps
  the size checks in runtime tests only because your stubs must compile —
  the promoted-to-`static_assert` version is the production idiom.)
- **Cache-line budgeting.** The unit of memory traffic is the 64-byte
  cache line. `OrderNaive` at 40 bytes straddles two lines for most array
  elements; the 24-byte reorder fits two-to-a-line-plus-change. When
  Module 6 measures hot loops over order books, the structs you shrink
  here are the difference between one cache miss and two — and `alignas`,
  false sharing, and per-line struct packing all build directly on today's
  layout algorithm.

## Assignment spec

Everything lives in `include/course/layout.hpp` (header-only). Three parts:

1. **The algorithm.** Implement `aligned_up(offset, alignment)` (round up
   to the next multiple; alignment is a power of two — naive modulo or the
   bit trick, your call) and `packed_struct_size<Ts...>()` (replay §1's
   algorithm over `sizeof(Ts)`/`alignof(Ts)` in order, tail padding
   included). Both `constexpr`. The tests compare your answers against
   real structs the compiler laid out.
2. **The reorder.** `OrderCompact` ships with the same member order as
   `OrderNaive` (40 bytes). Reorder its members — same six members, same
   names, nothing added or removed — to reach `sizeof == 24`. Leave
   `OrderNaive` untouched; it's the before-picture.
3. **The empty member.** Make `sizeof(TaggedValue<double, PriceTag>)`
   equal `sizeof(double)` without breaking `value()`/`set_value()`.

**Acceptance criteria:** all `m03a01.*` tests pass in `debug` and `asan`.
Then run the `-fdump-record-layouts` dump from THEORY §3 on your
`OrderCompact` and confirm you can narrate every offset in it from memory —
that narration is a stock interview exercise.

## Hints

<details><summary>Hint 1 — aligned_up without the bit trick</summary>
How far past the previous multiple of <code>alignment</code> is
<code>offset</code>? That's <code>offset % alignment</code>. If it's zero
you're done; otherwise add what's missing. Get this correct and boring
first — <code>packed_struct_size</code> is built on it. Then, if you like,
switch to <code>(offset + alignment - 1) &amp; ~(alignment - 1)</code> and
convince yourself it's the same function when alignment is a power of two.
</details>

<details><summary>Hint 2 — packed_struct_size shape</summary>
You need one running <code>offset</code> and one running
<code>max_align</code> across the pack. A C++17 fold over a comma
expression works:
<pre>std::size_t offset = 0, max_align = 1;
((offset = aligned_up(offset, alignof(Ts)) + sizeof(Ts),
  max_align = alignof(Ts) &gt; max_align ? alignof(Ts) : max_align), ...);
return aligned_up(offset, max_align);</pre>
If fold expressions feel like line noise (fair — Module 4 tames them),
expand the pack into two local arrays
<code>{sizeof(Ts)...}</code>/<code>{alignof(Ts)...}</code> and write a
plain loop; constexpr functions may loop.
</details>

<details><summary>Hint 3 — the reorder</summary>
Sort by alignment, descending: the 8s (<code>price</code>,
<code>id</code>), then the 4 (<code>qty</code>), the 2
(<code>venue</code>), the 1s (<code>side</code>, <code>flag</code>).
Compute the layout by hand — 8+8+4+2+1+1 = 24, already a multiple of 8, so
zero padding anywhere. Feed the same type sequence to your own
<code>packed_struct_size</code> and watch it agree.
</details>

<details><summary>Hint 4 — TaggedValue</summary>
THEORY §4, second bullet, one attribute on one member. Verify the flip
side too: give the tag a data member in a scratch file and confirm the
size grows again — <code>[[no_unique_address]]</code> is permission, not
magic.
</details>

## Further reading

- *The Lost Art of Structure Packing*, Eric S. Raymond —
  <http://www.catb.org/esr/structure-packing/> — the canonical essay; §1–§2
  of this handout in long form.
- cppreference:
  [no_unique_address](https://en.cppreference.com/w/cpp/language/attributes/no_unique_address)
  and [Empty Base Optimization](https://en.cppreference.com/w/cpp/language/ebo).
- CppCon 2019, Timur Doumler, *"Type Punning in Modern C++"* — what you may
  and may not do when mapping bytes to structs; the legal footing for §5's
  wire-format `memcpy`.
- CppCon 2014, Chandler Carruth, *"Efficiency with Algorithms, Performance
  with Data Structures"* — why layout, not cleverness, dominates real
  performance; the mindset for Module 6.
- *System V AMD64 ABI* §3.1 and *AAPCS64* §5 — the actual tables of
  fundamental-type sizes/alignments for your two platforms (skim, don't
  memorize).
- cppreference: [Standard-layout types](https://en.cppreference.com/w/cpp/language/data_members#Standard-layout)
  and [offsetof](https://en.cppreference.com/w/cpp/types/offsetof) — the
  fine print behind §2's "the compiler may not reorder".
