# Module 7 — Concurrency I: Threads, Atomics, Ordering

**Time budget:** ~2 weeks. Three assignments, strictly in order.

Two modules of concurrency, split by tool: this one covers threads, locks,
and the memory model — the vocabulary and the correctness rules — and
Module 8 spends it all on lock-free structures.

Your machine is the star this module: **Apple Silicon has a weakly
ordered memory model**, so the reorderings that x86 politely hides
actually happen on your CPU. The relaxed-atomics bugs in a02 *reproduce*
on your laptop while an x86 colleague would swear the code is fine. This
is the best concurrency-learning hardware money can buy.

From here on, the **`tsan` preset is mandatory equipment**:
`cmake --preset tsan && cmake --build --preset tsan && ctest --preset
tsan -R m07`. ThreadSanitizer sees the races your tests get lucky on.

1. **a01** — the blocking toolbox: mutexes, condition variables, and a
   producer/consumer queue (your Java `BlockingQueue`, rebuilt honestly);
2. **a02** — the memory model: happens-before, acquire/release, and
   watching relaxed atomics break on your own CPU;
3. **a03** — spinning: TAS/TTAS/backoff locks, MESI traffic, and false
   sharing — the performance bug you can't see in source code.

## Assignments

| # | Assignment | What you'll touch |
|---|------------|-------------------|
| 1 | [a01-threads-and-locks](a01-threads-and-locks/) | jthread, mutex, condition_variable, blocking queue |
| 2 | [a02-memory-ordering](a02-memory-ordering/) | atomics, acquire/release, litmus experiments, double-checked locking |
| 3 | [a03-spinlocks-and-false-sharing](a03-spinlocks-and-false-sharing/) | TAS/TTAS/backoff, MESI, interference size |

## Interview questions

<details>
<summary>"What IS a data race, precisely, and what happens if you have one?"</summary>

Two threads access the same memory location, at least one is a write, and
neither access happens-before the other, with at least one non-atomic.
The consequence is not "you might read a stale value" — it's **undefined
behavior**: the compiler optimizes assuming races don't exist, so torn
values, vanishing writes, and impossible control flow are all on the
table. `std::atomic` isn't "slower but safe access"; it's what makes the
program's meaning defined at all.
</details>

<details>
<summary>"Explain acquire/release in one whiteboard diagram."</summary>

A release store is a one-way gate: nothing written before it may move
after it. An acquire load is the matching gate: nothing read after it may
move before it. When an acquire load READS THE VALUE a release store
wrote, everything before the store becomes visible after the load
(synchronizes-with → happens-before). That pairing is the entire
message-passing pattern: write payload, release-store flag; acquire-load
flag, read payload.
</details>

<details>
<summary>"x86 vs ARM memory models — what actually differs?"</summary>

x86 is TSO: loads aren't reordered with loads, stores aren't reordered
with stores; the only visible relaxation is a store followed by a load to
a different address (store buffer). So acquire/release on x86 is mostly
free — plain MOVs already behave that way — and code with missing
barriers often *works on x86 by luck*. ARM64 is weakly ordered: loads and
stores reorder freely unless you use `ldar`/`stlr` (acquire/release) or
barriers. Same C++ source, different instruction cost and different bug
visibility. `seq_cst` additionally needs a global order — extra fences on
ARM, `xchg`-style ops on x86.
</details>

<details>
<summary>"Why do condition variable waits need a predicate loop?"</summary>

Spurious wakeups are permitted (futex/kernel-level artifacts), and more
importantly the condition can change between notify and the waiter
re-acquiring the lock (stolen wakeup). `cv.wait(lock, [&]{ return
ready; })` re-checks under the lock, making both harmless. Forgetting the
predicate is the #1 CV bug; the #2 is modifying the condition without
holding the mutex, which races the check inside wait.
</details>

<details>
<summary>"What is false sharing and how do you find it in production?"</summary>

Two threads write different variables that share a cache line: coherence
(MESI) ping-pongs the line between cores — each write invalidates the
other core's copy, so both run at cross-core-latency speed while touching
"independent" data. Find it: perf c2c on Linux (HITM events), or the
old-fashioned way — suspicious adjacent hot fields + an alignas
experiment. Fix: pad/align hot per-thread data to the destructive
interference size (64B x86, 128B Apple Silicon).
</details>

<details>
<summary>"When is a spinlock better than a mutex?"</summary>

When the critical section is shorter than a context switch (~1–10µs),
the lock is mostly uncontended, and threads are pinned to dedicated cores
that have nothing better to do — i.e., exactly the HFT setup. A blocking
mutex costs a syscall on contention (and a scheduler round-trip to wake);
a spinlock costs cache-line traffic and burns a core. On shared/oversubscribed
machines spinning is antisocial: you're burning the cycles the lock
holder needs to finish. (And a spinning thread holding the lock through
a context switch is the pathological case: everyone spins on a sleeping
owner.)
</details>
