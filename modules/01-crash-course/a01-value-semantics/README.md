# a01 — Value Semantics: Running Statistics

**Estimated effort:** 3–5 hours.

## Learning objectives

- Internalize that C++ objects are values: copies are deep and independent,
  and you get correct copying *for free* when your members are value types.
- Write a const-correct class: const member functions, `const&` parameters.
- Implement a numerically sound streaming algorithm (Welford) and a merge
  operation — the shape of every metrics aggregator you'll ever build.

## THEORY

### 1. `Foo f` is the object

In Java, every non-primitive variable is a pointer the language hides from
you; `new` is the only way to make an object, and the GC owns them all. In
C++:

```cpp
RunningStats a;      // a real object, right here, in this stack frame
RunningStats b = a;  // ANOTHER object — a memberwise COPY of a
b.add(42.0);         // b changed; a did not
```

There is no shared object behind `a` and `b`. This is *value semantics*.
Java's `int` behaves this way; C++ makes *every* type behave this way by
default, including yours. The object's bytes live wherever you declared it —
here, the stack frame — and die at scope exit. No allocation happened above.

The compiler writes the copy for you (memberwise), and for a class whose
members are all doubles and ints, that default is exactly right. Rule of
thumb you'll formalize in Module 2: if your members are value types, write
**no** copy code — the "Rule of Zero."

### 2. Choosing parameter types — the everyday decision

```cpp
void f(RunningStats s);        // by value: copy; callee has its own
void f(const RunningStats& s); // by const ref: read-only view of caller's
void f(RunningStats& s);       // by ref: callee mutates caller's
```

Idiom: cheap-to-copy types (ints, doubles, `string_view`, spans) by value;
everything else read-only by `const&`; out/in-out by `&`. When you write
`merge(const RunningStats& other)`, the signature *is* documentation — and
unlike documentation, the compiler enforces it.

### 3. `const` is a contract, not a comment

A member function declared `double mean() const` promises not to mutate the
object, and only const member functions are callable on a `const
RunningStats&`. Get this wrong in one place and const-correctness rots
through the whole codebase — a const-poisoned API is a classic C++ code
smell interviewers probe. The tests call your accessors through `const&`;
they won't compile if you forget.

### 4. Streaming statistics and why "sum of squares" is wrong

The naive variance — accumulate `Σx` and `Σx²`, then `Σx²/n − mean²` —
catastrophically cancels when the variance is small relative to the mean²
(exactly the shape of latency data: mean 800ns, jitter 5ns). The standard
fix is **Welford's online algorithm**: maintain running `mean` and `M2`
(sum of squared deviations *from the current mean*):

```
count += 1
delta  = x - mean
mean  += delta / count
M2    += delta * (x - mean)      // note: uses the UPDATED mean
variance = M2 / count            // population variance
```

Merging two partial aggregates (think: per-thread stats combined at the end
— you'll do it for real in Module 7) uses the Chan et al. parallel formula;
deriving it is part of the assignment (Hint 3 has it).

> **Domain note:** doubles are fine here — statistics are diagnostics, not
> money. The "no floating point" rule from Module 0 is about *prices*.

## Assignment spec

Implement `RunningStats` in `include/course/running_stats.hpp` (header-only;
fill in the `// TODO` bodies and add whatever private state you need).

- `add(double x)` — O(1), constant memory, Welford update.
- `count()`, `mean()`, `min()`, `max()`, `variance()` — all `const`.
  Population variance (÷ n). Calling anything but `count()` on an empty
  object is a precondition violation; the tests never do it.
- `merge(const RunningStats& other)` — after `a.merge(b)`, `a` equals the
  stats of both streams concatenated; `b` is untouched. Merging an empty
  object (either side) must work.
- Copies must be independent (the tests check; if you follow the Rule of
  Zero this costs you nothing).

**Acceptance criteria:** all `m01a01.*` tests pass in `debug` and `asan`.
No heap allocation anywhere (you shouldn't even feel tempted).

## Hints

<details><summary>Hint 1 — state</summary>
Five members suffice: <code>count_</code>, <code>mean_</code>,
<code>m2_</code>, <code>min_</code>, <code>max_</code>. Don't initialize
min/max to 0 — first <code>add</code> should set them unconditionally (or
initialize to ±infinity from <code>&lt;limits&gt;</code>).
</details>

<details><summary>Hint 2 — the failing variance test</summary>
<code>ShiftedDataHasSameVariance</code> feeds values near 1e9 with tiny
spread. If it fails but <code>BasicVariance</code> passes, you implemented
the naive Σx² formula — reread THEORY §4.
</details>

<details><summary>Hint 3 — merge formula</summary>
With <code>n = n_a + n_b</code> and <code>delta = mean_b - mean_a</code>:
<code>mean = mean_a + delta * n_b / n</code> and
<code>M2 = M2_a + M2_b + delta² * n_a * n_b / n</code>.
Handle <code>n_a == 0</code> or <code>n_b == 0</code> before dividing.
</details>

## Further reading

- *A Tour of C++* ch. 1, 4.2 (classes), 5.2 (copy).
- [Welford's algorithm](https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance)
  — including the parallel/merge variant (Chan et al.).
- Herb Sutter, [GotW #4: Class Mechanics](https://herbsutter.com/2013/05/20/gotw-4-class-mechanics/)
  — const-correctness habits.
