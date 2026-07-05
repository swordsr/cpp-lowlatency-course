#include "course/sockets.hpp"

#include <unistd.h>

namespace course {

// --- OwnedFd (given) ---------------------------------------------------------

OwnedFd& OwnedFd::operator=(OwnedFd&& o) noexcept {
    if (this != &o) {
        if (fd_ >= 0) ::close(fd_);
        fd_ = std::exchange(o.fd_, -1);
    }
    return *this;
}

OwnedFd::~OwnedFd() {
    if (fd_ >= 0) ::close(fd_);
}

// --- byte order ----------------------------------------------------------------

std::uint16_t to_be16(std::uint16_t host) {
    // TODO (shifts only — express "most significant byte first").
    (void)host;
    return 0;
}

std::uint16_t from_be16(std::uint16_t wire) {
    // TODO
    (void)wire;
    return 0;
}

std::uint32_t to_be32(std::uint32_t host) {
    // TODO
    (void)host;
    return 0;
}

std::uint32_t from_be32(std::uint32_t wire) {
    // TODO
    (void)wire;
    return 0;
}

// --- TcpListener -----------------------------------------------------------------

std::optional<TcpListener> TcpListener::bind_any(std::uint16_t port) {
    // TODO: socket(AF_INET, SOCK_STREAM, 0), SO_REUSEADDR, bind to
    // 127.0.0.1:port (Hint 1), listen(., 16).
    (void)port;
    return std::nullopt;
}

std::uint16_t TcpListener::local_port() const {
    // TODO: getsockname (Hint 1).
    return 0;
}

std::optional<TcpStream> TcpListener::accept_one() {
    // TODO: accept(2); remember — it returns a NEW fd.
    return std::nullopt;
}

// --- TcpStream ---------------------------------------------------------------------

std::optional<TcpStream> TcpStream::connect_local(std::uint16_t port) {
    // TODO: socket + connect to 127.0.0.1:port.
    (void)port;
    return std::nullopt;
}

bool TcpStream::send_all(std::string_view data) {
    // TODO (Hint 2).
    (void)data;
    return false;
}

std::optional<std::size_t> TcpStream::recv_some(char* buffer,
                                                std::size_t max) {
    // TODO: one recv(2): n<0 -> nullopt, else n (0 == EOF).
    (void)buffer;
    (void)max;
    return std::nullopt;
}

bool TcpStream::set_nodelay() {
    // TODO (Hint 3).
    return false;
}

bool TcpStream::nodelay_enabled() const {
    // TODO: getsockopt.
    return false;
}

// --- UdpSocket ----------------------------------------------------------------------

std::optional<UdpSocket> UdpSocket::bind_any(std::uint16_t port) {
    // TODO: SOCK_DGRAM this time; bind exactly as in Hint 1.
    (void)port;
    return std::nullopt;
}

std::uint16_t UdpSocket::local_port() const {
    // TODO
    return 0;
}

bool UdpSocket::send_to_local(std::uint16_t port, std::string_view data) {
    // TODO: sendto(2) with the destination sockaddr_in.
    (void)port;
    (void)data;
    return false;
}

std::optional<std::string> UdpSocket::recv_one(std::size_t max) {
    // TODO: recvfrom(2) into a string sized to what actually arrived.
    (void)max;
    return std::nullopt;
}

}  // namespace course
