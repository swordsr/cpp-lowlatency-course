#include "course/raii.hpp"

namespace course {

PosixFile::PosixFile(const std::string& path) {
    // TODO: open(2) O_RDONLY; throw std::system_error on failure (Hint 1).
    (void)path;
}

PosixFile::PosixFile(PosixFile&& other) noexcept {
    // TODO: take other's fd, leave other empty (Hint 2).
    (void)other;
}

PosixFile& PosixFile::operator=(PosixFile&& other) noexcept {
    // TODO (Hint 2 — order of operations matters).
    (void)other;
    return *this;
}

PosixFile::~PosixFile() {
    // TODO: close(2) unless fd_ == -1.
}

std::string PosixFile::read_all() {
    // TODO: read(2) loop (Hint 3).
    return {};
}

ScopeTimer::ScopeTimer(std::int64_t& out_ns) : out_ns_(out_ns) {
    // TODO: record the start time.
}

ScopeTimer::~ScopeTimer() {
    // TODO: write elapsed nanoseconds to out_ns_.
}

}  // namespace course
