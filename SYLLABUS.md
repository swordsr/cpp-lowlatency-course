# Systems & Low-Latency C++ — Course Syllabus

A self-study course taking an experienced backend engineer (Java/Scala/Python)
to interview-ready competence in systems and low-latency C++, aimed at
high-frequency-trading and performance-engineering roles.

This document is the **course design**. The root [README](README.md) covers
day-to-day mechanics (building, running tests, tracking progress).

---

## 1. Goals & outcomes

By the end of the course you can:

- Write idiomatic modern C++ (C++20) with correct ownership and lifetime
  management — and explain *why* it's correct.
- Explain the C++ object model (layout, vtables, dispatch cost) and choose
  between runtime and compile-time polymorphism deliberately.
- Use templates, concepts, and `constexpr` to move work to compile time.
- Reason about caches, branches, allocation, and memory layout; measure with a
  benchmark harness instead of guessing; read compiler output.
- Write correct concurrent code with atomics and explicit memory ordering, and
  build the classic lock-free structures (SPSC/MPMC queues, seqlock).
- Build the two canonical HFT systems — a limit order book / matching engine
  and a market-data feed handler — and wire them into a measured end-to-end
  pipeline.
- Answer the interview questions that come with all of the above.

**Non-goals:** GUI/desktop C++, C++98 archaeology, build-system mastery beyond
what the course needs, options math / trading strategy content.

## 2. Prerequisites & environment

- Strong general programming background; CS fundamentals assumed. No prior
  C++ required — Module 1 ramps syntax quickly rather than gently.
- **Machine:** Apple Silicon Mac (course is developed and CI-tested on ARM64
  macOS). Everything builds natively — no VM or container required.
- **Linux/x86 policy:** HFT production is Linux/x86, so wherever macOS/ARM64
  differs, the handout teaches both: you *run* the macOS-native tool or API
  (Instruments/`sample`, kqueue, `clock_gettime`, ARM64 memory model) and the
  THEORY section covers the Linux/x86 equivalent (`perf`, epoll, `rdtsc`, x86
  TSO) with the exact commands/idioms you'd use there. Compiler Explorer is
  used to read **both ARM64 and x86-64** assembly.
- **Toolchain:** AppleClang (Xcode Command Line Tools), CMake ≥ 3.24, Ninja,
  C++20. GoogleTest and Google Benchmark are fetched automatically by CMake
  (FetchContent, pinned versions). Sanitizers (ASan/UBSan/TSan) are wired in
  as first-class build presets.

## 3. How the course works

### Assignments

Each module directory `modules/NN-name/` has a module `README.md` (overview,
sequencing, **interview question bank** with answers in collapsed sections)
and one folder per assignment. Every assignment ships:

