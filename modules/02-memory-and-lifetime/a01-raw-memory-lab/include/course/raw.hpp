#pragma once

#include <cstddef>
#include <cstdint>

namespace course {

/// Sum of the half-open range [first, last). Empty range -> 0.
std::int64_t sum(const int* first, const int* last);

/// Owns a zero-filled heap byte buffer. Deliberately primitive: this is
/// the last time you'll manage new[]/delete[] without move semantics.
class RawBuffer {
public:
    explicit RawBuffer(std::size_t n);
    ~RawBuffer();

    RawBuffer(const RawBuffer&) = delete;
    RawBuffer& operator=(const RawBuffer&) = delete;
    RawBuffer(RawBuffer&&) = delete;
    RawBuffer& operator=(RawBuffer&&) = delete;

    /// Reallocates to `new_n` zero-filled bytes, preserving the first
    /// min(size(), new_n) bytes.
    void resize(std::size_t new_n);

    char* data() { return data_; }
    const char* data() const { return data_; }
    std::size_t size() const { return size_; }

private:
    char* data_ = nullptr;
    std::size_t size_ = 0;
};

}  // namespace course
