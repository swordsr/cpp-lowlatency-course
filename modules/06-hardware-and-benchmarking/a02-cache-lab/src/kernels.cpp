#include "course/kernels.hpp"

// After the benchmark run: paste your working-set sweep table here, and
// mark where you believe L1 and L2 end on your machine.

namespace course {

std::int64_t sum_sequential(const std::int64_t* data, std::size_t n) {
    // TODO
    (void)data;
    (void)n;
    return 0;
}

std::int64_t sum_strided(const std::int64_t* data, std::size_t n,
                         std::size_t stride) {
    // TODO
    (void)data;
    (void)n;
    (void)stride;
    return 0;
}

std::int64_t sum_gather(const std::int64_t* data, const std::uint32_t* idx,
                        std::size_t n) {
    // TODO
    (void)data;
    (void)idx;
    (void)n;
    return 0;
}

std::int64_t chase(const Node* head) {
    // TODO
    (void)head;
    return 0;
}

}  // namespace course
