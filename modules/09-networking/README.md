# Module 9 — Networking & Binary I/O

**Time budget:** ~2 weeks. Three assignments.

The last skills module before the capstones, and the one where the
machine's role changes: packets arrive when the *market* says so. You'll
wrap raw BSD sockets in RAII (a01), multiplex them with a kqueue event
loop (a02 — this one only builds on your Mac, by design), and decode the
kind of framed binary stream real exchanges emit (a03 — which is
`m11a01`'s warm-up lap).

The trading-system context for everything here: market data arrives as
**UDP multicast** (one packet, every subscriber; lossy by design — gap
handling is YOUR problem, see Module 11), order entry runs over **TCP**
(reliable, ordered, with Nagle's algorithm waiting to add 40ms to your
order if you forget one socket option). Serious shops bypass the kernel
entirely (Solarflare/ef_vi, DPDK, or FPGAs) — you should be able to
*say why* (per-packet syscall + copy + wakeup costs) even though this
course stays on the sockets API.

## Assignments

| # | Assignment | What you'll touch |
|---|------------|-------------------|
| 1 | [a01-sockets](a01-sockets/) | RAII sockets, TCP echo, UDP datagrams, byte order, TCP_NODELAY |
| 2 | [a02-kqueue-event-loop](a02-kqueue-event-loop/) | readiness multiplexing, kevent, timers (macOS only; epoll sidebar) |
| 3 | [a03-binary-protocol-parsing](a03-binary-protocol-parsing/) | framing, bounds-checked decoding, zero-copy, strict aliasing |

## Interview questions

<details>
<summary>"Why is market data UDP multicast but order entry TCP?"</summary>

Market data: one sender, thousands of receivers — multicast delivers one
packet to all NICs at line rate; TCP would mean per-subscriber
connections and the slowest consumer throttling everyone (or the
exchange buffering unboundedly). Loss is handled above UDP: sequence
numbers + snapshot/retransmission channels. Order entry: you need
guaranteed, ordered, acknowledged delivery of YOUR orders — exactly
TCP's contract (with TCP_NODELAY, and often a sequence-numbered session
protocol like OUCH on top anyway, because "TCP delivered it" ≠ "the
exchange accepted it").
</details>

<details>
<summary>"What does Nagle's algorithm do and why does every trading system disable it?"</summary>

Nagle batches small writes: with unacked data in flight, the kernel
holds new small segments until an ACK returns (potentially interacting
with delayed ACK for up to ~40-200ms of added latency). Great for
telnet in 1984; catastrophic for a 40-byte order. `setsockopt(fd,
IPPROTO_TCP, TCP_NODELAY, ...)` — the single most famous socket option
in finance. Follow-up: you still shouldn't do many tiny writes —
batch in userspace and write once.
</details>

<details>
<summary>"select vs poll vs epoll/kqueue — what actually improved?"</summary>

select/poll pass the ENTIRE fd set to the kernel per call — O(n) copy
and scan every time, plus select's FD_SETSIZE cap. epoll (Linux) and
kqueue (BSD/macOS) make the interest set *stateful in the kernel*:
register once, then each wait returns only ready fds — O(ready), not
O(watched). kqueue is the more general design (one API for fds, timers,
signals, processes); epoll is fd-only with timerfd/signalfd bolted on.
Same architecture, different spelling — knowing both marks you as
portable.
</details>

<details>
<summary>"Level-triggered vs edge-triggered readiness?"</summary>

Level: "fd IS readable" — reported as long as data remains; you may read
partially and be told again. Edge: "fd BECAME readable" — reported once
per transition; you must drain until EAGAIN or you'll wait forever on
data that already arrived. Edge saves wakeups on hot connections but
demands the drain-loop discipline and non-blocking fds. kqueue is
level-triggered by default (EV_CLEAR opts into edge); so is epoll
(EPOLLET opts in).
</details>

<details>
<summary>"Why does reading an int directly from a network buffer crash on some machines — and what's the portable idiom?"</summary>

`*(uint32_t*)(buf + 3)` is (1) potentially misaligned — UB, and a real
SIGBUS on some ARM configs — and (2) a strict-aliasing violation — UB
even aligned. The blessed idiom: `memcpy(&v, buf + 3, sizeof v)` —
compilers turn it into a single load on x86 AND ARM64 (both handle
misaligned loads in hardware); the memcpy is free and the UB is gone.
Then fix endianness explicitly. a03 makes you build exactly this.
</details>
