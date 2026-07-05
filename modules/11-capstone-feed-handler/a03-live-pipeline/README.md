# a03 — The Live Pipeline: Feed → Book → Strategy → Gateway

**Estimated effort:** 10–14 hours. The system assignment: real sockets,
real threads, your rings in the middle. Links m11a02 (and everything
under it), m08a01, m09a01.

## Learning objectives

- Wire the canonical trading-system shape: one thread per stage,
  wait-free rings between stages, sockets at both ends.
- Own thread lifecycle: startup order, clean shutdown of threads
  blocked in syscalls, no leaks, no races (tsan says so).
- Carry a timestamp through a pipeline — the plumbing a04 measures.

## THEORY

### 1. The architecture (and why each piece)

```text
UDP socket ──> [FEED thread] ──ring──> [STRATEGY thread] ──ring──> [GATEWAY thread] ──TCP──> exchange
               recv datagram           pop TopOfBook               pop OrderRequest
               BookBuilder             decide (given toy)          serialize + send_all
               push TopOfBook                                      record t1 (a04)
```

One thread per stage because each blocks on a different thing (the
feed on the NIC, the gateway on TCP) and the stage in the middle must
never block at all. **Rings between stages** (your m08a01, by value):
the feed thread must keep pace with the market no matter what — a slow
strategy backs up ITS ring; the feed drops top-of-book updates into a
full ring by OVERWRITING nothing — it simply doesn't push, because a
newer update is coming anyway (market data is superseding; order
requests are NOT — the strategy→gateway ring being full is a real
problem, counted in the stats). This asymmetry — which queue may shed
load — is a design interview question all by itself.

The messages are values (m01's lesson at system scale): `TopOfBook`
{bid, ask, seq, rx_time} and `OrderRequest{side, price, qty, rx_time}`
— 32-48 bytes copied through rings beats any shared-state locking
(module README bank), and `rx_time` rides along because a04's
tick-to-trade clock starts at the recv (m06a01's coordinated-omission
lesson: timestamp ARRIVAL, not processing).

### 2. Shutdown is the hard 20%

`stop()` must unblock: (a) the feed thread sitting in `recv` — the
standard trick is sending yourself one datagram on loopback (a
"poison packet"; check a flag after every recv return); (b) the
strategy/gateway threads spinning on `try_pop` — they check the stop
flag each idle loop (with a `cpu_pause()`); (c) join everything
(jthread), THEN close sockets. Order matters: close a socket a thread
is using and you've built a use-after-free with extra steps (m01a03's
fd lessons at system scale). The ShutdownIsClean test runs the whole
lifecycle twice to catch leftover state.

### 3. Staleness is a trading halt

The feed thread checks `builder.is_stale()` after every datagram: when
stale, it pushes NOTHING (the strategy quoting off a stale book is the
disaster a02 built the flag for). The GapHaltsTrading test injects a
gap and asserts the silence.

## Assignment spec

`include/course/pipeline.hpp` + `src/pipeline.cpp`. Given: the config,
message structs, and `toy_strategy` (a spread-crossing toy — the
strategy is deliberately NOT the interesting part). You implement the
`Pipeline` class:

- `explicit Pipeline(PipelineConfig)` — config carries `md_port` (0 =
  ephemeral), `gateway_port` (a TCP server the tests run), and
  `max_spread_ticks` for the toy.
- `bool start()` — bind UDP (expose the real port via `md_port()`),
  connect TCP to the gateway (with `set_nodelay()` — you know why),
  launch the three threads. False if either socket fails.
- `void stop()` — THEORY §2. Idempotent.
- `std::uint16_t md_port() const`.
- Wire format to the gateway (fixed, tests parse it):
  `"B|S,<price>,<qty>\n"` e.g. `"B,101,5\n"`.
- `PipelineStats stats() const` — `{packets, updates_pushed,
  orders_sent, ring_full_drops}` (atomics; approximate reads fine).
- Feed thread: recv → `builder.on_datagram` → if live AND top-of-book
  (bid or ask) changed since last push → push `TopOfBook` with the
  post-recv timestamp. Full md-ring: drop (superseding data), count.
- Strategy thread: pop → `toy_strategy` → maybe push `OrderRequest`.
  Full order-ring: count (`ring_full_drops`) — and know that in a real
  system this is an incident, not a counter.
- Gateway thread: pop → serialize → `send_all`.

**Acceptance criteria:** all `m11a03.*` green in `debug`, `asan`, and
**`tsan`** (three threads + two rings = tsan's home game). Tests drive
the real pipeline over loopback with deadline bounds.

## Hints

<details><summary>Hint 1 — member layout</summary>
Config, BookBuilder, UdpSocket + TcpStream (optionals — born in
start()), two rings (<code>SpscRing&lt;TopOfBook, 1024&gt;</code>,
<code>SpscRing&lt;OrderRequest, 1024&gt;</code>), three
<code>std::jthread</code>s, <code>std::atomic&lt;bool&gt; stop_</code>,
and the stats atomics. Declaration order = destruction order in
reverse — threads before sockets means threads die first. Think it
through once; it saves a shutdown crash.
</details>

<details><summary>Hint 2 — top-of-book change detection</summary>
Keep last pushed {bid, ask} in the feed thread (plain locals — single
thread, no sharing). Compare after each datagram; push only on change.
Pushing every packet floods the ring with duplicates and the
DeduplicatesUpdates test notices.
</details>

<details><summary>Hint 3 — the poison packet</summary>
stop(): set the flag, then send one dummy datagram to
<code>127.0.0.1:md_port()</code> from a throwaway UdpSocket so the
blocked recv returns. The feed loop checks the flag FIRST after every
recv — the dummy's content never matters. (The gateway TCP write side
needs no poison: it blocks on try_pop, which doesn't block.)
</details>

## Further reading

- Martin Thompson, *"Designing for Performance"* (GOTO 2015) — the
  single-writer principle your pipeline embodies.
- LMAX Disruptor paper — the famous ancestor of ring-based pipelines.
- Your m08a01 ping-pong bench numbers — the latency floor a04 will
  find, predicted by your own measurement.
