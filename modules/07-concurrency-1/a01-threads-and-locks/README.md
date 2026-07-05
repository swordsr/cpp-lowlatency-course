# a01 — Threads & Locks: A Blocking Queue

**Estimated effort:** 5–7 hours.

## Learning objectives

- Create and join threads the RAII way (`std::jthread`), and know what
  happens if you don't.
- Use `std::mutex` + `std::condition_variable` correctly: predicate
  loops, notify placement, lock scopes.
- Build the workhorse structure of threaded systems — a bounded-brain,
  unbounded-storage blocking MPMC queue with clean shutdown semantics.
- Map every piece onto what you know from Java
  (`synchronized`/`wait`/`notify`, `ArrayBlockingQueue`) and note where
  C++ trusts you with the sharp edge.

## THEORY

### 1. Threads: the RAII rule you already believe

`std::thread` terminates your whole program if destroyed while joinable —
forgetting `join()` isn't a leak, it's `std::terminate`. C++20's
`std::jthread` joins in its destructor (RAII wins again) and adds
cooperative cancellation via `std::stop_token`. Use `jthread` by default;
know `thread` exists because every codebase predates it.

> **Toolchain reality check:** Apple's libc++ *still* hasn't shipped
> `std::jthread` (libstdc++ has had it for years) — so the course
> provides `course::Jthread` in `common/include/course/jthread.hpp`:
> the real `std::jthread` where available, a join-on-destruction
> wrapper where not. Use it in your code; learn `std::jthread`'s name
> and semantics for interviews. "Library support lags the standard —
> check the implementation status pages" is itself a lesson worth the
> detour.

Cost intuition: creating a thread is ~10–50µs (syscall, stack allocation)
— never on a hot path; a context switch ~1–10µs plus cache pollution.
Trading systems create all threads at startup, pin them to cores, and
never let them sleep. This module's *blocking* designs are the general-
purpose baseline; Module 8 builds what HFT actually runs — but you must
be fluent in both, and interviews check the blocking one first.

### 2. Mutex + lock objects, not lock calls

```cpp
std::mutex m;
{
    std::lock_guard lock{m};   // locks now, unlocks at }
    // touch shared state
}                               // exception-safe by construction
```

`lock_guard` is the `synchronized` block with visible cost; `unique_lock`
is the flexible sibling (can unlock early, required by condition
variables). The C++ difference from Java: the mutex and the data it
guards are associated *by convention only* — nothing stops an unguarded
access. Discipline: every shared mutable field's declaration gets a
comment naming its mutex, and every access site holds it. (TSan audits
this convention mechanically — that's why the preset is mandatory now.)

### 3. Condition variables: the three commandments

A CV is a waiting room keyed to a mutex-guarded condition:

```cpp
std::unique_lock lock{m_};
cv_.wait(lock, [&] { return !queue_.empty() || closed_; });  // (1)
```

1. **Always wait with a predicate.** Spurious wakeups are legal, and a
   notified waiter can lose the race to a barging thread that consumes
   the condition first; the predicate re-check makes both harmless.
2. **Only change the condition while holding the mutex** — otherwise the
   check-then-sleep inside `wait` races the change and the notify slips
   into the gap: a lost wakeup, the classic once-a-week production hang.
3. **`notify_one` for one unit of work, `notify_all` for state changes**
   (like shutdown) that every waiter must observe.

Java mapping: `wait/notify` on an object monitor, with the same predicate
loop the Java docs also insist on and half of production Java skips.

### 4. Shutdown is part of the interface

The hard design question in any blocking structure: how does a consumer
blocked in `pop()` learn the show is over? This queue's answer (the
standard one): `close()` — after it, `push` returns false, and `pop`
drains remaining items then returns `std::nullopt`. Closing wakes *all*
waiters (`notify_all` — commandment 3). Alternatives you should be able
to discuss: poison-pill sentinel items (one per consumer, clumsy) and
`std::stop_token`-aware CVs (`condition_variable_any`, heavier).

## Assignment spec

`include/course/blocking_queue.hpp` (header-only):

`BlockingQueue<T>` — unbounded FIFO, any number of producers/consumers:

- `bool push(T value)` — enqueue + wake one waiter; false (and drop) if
  closed.
- `std::optional<T> pop()` — blocks until an item or closed; drains
  leftovers after close; nullopt only when closed AND empty.
- `bool try_pop(T& out)` — non-blocking.
- `std::size_t size() const`.
- `void close()` — idempotent; wakes everyone.

**Acceptance criteria:** all `m07a01.*` tests pass in `debug`, `asan`,
AND **`tsan`** — from this module on, tsan-clean is part of "done". The
tests run real multi-threaded scenarios with a 60s ctest timeout: a
deadlock shows up as a timeout failure, and that *is* the test working.

## Hints

<details><summary>Hint 1 — member layout</summary>
<code>std::queue&lt;T&gt;</code> (or your m02a04 Vector + head index),
<code>mutable std::mutex</code> (mutable so <code>size() const</code> can
lock), <code>std::condition_variable</code>, <code>bool closed_</code>.
Every member access inside every method happens under the lock — no
exceptions in this assignment.
</details>

<details><summary>Hint 2 — pop, exactly</summary>
unique_lock; <code>cv_.wait(lock, [&]{ return !q_.empty() ||
closed_; });</code> then: if queue non-empty → take front (move!), pop,
return it; else (closed and empty) → nullopt. Note the drain: closed
with items left still returns items — the predicate and this order
encode that.
</details>

<details><summary>Hint 3 — a test hangs; now what</summary>
That's a lost wakeup or a wait without predicate. Check: does every
path that makes the predicate true notify? Does close()
<code>notify_all</code>? Is the condition mutated under the mutex
(commandment 2)? Run the tsan preset — it often names the racing line
outright.
</details>

## Further reading

- *C++ Concurrency in Action* 2e, ch. 2–4 — this module's textbook;
  ch. 4.1 is this exact queue.
- [std::jthread](https://en.cppreference.com/w/cpp/thread/jthread) and
  P0660 (stop_token rationale).
- Herb Sutter, *"Lock-Free Programming (or, Juggling Razor Blades)"*
  (CppCon 2014) part 1 — watch now as Module 8's trailer.
