# Systems & Low-Latency C++ — Self-Study Course

A hands-on C++ course targeting systems / low-latency / HFT-style work:
college-style assignments with real handouts, test suites that define the
spec, and benchmark harnesses where performance is the point.

**Read [SYLLABUS.md](SYLLABUS.md) first** — it's the course design: module
list, conventions, pacing, and the coaching protocol.

## One-time setup (macOS, Apple Silicon)

```sh
xcode-select --install          # AppleClang toolchain
brew install cmake ninja        # build system (CMake ≥ 3.25)
git clone <this repo> && cd cpp-lowlatency-course
```

## Daily loop

```sh
cmake --preset debug                 # configure (first run fetches GoogleTest/Benchmark)
cmake --build --preset debug         # build everything scaffolded so far
ctest --preset debug -R m02a03       # run one assignment's tests (module 2, assignment 3)
ctest --preset debug                 # run everything
```

Each assignment `modules/NN-*/aNN-*/` builds a library target named like
`m02a03`, a test binary `m02a03_tests`, and (where performance matters) a
benchmark binary `m02a03_bench`. Tests start mostly red — implementing the
`// TODO` stubs until they're green **is** the assignment.

Run a benchmark (always from the `release` preset):

```sh
cmake --preset release && cmake --build --preset release --target m06a02_bench
./build/release/modules/06-hardware-and-benchmarking/a02-cache-lab/m06a02_bench
```

### Sanitizer presets — use them

| Preset | What it catches | Use during |
|--------|-----------------|------------|
| `asan` | leaks, use-after-free, buffer overflows, UB | Modules 2–5 especially |
| `tsan` | data races | Modules 7–8 especially |
| `release` | nothing — fast; benchmarks only | benchmark runs |

Same commands, different preset name: `cmake --preset asan && cmake --build
--preset asan && ctest --preset asan -R m02a01`.

## CI

Every push builds on an Apple Silicon macOS GitHub runner. The **build** must
stay green (stubs compile by design). Tests run non-blocking and the workflow
summary shows per-assignment pass counts — your progress at a glance.

## Progress

- **Module 0 — Setup & Tooling**
  - [ ] a01-toolchain-hello
- **Module 1 — C++ Crash Course for Polyglots**
  - [ ] a01-value-semantics
  - [ ] a02-strings-vectors-algorithms
  - [ ] a03-raii-first-contact
- **Module 2 — Memory & Object Lifetime**
  - [ ] a01-raw-memory-lab
  - [ ] a02-rule-of-five
  - [ ] a03-unique-ptr
  - [ ] a04-vector
- **Module 3 — The Object Model**
  - [ ] a01-layout-and-padding
  - [ ] a02-virtual-dispatch
  - [ ] a03-crtp-and-type-erasure
- **Module 4 — Templates & Compile-Time**
  - [ ] a01-generic-fixed-point
  - [ ] a02-concepts-and-traits
  - [ ] a03-constexpr-and-variadics
- **Module 5 — Allocators & Data-Oriented Design**
  - [ ] a01-arena-allocator
  - [ ] a02-object-pool
  - [ ] a03-small-vector-and-soa
- **Module 6 — Hardware & Benchmarking**
  - [ ] a01-benchmark-methodology
  - [ ] a02-cache-lab
  - [ ] a03-branch-lab
- **Module 7 — Concurrency I**
  - [ ] a01-threads-and-locks
  - [ ] a02-memory-ordering
  - [ ] a03-spinlocks-and-false-sharing
- **Module 8 — Concurrency II: Lock-Free**
  - [ ] a01-spsc-ring-buffer
  - [ ] a02-mpmc-queue
  - [ ] a03-seqlock
- **Module 9 — Networking & Binary I/O**
  - [ ] a01-sockets
  - [ ] a02-kqueue-event-loop
  - [ ] a03-binary-protocol-parsing
- **Module 10 — Capstone I: Order Book & Matching Engine**
  - [ ] a01-order-book-core
  - [ ] a02-matching-engine
  - [ ] a03-performance-pass
- **Module 11 — Capstone II: Feed Handler & Pipeline**
  - [ ] a01-itch-parser
  - [ ] a02-book-builder
  - [ ] a03-live-pipeline
  - [ ] a04-tick-to-trade

## Getting help

If you're stuck, work the handout's hint ladder first, then ask an assistant
— but note the coaching protocol in [SYLLABUS.md](SYLLABUS.md#rules-of-engagement-coaching-protocol):
assistants point at failing tests and give hints; they don't write your
implementations.
