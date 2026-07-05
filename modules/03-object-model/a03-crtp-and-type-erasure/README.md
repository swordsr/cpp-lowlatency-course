# a03 — CRTP & Type Erasure: Polymorphism Without the Vtable

**Estimated effort:** 6–8 hours.

## Learning objectives

- Implement the Curiously Recurring Template Pattern and explain why its
  "impossible" downcast is legal and free.
- Synthesize an interface from primitives (`>`, `<=`, `>=` from `==` and
  `<`) — and explain why C++20 makes `!=` synthesize *itself*.
- Build type erasure by hand: a non-owning `FunctionRef` that dispatches
  across unrelated callable types with no inheritance and no allocation.
- Choose between direct, virtual, CRTP, and erased dispatch on purpose,
  with benchmark numbers to back the choice.

## THEORY

### 1. CRTP: the base class that knows its derived class

a02 ended with the observation that virtual dispatch is a *runtime* answer
to a question that is often already settled at *compile time*. CRTP is the
compile-time answer:

```cpp
template <typename Derived>
struct Counter {
    void bump() { static_cast<Derived&>(*this).on_bump(); }  // no vtable
};

struct OrderCounter : Counter<OrderCounter> {
    void on_bump() { ++n; }
    int n = 0;
};
```

The base is a template *parameterized on its own derived class* — each
derived class gets its own base type, and the base can `static_cast` itself
down because, by construction, the only thing that ever inherits
`Counter<OrderCounter>` is `OrderCounter`. The call resolves statically:
the compiler sees the concrete type, inlines `on_bump`, and the
"polymorphic" call vanishes entirely. That's why per-message hot paths in
trading systems use CRTP where an OO codebase would use an interface.

The part that looks illegal: inside `Counter<Derived>`, `Derived` is an
*incomplete type* — the compiler is still in the middle of defining it when
it instantiates the base. The trick is *when* things are instantiated:
member function **bodies** of a class template are only instantiated when
called, long after `Derived` is complete. Signatures and member variables
don't get that grace period — `Derived d_;` as a member of the base would
genuinely be an error. This instantiation-timing rule is the load-bearing
wall of the whole pattern (and a favorite "why does this compile?"
interview question).

