# Module 11 — Capstone II: Feed Handler & the Integrated Pipeline

**Time budget:** ~3½ weeks. Four stages. Everything you've built gets
conscripted: m09a03's wire skills (a01 links it for your ByteReader),
m10a01's book (a02 rebuilds books from the feed), m08a01's SPSC rings
and m09a01's sockets (a03 wires the live pipeline), m06a01's histogram
(a04 measures tick-to-trade). By the end you have the classic
HFT-portfolio system: **UDP feed → book builder → strategy → order
gateway**, measured end to end.

The protocol is "CMD" — Course Market Data — an honest miniature of
Nasdaq ITCH over MoldUDP64: sequenced datagrams carrying add/execute/
delete messages for OTHER people's orders, from which you reconstruct
the market. The two hard truths that shape everything:

1. **UDP loses packets.** Sequence numbers make loss *detectable*; a
   snapshot + replay protocol makes it *recoverable* (a02). Until
   recovered, your book is a lie — and trading on a stale book is how
   firms make the news.
2. **The pipeline is only as honest as its clock.** Tick-to-trade is
   measured from the packet's ARRIVAL (m06a01's coordinated-omission
   lesson, in production form), through every queue, to the last byte
   of the order write (a04).

## Assignments

| # | Assignment | What you'll touch |
|---|------------|-------------------|
| 1 | [a01-itch-parser](a01-itch-parser/) | CMD decoding, sequencing, gap detection, heartbeats |
| 2 | [a02-book-builder](a02-book-builder/) | feed → your m10 book; stale state; snapshot + replay recovery |
| 3 | [a03-live-pipeline](a03-live-pipeline/) | three threads, two rings, real sockets; a UDP replayer tool |
| 4 | [a04-tick-to-trade](a04-tick-to-trade/) | end-to-end latency, the experiment, the write-up |

## Interview questions

<details>
<summary>"Walk me through what happens when your feed handler misses a packet."</summary>

Detection: the next packet's first sequence number exceeds what I
expect — the gap size is known instantly. Response: mark the book
stale (STOP quoting off it — that's a risk control, not an
optimization), buffer arriving packets, request recovery. Recovery:
either the retransmission channel (small gaps) or the snapshot channel
(big ones): load the snapshot at sequence S, replay buffered packets
with sequence filtering (messages ≤ S are already reflected), go live
when contiguous. The follow-ups probe the edges: gap during recovery
(re-arm), duplicates across the snapshot boundary (the seq filter),
and how long you can be stale before it matters (venue and strategy
dependent — knowing it's a *risk* question scores the point).
</details>

<details>
<summary>"Why does your pipeline use SPSC queues between stages instead of locking a shared book?"</summary>

Stage isolation: the feed thread must NEVER wait — packets don't. Each
stage owns its data (feed owns the book, strategy owns decisions,
gateway owns the socket); stages communicate by VALUE through wait-free
rings (m08a01), so a slow gateway backs up its ring instead of stalling
the feed. The alternative — strategy threads locking the book — puts
reader stalls on the packet path. Cite the numbers: ring hop ~100-300ns
and bounded; an unlucky mutex, unbounded.
</details>

<details>
<summary>"Your tick-to-trade p50 is 8µs but p99.9 is 900µs. Debug it."</summary>

The shape says queueing or scheduling, not compute. Suspects in order:
(1) coordinated-omission-style bursts — packets arriving faster than a
stage drains its ring (check ring occupancy high-water marks);
(2) the OS descheduling an unpinned thread (macOS won't let you pin;
say the Linux words: isolcpus + taskset + rt priority); (3) allocation
or syscall on the hot path (the m10a03 tattletale technique); (4) TCP
send blocking on a full socket buffer. The answer they want is the
*ordered list of hypotheses with a measurement for each*, not a guess.
</details>
