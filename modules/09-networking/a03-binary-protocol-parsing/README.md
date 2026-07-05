# a03 — Binary Protocol Parsing: Bytes Off the Wire, Safely, Fast

**Estimated effort:** 6–8 hours. Direct warm-up for the Module 11 feed
handler — everything here reappears there at ITCH scale.

## Learning objectives

- Decode little-endian wire integers with the one idiom that is
  simultaneously UB-free, alignment-safe, and free at -O2: `memcpy`.
- Build a bounds-checked cursor (`ByteReader`) that makes truncated or
  hostile input a `nullopt`, never a heap overflow.
- Handle TCP framing: reassembling length-prefixed messages from
  arbitrary chunk boundaries — the stateful part everyone underestimates.
- Parse zero-copy: views into the receive buffer, owning nothing.

## THEORY

### 1. The Course Tape Protocol (CTP)

A miniature exchange feed, deliberately shaped like Nasdaq ITCH's
add/execute/cancel core. **All integers little-endian** (like ITCH's
cousins and most modern feeds — "network order" lost that war):

```text
frame   := u16 payload_len, payload            (len covers payload only)
payload := type u8, fields...
  'A' AddOrder : u64 order_id, i64 price_ticks, u32 qty, u8 side   (22 bytes)
  'E' Execute  : u64 order_id, u32 exec_qty                        (13 bytes)
  'X' Cancel   : u64 order_id                                      (9 bytes)
```

Exact-length discipline: a payload longer OR shorter than its type
demands is rejected. Real feed handlers are this strict — trailing bytes
mean you and the exchange disagree about the spec, and continuing means
you're trading on garbage.

### 2. The memcpy idiom (module README, now for real)

`*(uint32_t*)(buf + 3)` is misaligned-access UB (SIGBUS on some ARM
configs) *and* strict-aliasing UB (miscompiles at -O2 on any). The
blessed decode:

```cpp
std::uint32_t v;
std::memcpy(&v, buf + 3, sizeof v);   // compiles to ONE load on ARM64 & x86
v = /* if the wire were BE you'd swap here; CTP is LE like your CPU */ v;
```

Look at it in Compiler Explorer: `-O2` turns the memcpy into `ldr w0,
[x0, #3]` — the "safe" version costs nothing. That's the whole story:
there is no performance excuse for the UB version, and the UB version
has shipped real crashes. (For the byte-swap case: `__builtin_bswap32`
/ `std::byteswap` in C++23 — one instruction too.)

### 3. Framing: TCP hands you a byte hose

Module 9's a01 taught you `recv` returns *whatever's there*: half a
message, three and a half messages, one byte. A length-prefixed protocol
plus a reassembly buffer restores message boundaries:

```text
feed(chunk): append chunk to pending
             while pending holds a full frame (2 + len bytes):
                 emit payload view, drop the frame from pending
```

The classic bugs the tests aim at: a length prefix itself split across
chunks (you have 1 of the 2 length bytes — wait, don't guess); a frame
completed exactly at a chunk boundary; several frames arriving in one
chunk. The byte-by-byte test feeds a whole tape one byte at a time —
if your splitter survives that, it survives production.

### 4. Zero-copy and its contract

`ByteReader` and `decode` work on `std::string_view`s into the caller's
buffer — no allocation, no copy, just pointer math (m01a02's lesson at
its destination). The contract that buys the speed: **decoded views and
the Messages built from them are valid only until the buffer mutates**
— which in the splitter means "during the callback". Module 11's book
builder consumes messages inside the callback for exactly this reason.
The one copy we do make: `FrameSplitter`'s pending buffer, because
bytes that straddle chunks must live *somewhere*. Real handlers with
UDP (one datagram = whole frames) skip even that — as will yours.

## Assignment spec

`include/course/wire.hpp` + `src/wire.cpp`:

- **`ByteReader`** — cursor over a `string_view`: `remaining()`,
  `read_u8/u16/u32/u64/i64` → `std::optional<...>` (LE, memcpy idiom,
  advances on success; a failed read leaves the cursor put).
- **Messages** — `AddOrder{order_id, price_ticks, qty, side}`,
  `Execute{order_id, exec_qty}`, `Cancel{order_id}`;
  `using Message = std::variant<AddOrder, Execute, Cancel>`.
- **`decode(std::string_view payload)`** → `std::optional<Message>` —
  type byte + fields, exact length, unknown type ⇒ nullopt.
- **`FrameSplitter`** — `std::size_t feed(std::string_view chunk, const
  std::function<void(std::string_view)>& on_payload)` — reassembles
  frames across arbitrary chunk boundaries, invokes the callback per
  complete payload (zero-length payloads included — deliver them, let
  decode reject), returns how many payloads were delivered.

**Acceptance criteria:** all `m09a03.*` tests pass in `debug` and
`asan` — asan is load-bearing: every truncation test is one missed
bounds check away from a buffer over-read.

**Benchmark:** `m09a03_bench` (release, complete harness): decode
throughput over a synthetic million-message tape. Target: **≥ 300
MB/s** (≈ 15M+ msgs/s) on Apple Silicon once green — if you're an order
of magnitude under, something allocates per message (the profiler will
point at it in one run).

## Hints

<details><summary>Hint 1 — ByteReader shape</summary>
Members: the view + an offset. Each read: <code>if (remaining() &lt;
sizeof(T)) return std::nullopt;</code> memcpy from
<code>data() + offset</code>, advance, return. Write read_u16 first,
make its tests pass, then the rest are copies. (A private
<code>template &lt;typename T&gt; read()</code> is cleaner — your
call.)</details>

<details><summary>Hint 2 — decode is a switch on the type byte</summary>
Read u8 type; switch: each case reads its fields IN ORDER, then
requires <code>reader.remaining() == 0</code> (exact length — THEORY
§1). Any nullopt anywhere → nullopt out. std::optional's monadic
and_then (C++23) isn't available — plain ifs are fine and readable.
</details>

<details><summary>Hint 3 — the splitter's loop</summary>
Append chunk to <code>pending_</code>. Loop: if
<code>pending_.size() &lt; 2</code> break; read the LE u16 len (your
ByteReader on the pending buffer!); if <code>pending_.size() &lt; 2 +
len</code> break; call back with a view of bytes [2, 2+len); erase the
frame (<code>pending_.erase(0, 2 + len)</code> — O(n), fine here; the
capstone does better). The byte-by-byte test punishes any shortcut.
</details>

## Further reading

- [Nasdaq TotalView-ITCH 5.0 spec](https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/NQTVITCHSpecification.pdf)
  — skim §3-4; you'll implement its shape in m11a01.
- [Abseil Tip #1 revisited](https://abseil.io/tips/1) — string_view
  lifetimes; §4's contract is this tip in production.
- Trevor Jim / Robert Graham on protocol-parser CVEs — search
  "heartbleed bounds check"; a03's ByteReader is the vaccine.
- C++23 [std::byteswap](https://en.cppreference.com/w/cpp/numeric/byteswap),
  [std::start_lifetime_as](https://en.cppreference.com/w/cpp/memory/start_lifetime_as)
  — where the language is taking §2.
