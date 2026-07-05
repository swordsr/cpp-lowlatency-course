# a02 — Virtual Dispatch: What Polymorphism Costs

**Estimated effort:** 5–7 hours.

## Learning objectives

- Explain the mechanism: vptr in the object, vtable per dynamic type, and
  the three-step dance a virtual call compiles to.
- Quantify the cost on modern hardware — and articulate why *lost inlining*
  matters more than the indirect branch itself.
- Internalize the virtual-destructor rule by debugging its absence.
- Judge where `virtual` belongs in a low-latency system (and where a03's
  CRTP takes over).

## THEORY

### 1. The mechanism: one pointer in the object, one table per type

Declare even one virtual function and the compiler adds a hidden pointer —
the **vptr** — at the front of every object. It points to the **vtable**:
a per-*dynamic-type*, per-program array of function pointers, one slot per
virtual function. A virtual call is then exactly three steps:

```cpp
struct FeeModel { virtual std::int64_t fee_ticks(std::int64_t) const; };

std::int64_t f(const FeeModel* m, std::int64_t n) {
    return m->fee_ticks(n);
    // 1. load m->vptr                  (one memory load)
    // 2. load vptr[slot_of_fee_ticks]  (another memory load)
    // 3. indirect call to that address
}
```

The vptr is real object state — you pay for it per *instance*:

```cpp
struct NonVirt { std::int64_t x; };                        // sizeof == 8
struct Virt    { std::int64_t x; virtual ~Virt() = default; };  // sizeof == 16
```

The `LayoutObservation` tests pin these numbers down; they pass from day
one because they document facts, not your code.

Coming from Java this is the inversion to absorb: in Java **every**
instance method is virtual unless `final`/`private`/`static`, every object
carries an object header with a class pointer anyway, and you rely on the
JIT to *devirtualize* — it profiles call sites, sees one receiver class,
and inlines speculatively (with a deopt guard). C++ has no runtime to save
you: you opt **in** to dispatch per function with `virtual`, non-virtual is
the default, and whatever you wrote is what ships. The cost model is
visible in the source — which is exactly why HFT shops care about the
keyword.

### 2. What it costs: the branch is cheap when boring, and inlining is the real casualty

Step 3 above is an **indirect branch**: the CPU can't know the target until
the vtable load resolves, so it *predicts* the target (branch target
buffer / indirect branch predictor) and speculates ahead.

- **Monomorphic call site** (one dynamic type ever seen): prediction is
  nearly perfect. Overhead: two dependent loads plus a correctly-predicted
  branch — a handful of cycles.
- **Megamorphic call site** (several types, unpredictable order): the
  predictor keeps guessing wrong, and every miss is a pipeline flush —
  ~15–20 cycles on Apple Firestorm-class and modern x86 cores. The bench
  makes this brutal and then shows the cure: *sort by type* and the same
  objects get cheap again. Dispatch cost lives in the call site's history,
  not in the keyword.

But the headline cost is neither load nor branch. A direct call to a small
function **inlines**: the call disappears, and the optimizer sees through
it — constant folding, vectorization, dead-code elimination across the
boundary. A virtual call is an **optimization barrier**: the compiler
usually can't prove the target, so nothing crosses it. `x * 2 + 1` behind a
virtual call is a full function call forever; inlined, it's one `madd`
folded into a vectorized loop. That's a 10× gap on trivial operations —
compare `BM_DirectCall` against everything else in the bench.

### 3. Virtual destructors: the rule you get to rediscover

```cpp
std::unique_ptr<FeeModel> p = std::make_unique<BasisPointsFee>(5);
p.reset();  // delete (FeeModel*)... — which destructor runs?
```

