# Module 0 — Setup & Tooling

**Time budget:** ~half a week. One assignment.

This module exists to make sure your toolchain works and that you understand
the loop you'll repeat for the next six months: read the handout → run the
red tests → implement until green → run the benchmark → look at the assembly.

## Before you start

Work through the "One-time setup" and "Daily loop" sections of the
[root README](../../README.md). You should be able to run:

```sh
cmake --preset debug && cmake --build --preset debug && ctest --preset debug
```

## Assignments

| # | Assignment | What you'll touch |
|---|------------|-------------------|
| 1 | [a01-toolchain-hello](a01-toolchain-hello/) | the build/test/bench/sanitize loop, fixed-point prices, first look at assembly |

## Interview questions

Warm-ups on the compilation model — the answers are in the a01 handout and
its further reading. Try answering out loud before expanding.

<details>
<summary>Walk me through what happens between <code>clang++ main.cpp</code> and an executable.</summary>

Preprocessing (textual inclusion of headers, macro expansion) produces one
*translation unit* per `.cpp`; the compiler turns each TU into an object file
(machine code with unresolved symbol references); the linker merges object
files and libraries, resolving symbols into a final executable. Errors at
each stage look different: missing header (preprocess), type error (compile),
undefined/duplicate symbol (link).
</details>

<details>
<summary>Why does C++ split code into headers and source files, and what problems does that create?</summary>

Each TU compiles independently, so declarations must be repeated wherever
used — headers are the sharing mechanism. Consequences: the One Definition
Rule, include guards, long build times from textual inclusion, and the
declaration/definition split. (C++20 modules address this, but adoption in
trading firms is still patchy.)
</details>

<details>
<summary>Why do trading systems represent prices as integers rather than <code>double</code>?</summary>

Binary floating point can't represent most decimal fractions exactly
(`0.1 + 0.2 != 0.3`), equality comparisons become unreliable, and rounding
drifts under accumulation — unacceptable when the value is money and when two
independent systems must agree on it exactly. Fixed-point integers (price ×
scale) give exact decimal arithmetic, exact equality, deterministic behavior,
and faster comparisons.
</details>

<details>
<summary>What does <code>-O2</code> actually do? Name a few specific optimizations.</summary>

Inlining, constant folding/propagation, dead-code elimination, loop
unrolling/vectorization, register allocation improvements, branch layout.
Key interview follow-up: optimized code can look nothing like the source —
which is why we benchmark and read assembly rather than reason from source.
</details>