- **`README.md` — the handout.** Sections, in order:
  1. *Learning objectives* — what you'll be able to do afterwards.
  2. *THEORY* — actually teaches the concepts (with code and diagrams), not
     just names them. Includes Linux/x86 sidebars where relevant.
  3. *Assignment spec* — what to build, with acceptance criteria ("all tests
     in `tests/` pass") and explicit performance/complexity targets where
     performance is the point.
  4. *Hints* — an escalating ladder inside `<details>` spoiler blocks.
  5. *Further reading* — specific chapters, papers, and talks.
- **Interface stubs** in `include/` and `src/` with `// TODO` bodies that
  compile as-is (they return dummy values, so the suite builds and fails
  honestly rather than not compiling).
- **A test suite that defines the spec.** GoogleTest, thorough, mostly red
  until you implement things. Tests are the acceptance criteria; the handout
  never contradicts them.
- **A benchmark harness** (Google Benchmark) wherever performance is the
  point, plus a handout note on what to measure and what "good" looks like on
  Apple Silicon (with a sidebar on how numbers shift on x86 servers).

### Rules of engagement (coaching protocol)

For any AI assistant (or human tutor) working in this repo:

1. **Never write or commit implementation code** for assignment stubs. The
   student writes every implementation.
2. When the student is stuck: point at the failing test, ask a guiding
   question, or give the next hint from the ladder — in that order.
3. Full solution code only if the student **explicitly** asks for it, and it
   is never committed to this repo.
4. Scaffolding (build files, new assignment stubs, tests, benchmarks, handout
   text) is the assistant's job; extending or fixing scaffolding on request is
   fine.

### Verification & CI

- `ctest` per assignment is the ground truth (see README for commands).
- GitHub Actions builds every push on an **Apple Silicon macOS runner**: the
  *build* must always succeed (stubs included); tests run non-blocking and
  produce a per-assignment pass/fail summary that doubles as a progress view.

## 4. Curriculum

~6 months at ~10 hrs/week. 12 modules, 36 assignments, foundations-first:
every capstone ingredient (arena allocator, SPSC queue, event loop, binary
parser) is built in an earlier module.

### Module 0 — Setup & Tooling (½ week, 1 assignment)
Toolchain check-out; how this course's build/test/bench loop works.
- **a01-toolchain-hello** — implement a tiny fixed-point price parser to turn
  the test suite green; run tests, benchmarks, and the ASan preset; read your
  first assembly on Compiler Explorer.

### Module 1 — C++ Crash Course for Polyglots (1½ weeks, 3 assignments)
Syntax fast; semantics slow. Value semantics vs JVM reference semantics,
const-correctness, references/pointers, headers & translation units.
- **a01-value-semantics** — running-statistics types; copies, references,
  `const`, and where objects actually live.
- **a02-strings-vectors-algorithms** — parse and analyze a trades CSV with
  `std::string_view`, `std::vector`, iterators, and `<algorithm>`.
- **a03-raii-first-contact** — a file handle wrapper and a scope timer;
  destructors as the resource-management primitive; error-handling idioms
  (exceptions, error codes, `std::optional`).

### Module 2 — Memory & Object Lifetime (2½ weeks, 4 assignments)
The heart of "what's new coming from a GC language."
- **a01-raw-memory-lab** — `new`/`delete`, pointer arithmetic, a hand-rolled
  buffer; deliberately cause and then fix leaks/use-after-free under ASan.
- **a02-rule-of-five** — a RAII dynamic array with correct copy/move
  constructors and assignment; observe copy elision.
- **a03-unique-ptr** — implement `UniquePtr` (incl. custom deleters); the
  test suite defines the exact ownership semantics.
- **a04-vector** — implement a `Vector` with placement `new`, uninitialized
  storage, growth policy, and basic exception safety; benchmark growth
  strategies against `std::vector`.

### Module 3 — The Object Model (2 weeks, 3 assignments)
- **a01-layout-and-padding** — `sizeof`/`alignof`/`offsetof`, padding, empty
  base optimization; specs enforced by `static_assert` tests.
- **a02-virtual-dispatch** — vtables and object layout; benchmark virtual vs
  direct vs function-pointer vs `std::function` dispatch.
- **a03-crtp-and-type-erasure** — static polymorphism with CRTP; build a
  small `FunctionRef`; benchmark against virtual dispatch.

### Module 4 — Templates & Compile-Time (2 weeks, 3 assignments)
- **a01-generic-fixed-point** — class/function templates and non-type
  template parameters: a `Price<Scale>` fixed-point type.
- **a02-concepts-and-traits** — type traits, `requires`, concepts; constrain
  book/order abstractions; tests are compile-time (`static_assert`) heavy.
- **a03-constexpr-and-variadics** — compile-time lookup tables and parsing;
  variadic templates for a small message dispatcher.

### Module 5 — Allocators & Data-Oriented Design (2 weeks, 3 assignments)
- **a01-arena-allocator** — bump allocator + standard allocator adapter;
  benchmark vs `new`/`delete`.
- **a02-object-pool** — fixed-size pool with an intrusive free list, O(1)
  alloc/free; reuse target: order objects.
- **a03-small-vector-and-soa** — `SmallVector` with inline storage; AoS vs
  SoA layout benchmarks on order records.

### Module 6 — Hardware & Benchmarking (2 weeks, 3 assignments)
- **a01-benchmark-methodology** — Google Benchmark in anger: dead-code
  elimination, warm-up, variance, `DoNotOptimize`; build the latency-histogram
  helper reused by the capstones.
- **a02-cache-lab** — sequential/strided/random access, working-set sweeps
  that reveal your cache hierarchy; prefetching.
- **a03-branch-lab** — branchy vs branchless, `[[likely]]`, sorted-input
  effects; verify explanations by reading the assembly.

### Module 7 — Concurrency I: Threads, Atomics, Ordering (2 weeks, 3 assignments)
- **a01-threads-and-locks** — `std::jthread`, mutexes, condition variables; a
  blocking work queue.
- **a02-memory-ordering** — atomics and `memory_order`; litmus tests that
  *actually fail* on ARM64's weak memory model; acquire/release message
  passing. (x86 TSO sidebar: why these bugs hide on Intel.)
- **a03-spinlocks-and-false-sharing** — TAS/TTAS/backoff spinlocks;
  demonstrate and fix false sharing with
  `hardware_destructive_interference_size`.

### Module 8 — Concurrency II: Lock-Free (2 weeks, 3 assignments)
- **a01-spsc-ring-buffer** — the classic single-producer/single-consumer
  queue with cache-padded indices; throughput and latency benchmarks; TSan.
- **a02-mpmc-queue** — a bounded Vyukov-style MPMC queue; the ABA problem.
- **a03-seqlock** — reader-writer snapshots for market data; when lock-free
  reads beat locks, and their hazards.

### Module 9 — Networking & Binary I/O (2 weeks, 3 assignments)
- **a01-sockets** — BSD sockets: TCP echo pair and UDP datagrams; byte order,
  `MSG_*` flags, socket options.
- **a02-kqueue-event-loop** — non-blocking I/O with a kqueue readiness loop
  and timers (epoll taught side-by-side in theory).
- **a03-binary-protocol-parsing** — zero-copy, alignment-safe decoding of a
  framed binary stream replayed from capture files.

### Module 10 — Capstone I: Limit Order Book & Matching Engine (3 weeks, 3 assignments)
- **a01-order-book-core** — price-level book with add/cancel/modify and O(1)
  best-bid/ask; correctness-heavy test suite.
- **a02-matching-engine** — price-time priority matching and trade
  generation; scenario- and invariant-based tests.
- **a03-performance-pass** — swap in your pool/arena/intrusive structures;
  hit throughput and p99-latency targets from the handout; profile-driven.

### Module 11 — Capstone II: Feed Handler & Integrated Pipeline (3½ weeks, 4 assignments)
- **a01-itch-parser** — decode an ITCH-style market-data protocol from replay
  files; sequence-gap detection.
- **a02-book-builder** — reconstruct books from the feed; snapshot +
  incremental recovery.
- **a03-live-pipeline** — provided UDP multicast replayer → your feed handler
  → book → strategy stub → order gateway, connected by your SPSC queues.
- **a04-tick-to-trade** — end-to-end latency measurement with your histogram
  tooling; tuning pass; a short write-up template (portfolio piece).

## 5. Pacing

| Weeks | Modules |
|-------|---------|
| 1–2   | 0 + 1 |
| 3–5   | 2 |
| 6–9   | 3 + 4 |
| 10–13 | 5 + 6 |
| 14–17 | 7 + 8 |
| 18–19 | 9 |
| 20–26 | 10 + 11 |

Falling behind is normal; the modules are ordered so that even stopping after
Module 8 leaves you interview-competent on the core topics.

## 6. Reading list (course-wide)

Per-assignment handouts cite specific chapters/talks; the backbone:

- *A Tour of C++* (Stroustrup) — fast idiomatic overview, Modules 1–2.
- *Effective Modern C++* (Meyers) — Modules 2–4.
- *C++ Concurrency in Action, 2nd ed.* (Williams) — Modules 7–8.
- *What Every Programmer Should Know About Memory* (Drepper) — Modules 5–6.
- Agner Fog's optimization manuals — Module 6+.
- CppCon: Carl Cook *"When a Microsecond Is an Eternity"*, Matt Godbolt
  *"What Has My Compiler Done for Me Lately?"*, Timur Doumler *"C++ in Low
  Latency"* — Modules 6, 10–11.
- cppreference.com as the daily reference.
