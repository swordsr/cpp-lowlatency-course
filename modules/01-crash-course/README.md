# Module 1 — C++ Crash Course for Polyglots

**Time budget:** ~1½ weeks. Three assignments.

You already know how to program; this module is about where C++'s *semantics*
differ from the JVM/Python model in your head. Syntax is picked up along the
way — keep [A Tour of C++] and cppreference open and move fast. The three
recurring themes:

1. **Objects are values.** A `Foo f` *is* the object — not a reference to one.
   Assignment copies. Everything in Module 2 builds on this.
2. **`const` is load-bearing.** It's a compiler-checked API contract, not
   documentation.
3. **Destructors run deterministically.** At scope exit, in reverse order,
   even during exceptions. This one idea (RAII) replaces `finally`,
   try-with-resources, and most of what the GC did for you.

## Assignments

| # | Assignment | What you'll touch |
|---|------------|-------------------|
| 1 | [a01-value-semantics](a01-value-semantics/) | value types, copies, references, const-correctness |
| 2 | [a02-strings-vectors-algorithms](a02-strings-vectors-algorithms/) | string/string_view, vector, iterators, `<algorithm>` |
| 3 | [a03-raii-first-contact](a03-raii-first-contact/) | destructors, RAII resource wrappers, error-handling idioms |

Do them in order — a02 assumes a01's mental model, a03 assumes both.

## Interview questions

<details>
<summary>"In Java, <code>Foo f = g;</code> aliases; in C++ it copies. What are the consequences of value semantics for API design?"</summary>

Copies are independent (no spooky action at a distance), objects can live on
the stack and in contiguous arrays (cache-friendly — Module 6), and functions
must *choose* how to take parameters: by value (copy, callee owns), by
`const&` (read-only view, no copy), by `&` (in/out). The cost: accidental
expensive copies are easy; C++ answers with move semantics (Module 2).
</details>

<details>
<summary>"What does a <code>const</code> member function promise, and what breaks if you lie?"</summary>

It promises not to modify the observable state of the object, so it can be
called through `const&` — which is how read-only parameters are passed
everywhere. A non-const getter makes the type unusable through `const&`.
Casting away const and mutating an actually-const object is UB.
</details>

<details>
<summary>"Reference vs pointer — when would you use each?"</summary>

A reference is a non-null, non-reseatable alias — use it wherever those
constraints hold (most parameters). A pointer can be null and can be
reseated — use it when "absent" is a real state or when rebinding is needed.
Interview follow-up: references aren't objects; there's no "reference to
reference", and a dangling reference is just as UB as a dangling pointer.
</details>

<details>
<summary>"What is <code>std::string_view</code> and what's the classic bug with it?"</summary>

A non-owning (pointer, length) pair over character data — cheap to copy and
substring, no allocation. The bug: the view outlives the owner. Canonical
case: `std::string_view sv = make_string();` — the temporary dies at the end
of the statement and `sv` dangles. Never store a `string_view` without an
answer for "who owns the bytes and until when?"
</details>

<details>
<summary>"What is the small-string optimization?"</summary>

`std::string` stores short strings (typically ≤15 bytes with libc++/libstdc++)
inline inside the object instead of heap-allocating. Consequences: short
strings are cheap to copy and cache-friendly; crossing the SSO boundary
suddenly allocates; and pointers/views into a string are invalidated by
operations that reallocate it.
</details>

<details>
<summary>"How does RAII compare to try-with-resources / <code>finally</code>?"</summary>

Same goal, but inverted responsibility: in Java the *call site* must remember
the cleanup construct; in C++ the *type* encodes cleanup in its destructor,
so every user gets it automatically — including in early returns and
exception unwinding, and for members of other objects. There is no way to
forget it. This is why C++ has no `finally`.
</details>

[A Tour of C++]: https://www.stroustrup.com/tour3.html
