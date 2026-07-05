#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace course {

// --- byte order (implement with shifts; no htons, no memcpy) ---------------

std::uint16_t to_be16(std::uint16_t host);    // host -> big-endian bytes
std::uint16_t from_be16(std::uint16_t wire);  // big-endian bytes -> host
std::uint32_t to_be32(std::uint32_t host);
std::uint32_t from_be32(std::uint32_t wire);

// --- RAII fd (GIVEN — m01a03, graduated) ------------------------------------

/// Move-only owner of a file descriptor. -1 == empty.
class OwnedFd {
public:
    OwnedFd() = default;
    explicit OwnedFd(int fd) : fd_(fd) {}
    OwnedFd(OwnedFd&& o) noexcept : fd_(std::exchange(o.fd_, -1)) {}
    OwnedFd& operator=(OwnedFd&& o) noexcept;
    ~OwnedFd();
    OwnedFd(const OwnedFd&) = delete;
    OwnedFd& operator=(const OwnedFd&) = delete;

    int get() const { return fd_; }
    bool valid() const { return fd_ >= 0; }

private:
    int fd_ = -1;
};

// --- your part ----------------------------------------------------------------

class TcpStream;

/// Listening TCP socket on 127.0.0.1. SO_REUSEADDR always set.
class TcpListener {
public:
    /// port 0 => kernel-assigned ephemeral port (the tests rely on it).
    static std::optional<TcpListener> bind_any(std::uint16_t port);

    /// The actually-bound port (via getsockname).
    std::uint16_t local_port() const;

    /// Blocks for one client; nullopt on error.
    std::optional<TcpStream> accept_one();

private:
    explicit TcpListener(OwnedFd fd) : fd_(std::move(fd)) {}
    OwnedFd fd_;
};

/// One TCP conversation.
class TcpStream {
public:
    static std::optional<TcpStream> connect_local(std::uint16_t port);

    /// Sends EVERY byte or reports failure (Hint 2 — partial sends).
    bool send_all(std::string_view data);

    /// Up to `max` bytes into `buffer`. nullopt = error; 0 = orderly EOF.
    std::optional<std::size_t> recv_some(char* buffer, std::size_t max);

    /// TCP_NODELAY on. Returns success.
    bool set_nodelay();

    /// Reads the option back via getsockopt (the test's referee).
    bool nodelay_enabled() const;

    /// For accept_one's use and the tests' plumbing.
    explicit TcpStream(OwnedFd fd) : fd_(std::move(fd)) {}

private:
    OwnedFd fd_;
};

/// Connectionless UDP endpoint on 127.0.0.1.
class UdpSocket {
public:
    static std::optional<UdpSocket> bind_any(std::uint16_t port);
    std::uint16_t local_port() const;

    bool send_to_local(std::uint16_t port, std::string_view data);

    /// Blocks for ONE datagram (boundaries preserved); nullopt on error.
    std::optional<std::string> recv_one(std::size_t max = 65536);

private:
    explicit UdpSocket(OwnedFd fd) : fd_(std::move(fd)) {}
    OwnedFd fd_;
};

}  // namespace course
