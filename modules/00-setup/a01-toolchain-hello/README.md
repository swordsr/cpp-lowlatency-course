# a01 — Toolchain Hello: Fixed-Point Prices

**Estimated effort:** 2–4 hours (most of it tooling, deliberately).

## Learning objectives

After this assignment you can:

- Run the full course loop: build a preset, run one assignment's tests, run
  its benchmark, run it under sanitizers.
- Explain the C++ compilation model (preprocessor → compiler → linker) and
  what a translation unit is.
- Explain why trading systems use fixed-point integer prices, and implement
  conversion to/from decimal strings.
- Pull up an assignment's function in Compiler Explorer and compare ARM64 vs
  x86-64 output.

## THEORY

### 1. The compilation model (10,000-ft view, on purpose)

Java gave you one compiler, one artifact format, and a classloader. C++ gives
you a pipeline you must understand because its seams are where errors appear:

```
price.cpp ──preprocess──▶ translation unit ──compile──▶ price.o ─┐
tests.cpp ──preprocess──▶ translation unit ──compile──▶ tests.o ─┼─link──▶ m00a01_tests
                                              libgtest.a ────────┘
```

- **Preprocessing** is *textual*: `#include "course/price.hpp"` literally
  pastes the header into the file. What comes out — one `.cpp` plus
  everything it transitively includes — is a **translation unit (TU)**.
- **Compilation** turns one TU into one object file, independently of every
  other TU. The compiler never sees your whole program. That's why functions
  used across TUs need declarations in headers: the compiler trusts the
  signature and leaves a hole.
- **Linking** merges object files, filling each hole with the one definition
  of that symbol. Two definitions → duplicate-symbol error; zero → undefined
  reference. This is the **One Definition Rule (ODR)** enforced mechanically.

Try it: `cmake --build --preset debug --verbose` shows the actual compiler
commands. You will never write these by hand — that's CMake's job — but you
must be able to read them.

### 2. Why fixed-point prices

`double` stores base-2 fractions. `155.10` is not representable in base 2;
you get `155.09999999999999...`. Three consequences kill floats for money:

1. **Equality is broken.** A limit order at 155.10 and an incoming quote at
   "155.10" may compare unequal after different arithmetic paths.
2. **Accumulation drifts.** Summing millions of fills accrues error.
3. **Non-determinism across builds.** Compilers may use different instruction
   sequences (e.g. fused multiply-add) for the same source, so two builds can
   disagree — fatal when a risk system re-checks an engine's numbers.

The industry answer: represent a price as an integer count of **ticks** at a
fixed scale. This course uses scale 10⁴, so `155.1` is the `int64_t`
`1'551'000`. All arithmetic becomes exact integer arithmetic; comparisons are
single instructions. Exchange protocols (e.g. Nasdaq ITCH — wait for Module
11) do exactly this on the wire.

The subtle part, and the actual work of this assignment, is the **string
boundary**: parsing `"155.1"` into ticks without ever going through floating
point, and formatting ticks back. Parsing digits by hand — `value = value*10
+ (c - '0')` — with explicit validation and overflow checks is a rite of
passage; you'll write faster versions of it in Module 11.

### 3. This course's harness

- **Tests are the spec.** Open `tests/price_test.cpp` *before* implementing.
  Each `TEST(Suite, Name)` documents one behavior; the assertion messages
  tell you what's expected. `ctest --preset debug -R m00a01` runs just these.
- **Benchmarks** live in `bench/` and use Google Benchmark. They only mean
  anything from the `release` preset. Each benchmark's handout section says
  what to measure and what "good" looks like.
- **Sanitizers** are compiler instrumentation that catch memory errors (ASan),
  undefined behavior (UBSan), and data races (TSan) *at runtime, in the act*.
  Coming from the JVM: they're your replacement for the safety the GC and
  verifier silently gave you. `cmake --preset asan` etc.

