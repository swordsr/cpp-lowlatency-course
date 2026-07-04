#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace course {

/// RAII wrapper over a read-only POSIX file descriptor. Move-only:
/// exactly one PosixFile owns a given fd at any time.
class PosixFile {
public:
    /// Opens `path` read-only; throws std::system_error (generic_category,
    /// current errno) on failure.
    explicit PosixFile(const std::string& path);

    PosixFile(const PosixFile&) = delete;
    PosixFile& operator=(const PosixFile&) = delete;

    /// Transfers ownership; `other` is left empty (fd() == -1).
    PosixFile(PosixFile&& other) noexcept;

    /// Closes the currently held fd (if any), then takes other's.
    /// Self-assignment is safe.
    PosixFile& operator=(PosixFile&& other) noexcept;

    /// Closes the fd unless empty.
    ~PosixFile();

    /// Raw descriptor; -1 when empty (moved-from).
    int fd() const { return fd_; }

    /// Reads the entire remaining contents. Throws std::system_error on
    /// read failure.
    std::string read_all();

private:
    int fd_ = -1;
};

/// Writes the elapsed wall time (steady_clock, nanoseconds) to `out_ns`
/// when the scope ends — on every exit path. Writes nothing before that.
class ScopeTimer {
public:
    explicit ScopeTimer(std::int64_t& out_ns);
    ~ScopeTimer();

    ScopeTimer(const ScopeTimer&) = delete;
    ScopeTimer& operator=(const ScopeTimer&) = delete;

private:
    std::int64_t& out_ns_;
    std::chrono::steady_clock::time_point start_;
};

}  // namespace course
