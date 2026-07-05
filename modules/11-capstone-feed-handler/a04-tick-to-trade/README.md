# a04 — Tick-to-Trade: Measure the Thing You Built

**Estimated effort:** 6–10 hours, half of it running experiments and
writing. The final assignment: less new code, more judgment — which is
the actual job.

## Learning objectives

- Instrument an end-to-end latency path correctly (clock placement,
  coordinated omission avoided by construction).
- Run a load experiment: latency vs offered rate, finding the knee.
- Write the one-page performance report that a desk lead would actually
  read — your portfolio piece.

## THEORY

### 1. What tick-to-trade means, exactly

The industry's headline number: **from the market-data packet arriving
at your process to the last byte of your order leaving it.** Your
plumbing already carries the start time — `TopOfBook.rx_time`, stamped
immediately after `recv` returned (NOT when the strategy got around to
popping it: that's m06a01's coordinated-omission lesson — the queue
wait is part of YOUR latency, and a clock started late hides it). The
stop time: in the gateway thread, right after `send_all` returns.
`t1 - rx_time` → your m06a01 histogram, one record per order. That's
the whole implementation: ~5 lines in your a03, plus returning the
histogram from `tick_to_trade()`.

What it deliberately excludes: the wire, the NIC, the kernel's receive
path before recv returned, and the exchange. Real firms measure those
too (hardware timestamps at the NIC — Solarflare/Arista tap-and-splice
setups); process tick-to-trade is the part you own with software. Say
both halves in an interview.

### 2. The experiment: latency vs load

One number is not a result. The experiment (harness given —
`m11a04_bench`) drives your pipeline through loopback at increasing
packet rates and prints p50/p99/p99.9/max at each. Expected shape:

- **Flat region:** at low rates, latency is the pipeline's intrinsic
  cost — recv + book + two ring hops + TCP send. On Apple Silicon
  loopback, healthy is **~10–40µs p50** (dominated by the kernel
  socket paths; your rings are noise at this scale — you PROVED that
  in m08a01).
- **The knee:** as offered rate approaches service rate, queues stop
  being empty; p99 lifts first (queueing theory in one sentence:
  utilization → 1 means wait → ∞). Find the knee; report it as your
  capacity.
- **Past the knee:** rings fill, your feed thread sheds superseded
  updates (`ring_full_drops` climbing) — the system degrades the way
  you DESIGNED it to (a03 THEORY §1), which is the difference between
  overload and failure.

### 3. The report is the deliverable

`REPORT_TEMPLATE.md` (next to this file) has the structure: setup,
method, results table, percentile plot (ASCII or a screenshot — the
dataviz matters less than the numbers being honest), analysis, and the
"what I'd do next" section. Two rules from the trade: every claim has
a number, every number has a method. A reader should be able to
reproduce your table from your text alone. This document — checked
into the repo with the code — is the artifact you show interviewers.

## Assignment spec

1. **Implement the measurement** in your a03 `Pipeline`:
   `tick_to_trade()` returns your gateway-thread histogram (declared in
   a03's header; recording happens after `send_all`). Single-threaded
   access pattern (record: gateway thread; read: after stop() — no
   synchronization needed; say why in a comment. m07's material).
2. **Pass the a04 tests** — they run the pipeline over loopback and
   interrogate the histogram (populated, plausible bounds, count ==
   orders_sent).
3. **Run the experiment**: `m11a04_bench` (and optionally the a03
   replayer with `gap_every` for the staleness drill). Save the output.
4. **Write `REPORT.md`** from the template. Commit it.

**Acceptance criteria:** `m11a04.*` green in `debug`, `asan`, `tsan`;
REPORT.md committed with real numbers from your machine.

## Hints

<details><summary>Hint 1 — count == orders_sent keeps failing</summary>
Record AFTER send_all returns (a failed send is not a trade — don't
record it), and read the histogram only after stop() joins the gateway
thread. If the count is off by the in-flight tail, your stop() drains
the order ring before joining — decide and document whether shutdown
drains or drops; the tests accept either IF stats and histogram agree.
</details>

<details><summary>Hint 2 — the numbers look too good</summary>
p50 of 2µs on loopback means your clock starts too late (rx_time
stamped in the strategy thread?) or stops too early. Walk the
timestamp's journey: recv → push → pop → decide → push → pop → send →
record. The m06a01 rule: when the answer flatters you, audit the
ruler first.
</details>

## Further reading

- Gil Tene, *"How NOT to Measure Latency"* — final rewatch; you now own
  every mistake he lists.
- Brendan Gregg, *Systems Performance* ch. 2 (methodologies) — the USE
  method for your "what I'd do next" section.
- David Gross (Optiver, Meeting C++ 2022) — compare his pipeline's
  numbers to yours; the gap is kernel bypass + tuning, now explainable.
