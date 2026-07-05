#include "course/branches.hpp"

// After the benchmark run: record your 2x2 matrix (branchy/branchless x
// sorted/random, ns/elem) here, and note whether the compiler had already
// converted your branchy count_above to csel (check the assembly!).

namespace course {

std::int64_t count_above_branchy(const std::int64_t* data, std::size_t n,
                                 std::int64_t threshold) {
    // TODO
    (void)data;
    (void)n;
    (void)threshold;
    return 0;
}

std::int64_t count_above_branchless(const std::int64_t* data, std::size_t n,
                                    std::int64_t threshold) {
    // TODO
    (void)data;
    (void)n;
    (void)threshold;
    return 0;
}

std::int64_t sum_above_branchy(const std::int64_t* data, std::size_t n,
                               std::int64_t threshold) {
    // TODO
    (void)data;
    (void)n;
    (void)threshold;
    return 0;
}

std::int64_t sum_above_branchless(const std::int64_t* data, std::size_t n,
                                  std::int64_t threshold) {
    // TODO
    (void)data;
    (void)n;
    (void)threshold;
    return 0;
}

}  // namespace course
