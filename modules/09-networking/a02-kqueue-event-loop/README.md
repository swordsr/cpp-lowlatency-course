# a02 — kqueue Event Loop: One Thread, Many Sockets

**Estimated effort:** 6–9 hours. **macOS-only by design** — this
assignment builds only on your Mac (CMake skips it elsewhere); the
Linux/epoll equivalent is taught side-by-side in THEORY because
interviews will ask for that one.

## Learning objectives

- Explain readiness-based multiplexing and why event loops beat
  thread-per-connection for I/O-bound work.
- Drive kqueue directly: kevent registration, the wait loop, dispatch.
- Handle timers as loop events (EVFILT_TIMER) — no sleeping threads.
- Translate every line to epoll for the Linux interview.

## THEORY

### 1. The problem: 1000 sockets, one hot core

Thread-per-connection burns a stack and a scheduler slot per socket and
context-switches on every packet — fine at 10 connections, absurd at
10,000 (the C10K problem, solved twenty years ago; the solution became
the architecture). The alternative: make every fd non-blocking, ask the
kernel "which of these are ready RIGHT NOW?", and handle exactly those,
on one thread. No locks (one thread!), no switches, and the hot core
stays hot — which is why every feed handler, every gateway, nginx,
Redis, and Node.js are all this same loop.

### 2. kqueue: the kernel keeps your interest list

select/poll hand the kernel the whole fd list *every call* — O(n) per
wait. kqueue (like epoll) is stateful: register interest once, then
each `kevent()` wait returns only what fired:

```cpp
int kq = kqueue();                          // the event queue fd
struct kevent change;
EV_SET(&change, fd, EVFILT_READ,            // "tell me when fd is readable"
       EV_ADD, 0, 0, nullptr);
kevent(kq, &change, 1, nullptr, 0, nullptr);        // register
struct kevent fired[64];
int n = kevent(kq, nullptr, 0, fired, 64, &timeout); // wait
for (int i = 0; i < n; ++i) handle(fired[i].ident);  // dispatch
```

One syscall registers OR waits (or both at once — the two middle
argument pairs). `ident` is the fd; `filter` says what kind of event;
`udata` is a user pointer riding along. Level-triggered by default:
unread data keeps firing (the forgiving mode; EV_CLEAR opts into
edge-triggered and its drain-until-EAGAIN discipline — module README
bank).

> **Linux/epoll sidebar — learn this table cold:**
> | kqueue | epoll |
> |--------|-------|
> | `kqueue()` | `epoll_create1(0)` |
> | `kevent(kq, &chg, 1, ...)` + EV_ADD/EV_DELETE | `epoll_ctl(ep, EPOLL_CTL_ADD/DEL, fd, &ev)` |
> | `kevent(kq, ..., fired, n, &ts)` | `epoll_wait(ep, fired, n, ms)` |
> | `EVFILT_READ` / level default / `EV_CLEAR` | `EPOLLIN` / level default / `EPOLLET` |
> | `EVFILT_TIMER` built in | `timerfd_create` + watch the fd |
>
> Same architecture; kqueue folds timers/signals/etc. into one API where
> Linux gives each its own fd flavor.

### 3. Timers belong to the loop

A trading event loop needs "if no heartbeat in 500ms, act" without a
sleeping thread. kqueue: `EVFILT_TIMER` with `EV_ONESHOT` — the timer is
just another event in the same wait. The loop thereby owns ALL time:
every callback — packet or timer — runs on the loop thread,
sequentially. That single-threadedness is the point: state touched only
from the loop needs no locks. (The classic sin follows: a slow callback
stalls *everything* — watch feed handlers hand heavy work to another
thread via your m08a01 ring instead.)

## Assignment spec

`include/course/event_loop.hpp` + `src/event_loop.cpp`:

`EventLoop` — single-threaded readiness loop. Constructor/destructor
(kqueue fd creation/close) are **given**; you implement:

- `bool watch_read(int fd, std::function<void(int)> on_readable)` —
  level-triggered read watch; callback gets the fd. Re-watching an
  already-watched fd replaces its callback.
- `bool unwatch(int fd)` — deregister; no callbacks after.
- `void add_timer(std::chrono::milliseconds delay, std::function<void()>
  on_fire)` — one-shot; fires on the loop thread.
- `int run_once(std::chrono::milliseconds max_wait)` — one kevent wait +
  dispatch of everything that fired; returns number of events handled
  (0 on timeout).
- `void run()` / `void stop()` — run_once until stop() (callable from a
  callback) flips the flag.

**Acceptance criteria:** all `m09a02.*` tests pass in `debug` and `asan`
**on your Mac** (CI runs them too — it's an Apple Silicon runner). The
tests drive the loop with pipes: deterministic, no network needed.

## Hints

<details><summary>Hint 1 — bookkeeping members</summary>
A <code>std::unordered_map&lt;int, std::function&lt;void(int)&gt;&gt;</code>
for read callbacks; a map from timer id (any unique uintptr — a counter)
to its callback. Dispatch looks up by <code>fired[i].ident</code> and
<code>fired[i].filter</code> — the same ident can exist under different
filters.
</details>

<details><summary>Hint 2 — EVFILT_TIMER one-shot registration</summary>
<code>EV_SET(&chg, timer_id, EVFILT_TIMER, EV_ADD | EV_ONESHOT,
0, delay_ms, nullptr)</code> — data is milliseconds by default. Erase
the callback from your map after firing (one-shot means the kernel
already forgot it; you should too — and mind iterator invalidation if
the callback adds another timer).
</details>

<details><summary>Hint 3 — run_once's dispatch loop</summary>
Copy the callback out of the map BEFORE invoking (<code>auto cb =
it-&gt;second;</code>): a callback that calls unwatch()/watch_read()
mutates the map you'd otherwise be holding an iterator into. The
UnwatchDuringCallback test exists because everyone learns this the
hard way once.
</details>

## Further reading

- `man 2 kqueue` — genuinely well-written; read EVFILT_READ and
  EVFILT_TIMER sections fully.
- Jonathan Lemon, *"Kqueue: A generic and scalable event notification
  facility"* (USENIX 2001) — the design paper, short and readable.
- [libuv design docs](https://docs.libuv.org/en/v1.x/design.html) — how
  a production cross-platform loop wraps kqueue/epoll/IOCP.
- The C10K page (module README) for the lineage.
