# a03 — RAII First Contact: A File Wrapper and a Scope Timer

**Estimated effort:** 3–5 hours.

## Learning objectives

- Explain RAII precisely: resource lifetime bound to object lifetime,
  cleanup in the destructor, enforced by the language at every exit path.
- Wrap a raw OS resource (a POSIX file descriptor) in a move-only RAII type.
- Choose between the three C++ error-reporting idioms and defend the choice.
- Meet move semantics informally (Module 2 formalizes it).

## THEORY

### 1. Deterministic destruction — the big one

```cpp
{
    PosixFile f{"/etc/hosts"};   // constructor acquires: open(2)
    auto text = f.read_all();
    if (text.empty()) return;    // destructor runs here
    process(text);               // ...or if this throws, here
}                                // ...or here. ALWAYS. In that frame.
```

The destructor runs at scope exit — normal fall-through, `return`, `break`,
or an exception unwinding past the frame. In reverse declaration order. Not
"eventually, when the GC feels like it": *at that closing brace*. This is
**RAII** — Resource Acquisition Is Initialization — and it's the single most
important idiom in the language. `std::string` is RAII around a buffer;
`std::vector` around an array; `std::lock_guard` (Module 7) around a mutex;
your `PosixFile` around a file descriptor.

The Java comparison that makes it click: try-with-resources requires the
*call site* to opt in, every time, and only works for block scope. RAII puts
the knowledge in the *type*: members of a class, elements of a vector,
temporaries mid-expression — everything gets cleaned up, and no caller can
forget. This is also why C++ has no `finally`: a language with destructors
doesn't need one.

The discipline it demands: **one resource, one owner object**. The moment a
raw resource handle (an fd, a `new` pointer, a lock) escapes into code that
might return early or throw, you have a leak path. The fix is never "be
careful"; it's "wrap it".

### 2. Move-only types: ownership you can hand off but not duplicate

What would copying a `PosixFile` mean? Two objects, one fd, two destructors
→ double `close(2)` — the second close might slam a descriptor the OS
already reused for someone else's socket. Nasty, real, production bug.
So we *delete* copying:

```cpp
PosixFile(const PosixFile&) = delete;
PosixFile& operator=(const PosixFile&) = delete;
```

But we allow **moving** — transferring the fd to a new object and leaving
the source empty (`fd_ == -1`, destructor does nothing). One owner at all
times, but ownership can travel: return it from factories, store it in a
vector. `std::unique_ptr` is exactly this pattern for heap memory, and you
will implement it in Module 2. The core move-constructor idiom is
`std::exchange(other.fd_, -1)` — take the value, leave a benign one.

### 3. Three error idioms — and when each is right

| Idiom | Example | Right when |
|-------|---------|-----------|
| exceptions | constructor `throw`s `std::system_error` | failure is exceptional; caller can't reasonably handle it locally; **constructors** (they can't return an error) |
| `std::optional<T>` / (C++23 `std::expected`) | `parse_price` | absence/failure is a normal, expected outcome |
| error codes / errno | POSIX itself | C ABI boundaries; hot paths that can't afford unwinding tables |

This assignment uses the first two deliberately: `PosixFile`'s constructor
throws `std::system_error` (carrying `errno`, so the caller learns *why*),
while `ScopeTimer` can't fail. Low-latency footnote: many HFT shops build
with `-fno-exceptions` on hot paths, precisely because unwinding is slow and
unpredictable — but constructors + exceptions remain the general-purpose
C++ idiom, and interviews expect you to know both positions and articulate
the trade.

### 4. `ScopeTimer` — RAII where the "resource" is a measurement

RAII generalizes beyond OS handles: anything with paired begin/end fits.
A timer that records `steady_clock::now()` at construction and writes the
elapsed nanoseconds at destruction gives you timing that can't forget to
stop — every return path is measured. (`steady_clock`, not `system_clock`:
NTP can yank the wall clock backwards mid-measurement.) You'll grow this
toy into the latency histogram of Module 6, and it's the shape of every
scope-profiler annotation you'll meet in production code.

## Assignment spec

Declared in `include/course/raii.hpp`, implemented in `src/raii.cpp`.

`PosixFile` — RAII over a read-only file descriptor:

- `explicit PosixFile(const std::string& path)` — `open(2)` with
  `O_RDONLY`; on failure throw `std::system_error` from `errno` with the
  category `std::generic_category()`.
- Non-copyable, movable (move ctor + move assignment). Moved-from state:
  `fd() == -1`, destructor is a no-op, safe to destroy.
- `~PosixFile()` — `close(2)` unless fd is -1. Move assignment closes the
  currently-held fd first (self-assignment must be safe).
- `int fd() const` — the raw descriptor (-1 if empty).
- `std::string read_all()` — the entire remaining file contents via
  `read(2)` in a loop into a growing `std::string`. (Loop! `read` may
  return short counts.)

`ScopeTimer` — writes elapsed wall time on destruction:

- `explicit ScopeTimer(std::int64_t& out_ns)` — records start now.
- `~ScopeTimer()` — writes elapsed nanoseconds (`std::chrono::steady_clock`)
  to the referenced int64. Nothing is written before destruction.
- Non-copyable, non-movable (it holds a reference; simplest correct choice).

**Acceptance criteria:** all `m01a03.*` tests pass in `debug` and `asan`.
The fd tests verify actual OS behavior (`fcntl` reports `EBADF` after your
destructor runs) — leaks and double-closes fail loudly.

## Hints

<details><summary>Hint 1 — headers you need</summary>
<code>&lt;fcntl.h&gt;</code> for <code>open</code>,
<code>&lt;unistd.h&gt;</code> for <code>read</code>/<code>close</code>,
<code>&lt;cerrno&gt;</code>, <code>&lt;system_error&gt;</code>,
<code>&lt;utility&gt;</code> for <code>std::exchange</code>,
<code>&lt;chrono&gt;</code>.
</details>

<details><summary>Hint 2 — move assignment order of operations</summary>
Guard <code>this != &other</code>, close your own fd if valid, then
<code>fd_ = std::exchange(other.fd_, -1)</code>. Walk through what happens
without each step: skip the guard → self-move closes then keeps a dead fd;
skip the close → you leak the fd you were holding.
</details>

<details><summary>Hint 3 — read_all loop</summary>
Fixed <code>char buf[4096]</code>; loop <code>read(fd_, buf, sizeof buf)</code>;
<code>n &gt; 0</code> → <code>out.append(buf, n)</code>; <code>n == 0</code>
→ EOF, done; <code>n &lt; 0</code> → throw <code>std::system_error</code>
(same recipe as the constructor).
</details>

## Further reading

- *A Tour of C++* ch. 5.2 (essential operations), 6.3 (resource management).
- CppCon 2019, Arthur O'Dwyer, *"Back to Basics: RAII and the Rule of Zero"*
  — the best single hour on this topic; watch before Module 2.
- [std::system_error](https://en.cppreference.com/w/cpp/error/system_error)
  on cppreference — the errno-carrying exception you'll throw.
- Herb Sutter, [GotW #102: Exception-Safe Function Calls](https://herbsutter.com/gotw/_102/).
