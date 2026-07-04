#include "course/raw.hpp"

namespace course {

std::int64_t sum(const int* first, const int* last) {
    // TODO: pointer arithmetic warm-up.
    (void)first;
    (void)last;
    return 0;
}

RawBuffer::RawBuffer(std::size_t n) {
    // TODO: allocate n zero-filled bytes (remember: new char[n]() zeroes).
    (void)n;
}

RawBuffer::~RawBuffer() {
    // TODO
}

void RawBuffer::resize(std::size_t new_n) {
    // TODO (Hint 1 — order of operations).
    (void)new_n;
}

}  // namespace course
