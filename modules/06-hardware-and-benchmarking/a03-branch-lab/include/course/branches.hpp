#pragma once

#include <cstddef>
#include <cstdint>

namespace course {

/// How many elements exceed threshold — with a literal `if`.
std::int64_t count_above_branchy(const std::int64_t* data, std::size_t n,
                                 std::int64_t threshold);

/// Same result, no conditional control flow in the loop body (Hint 1).
std::int64_t count_above_branchless(const std::int64_t* data, std::size_t n,
                                    std::int64_t threshold);

/// Sum of elements exceeding threshold — with a literal `if`.
std::int64_t sum_above_branchy(const std::int64_t* data, std::size_t n,
                               std::int64_t threshold);

/// Same result, compare+select instead of a branch (Hint 2).
std::int64_t sum_above_branchless(const std::int64_t* data, std::size_t n,
                                  std::int64_t threshold);

}  // namespace course
