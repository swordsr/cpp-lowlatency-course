# Tick-to-Trade Report — <your name>, <date>

> Copy to REPORT.md, fill every <angle-bracket>, delete the guidance
> quotes. One page is the target; two is the ceiling.

## 1. System under test

- Hardware: <machine, chip, cores, RAM>
- OS / toolchain: <macOS version, compiler, build preset (release!)>
- Pipeline: commit <hash>; config: <max_spread_ticks, ring sizes>
- Measurement: steady_clock from post-`recv` to post-`send_all`,
  recorded in the gateway thread into the m06a01 histogram.
  <Anything else a reproducer needs.>

## 2. Method

> How you drove load, for how long, how many repetitions, what you
> discarded (warmup?) and why. A stranger reproduces your table from
> this paragraph alone.

<method>

## 3. Results

| offered rate (pps) | orders | p50 (µs) | p99 (µs) | p99.9 (µs) | max (µs) | drops |
|--------------------|--------|----------|----------|------------|----------|-------|
| <1k>               |        |          |          |            |          |       |
| <5k>               |        |          |          |            |          |       |
| <10k>              |        |          |          |            |          |       |
| <knee?>            |        |          |          |            |          |       |

Observed knee: <rate> pps.

## 4. Analysis

> Three paragraphs max. (1) Where does the flat-region latency GO —
> budget it (kernel socket path vs rings vs book vs strategy; cite
> your m08a01 ping-pong number for the ring share). (2) What happens
> at the knee, mechanically — which queue grows, which counter moves.
> (3) The p99.9 story: what causes YOUR tail (scheduling? bursts? a
> specific measurement?).

<analysis>

## 5. What I'd do next, in order

> Ranked, each with the measurement that would confirm it helped.
> (Candidates you've earned the right to name: pinning + RT priority
> on Linux, kernel bypass, batching recv (recvmmsg), the m08a01
> cached-cursor ring, a leaner gateway serialization...)

1. <change> — expect <effect>, verify by <measurement>.
2. ...

## 6. Honest limitations

> Loopback isn't a network; macOS isn't the prod OS; the strategy is a
> toy. Say what your numbers do and don't claim.

<limitations>
