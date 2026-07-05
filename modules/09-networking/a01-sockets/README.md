# a01 — Sockets: RAII Around File Descriptors That Talk

**Estimated effort:** 6–8 hours.

## Learning objectives

- Drive the BSD sockets API directly: socket/bind/listen/accept/connect,
  send/recv, sendto/recvfrom — the layer every networking library wraps.
- Wrap it all in the move-only RAII style you built in m01a03.
- Implement byte-order conversion by hand and know when it applies.
- Set the socket options that matter in trading (TCP_NODELAY,
  SO_REUSEADDR) and explain each to an interviewer.

## THEORY

### 1. A socket is a file descriptor with opinions

Coming from Java NIO: `SocketChannel`/`DatagramChannel` are thin paint
over exactly this API. A socket *is* an fd — `close(2)` closes it,
ownership rules from m01a03 apply verbatim (double-close a socket fd and
you might close someone's database connection — fds are recycled
integers). Hence the same medicine: move-only RAII wrappers, one owner
per fd, ever.

The TCP server dance and its client counterpart:

```text
server: socket() → bind(addr) → listen() → accept() ⇒ NEW fd per client
client: socket() → connect(addr)         ⇒ the same fd converses
UDP:    socket() → bind(addr) → sendto()/recvfrom()   (no connection at all)
```

`accept` returning a *fresh* fd is the detail people miss: the listener
fd is a factory, never a conversation. UDP has no factory and no
conversation — each `recvfrom` is one datagram from whoever, boundaries
preserved (TCP is a byte stream: boundaries are YOUR problem — a03).

### 2. Byte order: the wire has one, your CPU has another

Multi-byte integers on the wire have a declared byte order. Network
protocol headers are big-endian ("network order"); your ARM64 and every
x86 are little-endian. `htons`/`ntohl` exist, but you'll implement the
conversions with shifts — because the shift idiom is endian-independent
(it expresses "most significant byte first" without caring what the CPU
is) and because interviewers ask. Reality check for later: most modern
exchange feeds (ITCH included) chose little-endian — "network order" is
convention, not law; read the spec of the protocol in front of you.

### 3. The options that pay rent

- **`TCP_NODELAY`** — disables Nagle (module README bank). The order-
  entry socket option. Your `TcpStream` exposes it; a test verifies via
  `getsockopt` that you actually set it.
- **`SO_REUSEADDR`** — lets a restarted server rebind its port while old
  connections linger in TIME_WAIT. Without it, every restart of your
  gateway waits ~60s. Set on listeners, always (given in the skeleton's
  hint — you write the `setsockopt` call).
- Ephemeral ports: `bind` to port 0 and the kernel picks a free port —
  `getsockname` reads it back. The tests use this everywhere so they
  never collide with anything on your machine.

### 4. Partial I/O: the contract nobody reads

`send` may write FEWER bytes than asked (full socket buffer); `recv`
returns whatever's available, 0 on orderly EOF, -1 with errno on error.
Every real networking bug audit finds a naked `send(fd, buf, n)` whose
return value was ignored. Your `send_all` loops until done — the
five-line function that separates people who've done this from people
who've read about it.

## Assignment spec

`include/course/sockets.hpp` + `src/sockets.cpp`. The RAII fd plumbing
(`OwnedFd`, move-only, closes on destruction) is **given** — you earned
it in m01a03. You implement, all over 127.0.0.1:

- Byte order: `to_be16/from_be16`, `to_be32/from_be32` — shifts, no
  `htons`, no `memcpy`, no UB.
- `TcpListener` — `bind_any(port)` (0 ⇒ ephemeral; SO_REUSEADDR on),
  `local_port()`, `accept_one()` → `std::optional<TcpStream>`.
- `TcpStream` — `connect_local(port)`, `send_all(string_view)`,
  `recv_some(buffer, max)` → `optional<size_t>` (nullopt = error,
  0 = EOF), `set_nodelay()`, `nodelay_enabled()`.
- `UdpSocket` — `bind_any(port)`, `local_port()`,
  `send_to_local(port, string_view)`, `recv_one(max)` →
  `optional<std::string>` (one datagram, boundaries preserved).

All factories return `std::optional` — a socket that failed to be born
is not an object (compare m01a03's throwing constructor: both idioms,
you've now used each and can argue the trade).

**Acceptance criteria:** all `m09a01.*` tests pass in `debug` and
`asan`. The tests run real loopback traffic — server threads accept and
echo using YOUR listener while YOUR client talks to it.

## Hints

<details><summary>Hint 1 — the sockaddr_in incantation</summary>
<pre>sockaddr_in addr{};
addr.sin_family = AF_INET;
addr.sin_port = htons(port);          // fine to use htons HERE
addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);</pre>
bind/connect take <code>(const sockaddr*)&addr, sizeof addr</code> —
yes, that cast is how this 1983 API does polymorphism. getsockname
fills the same struct; read the port back with ntohs.
</details>

<details><summary>Hint 2 — send_all</summary>
Loop: <code>n = ::send(fd, p, remaining, 0)</code>; n &lt; 0 → false;
advance p by n, shrink remaining; done when 0 remains. (On Linux you'd
pass MSG_NOSIGNAL to survive dead peers; macOS spells that
SO_NOSIGPIPE at setup — set it in the skeleton's given helper.)
</details>

<details><summary>Hint 3 — the NoDelay test fails</summary>
setsockopt wants IPPROTO_TCP (not SOL_SOCKET) for TCP_NODELAY, an
<code>int one = 1</code>, and <code>sizeof(int)</code>. The test reads
it back with getsockopt — no faking it with a bool member.
</details>

## Further reading

- Beej's Guide to Network Programming — the classic free intro; §5–6
  cover this assignment.
- Stevens, *UNIX Network Programming* Vol 1 — THE reference; ch. 4–8.
- [The C10K problem](http://www.kegel.com/c10k.html) (Kegel) —
  historical context that motivates a02.
- `man 2 socket`, `man 7 tcp`, `man 7 udp` — read them once for real.