If `~FeeModel` is **non-virtual**, deleting through the base pointer is
**undefined behavior** ([expr.delete]/3). In practice the compiler emits a
direct call to `~FeeModel` — the derived destructor never runs, derived
members (like `TieredFee`'s `std::vector`) leak, and nothing warns you.
Destruction through a pointer is a dispatch decision like any other call,
and it must be made on the *dynamic* type.

Hence the rule: **a base class with any virtual function gets a virtual
destructor** (public virtual, or protected non-virtual if deletion through
base is meant to be impossible). The header we ship violates it, on
purpose; the `DeleteThroughBasePointer` test is failing right now because
of it. Note the fine print in that test: since the stub state is UB, asan
or ubsan may abort instead of "just" skipping the derived destructor —
that's the same bug, reported louder. Java has no such trap: finalization
aside, the GC and `Object` header always know the dynamic type.

### 4. `virtual` in low-latency systems: configuration-time yes, per-message no

The question is never "is virtual slow?" — it's "how many times does this
call site fire, and could it have inlined?"

- **Configuration-time polymorphism: yes.** Choosing a `FeeModel`, a
  logger, a transport at startup — dozens of calls, wildly different
  implementations, testability via mocking. A vtable is the right tool and
  the cost is unmeasurable.
- **Per-message hot path: usually no.** A virtual call per market-data tick
  (millions/sec) pays the barrier every time, and if the handler set is
  mixed, the megamorphic penalty on top. Hot paths want the compiler to see
  the concrete type: templates, `final` + concrete refs, or CRTP —
  compile-time polymorphism with zero dispatch, which is exactly a03.

Middle ground worth knowing: marking classes/functions `final` lets the
compiler devirtualize provably-final calls, and GCC/Clang can speculatively
devirtualize with PGO/LTO — but unlike the JVM, only at build time and only
with proof or profiles.

### 5. Sidebar — spotting the indirect call in Compiler Explorer

Paste the snippet from §1 into [godbolt.org](https://godbolt.org) with
ARM64 clang `-O2`. The three steps are right there:

```asm
ldr x8, [x0]        ; load vptr        (step 1)
ldr x8, [x8, #8]    ; load vtable slot (step 2)
br  x8              ; indirect jump    (step 3 — tail call; else `blr x8`)
```

`blr x8` (branch-and-link to register) is *the* signature of a virtual
call on ARM64 — target in a register, not in the instruction.

**Linux/x86-64 sidebar:** same shape, different spelling — `mov rax,
[rdi]` / `call qword ptr [rax + 8]` (or loaded to a register then
`call rax`). Any `call`/`jmp` whose operand is a register or memory
location is indirect. On your x86 target machines these are also the
Spectre-v2 mitigation points (retpolines turn them into something uglier)
— one more reason hot paths avoid them. A direct call, by contrast, is
`bl _some_symbol` / `call _some_symbol` — and after inlining, nothing
at all.

## Assignment spec

Skeleton in `include/course/fee_model.hpp`, stubs in `src/fee_model.cpp`.
The constructors are given (storing config is boilerplate, not the lesson).
You implement:

- `FlatFee::fee_ticks` — returns the configured flat fee, notional ignored.
- `BasisPointsFee::fee_ticks` — `notional * bps / 10'000`, integer
  (truncating) division.
- `TieredFee::fee_ticks` — the bps of the highest tier whose
  `threshold_notional <= notional`; exactly-at-threshold reaches that tier;
  below the first threshold, the first tier's bps still applies
  (`tiers[0].threshold_notional` is documented as ignored / assumed 0).
- `FeeModel::fee_for` — non-virtual convenience: notional = price × qty,
  then dispatch to `fee_ticks`. Note it must dispatch *virtually* even
  though it is itself non-virtual.
- `total_fees` — sum `fee_ticks` over a
  `std::vector<std::unique_ptr<FeeModel>>`.
- **The bug:** one keyword is missing in the base class. The
  `DeleteThroughBasePointer` test finds it; THEORY §3 explains it. Fix it
  and say the rule out loud.

**Acceptance criteria:** all `m03a02.*` tests pass in `debug` and `asan`
(asan is non-negotiable here — before the fix it may crash on the dtor
test, which is the point). No changes to the test file; `override` on every
overriding function in your headers.

**Benchmark:** build the release preset and run `m03a02_bench` (harness
code — complete, none of it yours). Record ns/op for all six rows and
check the shape against the comment block at the top of
`bench/dispatch_bench.cpp`: direct ≈ vanishes, monomorphic virtual adds a
small constant, megamorphic-shuffled is clearly worse (expect several×
monomorphic), megamorphic-*sorted* recovers to near-monomorphic, function
pointer ≈ monomorphic virtual, `std::function` worst. "Good" is being able
to explain *each gap* with §2's vocabulary: which rows lost inlining, which
rows are paying for branch mispredictions, and why sorted vs shuffled
differ when they execute identical instructions.

## Hints

<details><summary>Hint 1 — the destructor test fails and asan screams</summary>
Read THEORY §3 again, slowly. Then look at what <code>p.reset()</code> on a
<code>std::unique_ptr&lt;FeeModel&gt;</code> actually executes: a
<code>delete</code> on a <em>base</em> pointer. Which destructor gets
picked, and how would the compiler even know to pick a different one?
The fix is one keyword in one place in the header.
</details>

<details><summary>Hint 2 — tier selection semantics</summary>
Walk the tiers in order and remember the bps of every tier whose
threshold is <code>&lt;=</code> notional; the last one you saw wins. Seed
with <code>tiers_[0].bps</code> so notionals below the first threshold are
covered without a special case. (An <code>std::upper_bound</code> on
threshold, minus one, also works — the vector is sorted — but a linear scan
over a handful of tiers is honest and likely faster anyway.)
</details>

<details><summary>Hint 3 — the one-liners</summary>
<pre>std::int64_t FeeModel::fee_for(std::int64_t price_ticks, std::int64_t qty) const {
    return fee_ticks(price_ticks * qty);   // non-virtual → virtual: still dispatches
}</pre>
and <code>total_fees</code> is a range-for accumulating
<code>m-&gt;fee_ticks(notional_ticks)</code> — or a one-line
<code>std::accumulate</code> if you're feeling fancy.
</details>

## Further reading

- *Effective C++* Item 7 — declare destructors virtual in polymorphic base
  classes.
- [CppCoreGuidelines C.35](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#c35-a-base-class-destructor-should-be-either-public-and-virtual-or-protected-and-non-virtual)
  and [C.128](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#c128-virtual-functions-should-specify-exactly-one-of-virtual-override-or-final)
  (`virtual`/`override`/`final` discipline).
- [Itanium C++ ABI — vtable layout](https://itanium-cxx-abi.github.io/cxx-abi/abi.html#vtable)
  — what your compiler actually emits (yes, ARM64 macOS uses this ABI too).
- CppCon 2019, Chandler Carruth, *"There Are No Zero-cost Abstractions"* —
  the dispatch section this time.
- Agner Fog, *The microarchitecture of Intel, AMD and VIA CPUs* — chapters
  on branch prediction, for the x86 side of §2.
