# Module 3 — The Object Model

**Time budget:** ~2 weeks. Three assignments.

Java hid the object model from you: every object on the heap, every method
virtual, a header word you never saw. C++ shows you everything — an object
is exactly the bytes you can account for, and every indirection is one you
asked for. This module is about being able to account for them:

1. **a01** — what the bytes are: layout, alignment, padding, empty classes;
2. **a02** — what `virtual` adds: vptr/vtable mechanics and the real cost
   of dynamic dispatch (measured, not folklore);
3. **a03** — the alternatives: CRTP static polymorphism and hand-rolled
   type erasure (`FunctionRef`), the two tools low-latency code reaches for
   when `virtual` is too much or too rigid.

Interviewers at trading firms love this module: "what does this struct's
layout look like?", "what happens, instruction by instruction, on a virtual
call?", "when would you not use virtual functions?" all come straight from
here.

## Assignments

| # | Assignment | What you'll touch |
|---|------------|-------------------|
| 1 | [a01-layout-and-padding](a01-layout-and-padding/) | sizeof/alignof/offsetof, padding rules, reordering, EBO |
| 2 | [a02-virtual-dispatch](a02-virtual-dispatch/) | vtables, virtual destructors, dispatch-cost benchmarks |
| 3 | [a03-crtp-and-type-erasure](a03-crtp-and-type-erasure/) | CRTP mixins, FunctionRef, choosing a dispatch mechanism |

## Interview questions

<details>
<summary>"Why is <code>sizeof(struct { char a; int b; char c; })</code> twelve and not six?"</summary>

Each member is placed at the next multiple of its alignment: `a` at 0, `b`
at 4 (3 padding bytes), `c` at 8; then the struct is padded to a multiple
of its strictest member alignment (4) so arrays of it stay aligned → 12.
Reordering to `int, char, char` gives 8. The compiler may not reorder for
you; layout is part of the ABI.
</details>

<details>
<summary>"Walk me through what the CPU does on <code>base->f()</code> when f is virtual."</summary>

Load the vptr from the object (one dependent load), load the function
address from the vtable slot (second dependent load), indirect call. Plus
the invisible costs: the callee can't be inlined (no constant propagation,
no dead-code elimination across the call), and a megamorphic call site
mispredicts its branch target. Monomorphic sites predict essentially
perfectly — which is why "virtuals are slow" without qualification is
wrong, and measured answers win interviews.
</details>

<details>
<summary>"When must a destructor be virtual, and what happens if it isn't?"</summary>

Whenever deletion may happen through a base-class pointer. Without it,
`delete base_ptr` is undefined behavior — in practice the derived
destructor (and its members' destructors) silently never run. Guideline:
a base class should have a destructor that is either public and virtual,
or protected and non-virtual.
</details>

<details>
<summary>"What is CRTP and why would an HFT codebase use it instead of virtual functions?"</summary>

`template <class D> struct Base` where the derived class inherits
`Base<Derived>`: the base can `static_cast<D&>(*this)` because the cast is
resolved when member functions are instantiated. Dispatch is static — the
compiler sees the concrete type, so calls inline and vanish. Used when the
set of types is closed at compile time and the call is on the per-message
hot path. The cost: no runtime substitution, one instantiation per derived
type (code size), and templates in headers.
</details>

<details>
<summary>"What's inside a <code>std::function</code>, and why might you avoid it on a hot path?"</summary>

Type erasure: a manually built vtable (invoke/copy/destroy operations) plus
storage for the callable — inline small-buffer storage if it fits,
otherwise a heap allocation. Costs: possible allocation on construction,
indirect call on invoke, and it *owns* a copy of the callable. A non-owning
`FunctionRef` (two words: object pointer + trampoline pointer) is the hot
path alternative when the callable outlives the call — same trick
`string_view` plays on `string`.
</details>

<details>
<summary>"Same interface, four dispatch options — direct, virtual, CRTP, std::function. How do you choose?"</summary>

Known single type → direct/CRTP (inlines). Closed type set, hot path →
CRTP or `variant`+`visit`. Open type set, substitution at runtime, cold or
warm path → virtual. Need to store arbitrary callables with ownership →
`std::function`; without ownership → `FunctionRef`. The follow-up they
want: "and I'd benchmark the two candidates on the real call pattern" —
which is exactly what a02/a03's benchmarks do.
</details>