### 4. Compiler Explorer (godbolt.org)

Compiler Explorer compiles a snippet with any compiler and shows the
assembly, live. You're on ARM64 but interviews assume x86-64 — Compiler
Explorer is how this course keeps you literate in both. Do this now:

1. Paste your finished `parse_price` into godbolt.org.
2. Compiler "ARM64 clang 17", flags `-O2 -std=c++20`. Skim: can you find the
   `value*10` (look for `madd` or shift-add tricks)?
3. Add a second compiler pane: "x86-64 clang 17", same flags. Same structure,
   different mnemonics (`lea`/`imul`).
4. Change `-O2` to `-O0` and watch the code balloon. This is why we never
   benchmark debug builds.

> **Linux/x86 sidebar:** on Linux you'd get the same view locally with
> `objdump -d --demangle build/release/.../m00a01_bench | less` or
> `perf annotate`. On macOS: `otool -tvV <binary>`.

## Assignment spec

Implement the two functions declared in `include/course/price.hpp`, in
`src/price.cpp`, replacing the `// TODO` bodies:

```cpp
std::optional<std::int64_t> parse_price(std::string_view text);
std::string format_price(std::int64_t ticks);
```

- Scale is `kPriceScale = 10'000` (4 decimal places).
- `parse_price`: optional leading `-`; integer part required; optional `.`
  followed by 1–4 fraction digits; anything else — empty input, other
  characters, missing digits, >4 fraction digits, or overflow of the tick
  value — returns `std::nullopt`. **No floating point anywhere.**
- `format_price`: exact inverse for canonical strings — no trailing fraction
  zeros, no `.` for whole ticks values, `-0.0001` for negative sub-1 values.
- The test suite is the authoritative spec; where prose and tests disagree,
  tests win.

**Acceptance criteria:** all `m00a01.*` tests pass in `debug` **and** `asan`
presets.

**Benchmark ritual (not graded):** build the `release` preset, run
`m00a01_bench`, and record ns/op for the parse benchmarks in a comment at the
top of your `price.cpp`. There is no target to hit — the point is to learn
the ritual and have a baseline number you'll beat in Module 11. Expect
roughly 5–20 ns per parse on Apple Silicon at `-O2`; if you see hundreds,
you're probably benchmarking a debug build.

## Hints

<details><summary>Hint 1 — where to start</summary>
Run the tests first: <code>ctest --preset debug -R m00a01</code>. Pick the
simplest failing test (parsing <code>"0"</code>) and make only it pass.
Repeat. You never need to think about the whole spec at once.
</details>

<details><summary>Hint 2 — parsing structure</summary>
Walk the string once with an index: consume optional <code>-</code>, then
required digits (accumulate <code>int64</code>), then if a <code>.</code>
remains, consume 1–4 digits into a fraction value while tracking how many you
saw, then require end-of-string. Combine at the end:
<code>sign * (whole * 10'000 + frac * 10^(4 - frac_digits))</code>.
</details>

<details><summary>Hint 3 — overflow without UB</summary>
Signed overflow is undefined behavior, so you can't do the multiply and check
after. Check <em>before</em>: <code>if (whole > (max - digit) / 10)
return std::nullopt;</code>. UBSan (the <code>asan</code> preset) will catch
you if you get this wrong — one test feeds it a value that overflows mid-
accumulation on purpose.
</details>

## Further reading

- *A Tour of C++* (3rd ed.), ch. 1–2 — skim as a syntax reference for this
  module.
- CppCon 2017, Matt Godbolt, *"What Has My Compiler Done for Me Lately?"* —
  the Compiler Explorer author on reading assembly. Watch before Module 6 at
  the latest.
- [Compiler Explorer](https://godbolt.org) — bookmark it.
- David Goldberg, *"What Every Computer Scientist Should Know About
  Floating-Point Arithmetic"*, §1–2 — why 155.10 doesn't exist in binary.