The costs, so you can argue both sides: the set of types is closed at
compile time (no plugging in a new strategy at runtime), each derived class
stamps out its own copy of the base's code (binary size), you can't put
`Counter<A>` and `Counter<B>` in one container (they're unrelated types),
and everything lives in headers.

### 2. Operator synthesis: the classic CRTP mixin

Java's `Comparable<T>` gives you one method and the library sorts with it.
C++ expects six comparison operators — all derivable from two primitives:
`>` is `<` with operands swapped, `<=` is `!(>)`, `>=` is `!(<)`. A CRTP
mixin writes them once, for every type that provides the two:

```cpp
struct PriceLevel : Comparisons<PriceLevel> {
    bool operator==(const PriceLevel&) const;  // you write these two
    bool operator<(const PriceLevel&) const;
    // >, <=, >= arrive from the mixin; != arrives from the LANGUAGE
};
```

What about `!=`? Since C++20 the compiler *rewrites* `a != b` as
`!(a == b)` whenever an `operator==` is visible — and if the mixin defined
its own `!=`, the rewritten candidate would beat it in overload resolution
anyway (its object parameter is an exact match; the inherited member needs
a derived-to-base binding). So the mixin deliberately omits it. The tests
pin this as an observation test, and it's a killer interview aside: you
know not just the pattern but where the language dissolved it.

> **C++20 sidebar:** the spaceship operator (`auto operator<=>(...) =
> default;`) made this *specific* mixin largely historical — the compiler
> can now synthesize all six itself. It stays in the course because (a) the
> pattern — synthesize interface from primitives via CRTP — generalizes far
> beyond comparisons (Boost.Operators, iterator facades, your Module 10
> book types), and (b) every pre-2020 codebase you'll ever be dropped into
> is full of it.

### 3. Type erasure: runtime polymorphism without inheritance

Virtual dispatch requires every participating type to inherit the
interface. But a lambda, a function pointer, and a hand-written functor
share no base class — yet `std::function<int(int)>` stores any of them.
The trick is a hand-rolled, single-entry vtable:

```cpp
void* obj_;                       // the callable, address only, type erased
R (*trampoline_)(void*, Args...); // knows how to un-erase and invoke it
```

At construction — the only moment the concrete type `F` is visible — you
capture the knowledge in a function *generated per type*: cast `void*` back
to `F*`, invoke. A stateless lambda `[](void* o, Args... a) -> R { ... }`
decays to exactly the function pointer you need. Calling is then two loads
and an indirect call: the same cost profile as a virtual call, but across
*unrelated* types.

`std::function` does this plus **ownership**: it copies the callable into
itself (small-buffer storage if it fits, otherwise the heap) and erases
copy/destroy operations too. That allocation is why it's banned from hot
paths. Your `FunctionRef` is the non-owning half: two words, trivially
copyable, never allocates — and, like `string_view` (Module 1), it carries
a lifetime contract instead: **the callable must outlive every call**.
Returning a `FunctionRef` bound to a local lambda is the same dangling bug
as returning a `string_view` to a local string. Perfect callback parameter;
dangerous stored member.

### 4. Choosing a dispatch mechanism

| Situation | Reach for |
|-----------|-----------|
| one known type | direct call (inlines; the compiler's problem now) |
| closed type set, hot path | CRTP (or `std::variant` + `visit` — Module 4 bank) |
| open type set, runtime substitution, warm/cold path | `virtual` |
| store arbitrary callables, own them | `std::function` (accept the allocation) |
| pass arbitrary callables downward, no ownership | `FunctionRef` |

The interview version of this table is THEORY §2 of the module README; the
benchmark below is your evidence.

## Assignment spec

Two headers, both header-only:

- **`include/course/comparisons.hpp`** — implement the three `// TODO`
  operators of the `Comparisons<Derived>` mixin in terms of *only*
  `Derived::operator==` and `Derived::operator<`. `PriceLevel` (given) is
  the fixture; the tests also instantiate the mixin on a second type to
  prove it's generic.
- **`include/course/function_ref.hpp`** — implement the binding
  constructor and `operator()` of `FunctionRef<R(Args...)>`. Must work
  with capturing lambdas, function pointers, and stateful functors *by
  reference* (the functor's state changes must be visible to the caller
  afterwards — that's the non-owning contract). Layout stays two words;
  the `static_assert`s on triviality must keep passing.

**Acceptance criteria:** all `m03a03.*` tests pass in `debug` and `asan`.

**Benchmark:** `m03a03_bench` (release preset). Compare: direct lambda
(the floor — expect the loop to vectorize), `FunctionRef` vs raw function
pointer vs monomorphic virtual (should be within noise of each other,
~1–2 ns/call on Apple Silicon), `std::function` call cost, and — the
headline — the **bind** benchmarks: constructing a `std::function` from a
4-word capture heap-allocates every time; binding a `FunctionRef` is two
stores. Record the bind-cost ratio in a comment at the top of
`function_ref.hpp`. Ignore the `FunctionRef` numbers until your tests are
green (the stub returns instantly and measures nothing).

## Hints

<details><summary>Hint 1 — the mixin's left-hand side</summary>
Inside <code>Comparisons&lt;Derived&gt;::operator&gt;</code>, who is
<code>a</code> in <code>a &gt; b</code>? It's the Derived object this base
lives inside: <code>const Derived& self = static_cast&lt;const
Derived&&gt;(*this);</code>. Then express each operator using only
<code>self == rhs</code> and the two orderings of <code>&lt;</code>.
</details>

<details><summary>Hint 2 — the three derivations</summary>
<code>a &gt; b ⇔ b &lt; a</code>; <code>a &lt;= b ⇔ !(b &lt; a)</code>;
<code>a &gt;= b ⇔ !(a &lt; b)</code>. If <code>GreaterOrEqual</code> fails
on the equal-objects case, you wrote it in terms of <code>&gt;</code> and
<code>==</code> instead of one clean negation.
</details>

<details><summary>Hint 3 — FunctionRef: what goes in obj_</summary>
<code>std::addressof(f)</code>, cast to <code>void*</code>. You are
storing the address of the caller's callable — const_cast is fine here
(<code>(void*)&f</code> via <code>const_cast</code>/<code>reinterpret_cast</code>
of the remove-reference type) because the trampoline casts back to the
exact original type before use.
</details>

<details><summary>Hint 4 — the trampoline's type problem</summary>
The trampoline must be a plain function pointer — no captures allowed. So
where does it learn F? From the CONSTRUCTOR's template parameter: generate
one trampoline per F at bind time. A capture-less lambda defined inside
the constructor, taking <code>(void*, Args...)</code>, decays to the
function pointer you need.
</details>

<details><summary>Hint 5 — the whole constructor, in words</summary>
Line 1: <code>obj_</code> = address of <code>f</code> as
<code>void*</code>. Line 2: <code>trampoline_ = [](void* obj, Args... args)
-&gt; R { return (*static_cast&lt;std::remove_reference_t&lt;F&gt;*&gt;(obj))(
std::forward&lt;Args&gt;(args)...); };</code>. Then
<code>operator()</code> is one line: call <code>trampoline_(obj_,
std::forward&lt;Args&gt;(args)...)</code>. If the mutable-functor test
fails, you copied somewhere instead of taking the address.
</details>

<details><summary>Hint 6 — the function-pointer test binds to a VARIABLE. Why?</summary>
Casting a function pointer to <code>void*</code> is only conditionally
supported in standard C++ (code and data live in different address spaces
on some machines; POSIX guarantees it, the standard doesn't). Binding to
the pointer <em>variable</em> sidesteps the issue portably:
<code>obj_</code> points at the variable (a data object), and the
trampoline casts back to <code>int(**)(int)</code> and calls through two
levels. Your generic constructor already does this correctly if it takes
the address of whatever it was given — no special case needed.
</details>

## Further reading

- *Effective Modern C++* Item 5 & 31 (deduction and lambdas underpin the
  trampoline).
- [CppCon 2021, Klaus Iglberger, *"Breaking Dependencies: Type Erasure —
  A Design Analysis"*](https://www.youtube.com/watch?v=4eeESJQk-mw) — the
  definitive hour on §3.
- James McNellis, *"The Curiously Recurring Template Pattern"* — and
  [cppreference on CRTP](https://en.cppreference.com/w/cpp/language/crtp).
- [P0792 `std::function_ref`](https://wg21.link/p0792) — the standards
  paper for the type you just built (adopted for C++26).
- Boost.Operators — §2's pattern, industrial strength.
