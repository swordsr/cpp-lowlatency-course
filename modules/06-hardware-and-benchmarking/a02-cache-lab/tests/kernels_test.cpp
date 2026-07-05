// The spec for m06a02. Deliberately small and purely functional — the
// kernels' POINT is their memory behavior, which the benchmark exposes.
// Here we only pin that they compute the right sums.
#include "course/kernels.hpp"

#include <gtest/gtest.h>

#include <numeric>
#include <vector>

namespace {

using course::chase;
using course::Node;
using course::sum_gather;
using course::sum_sequential;
using course::sum_strided;

TEST(Kernels, SequentialSums) {
    const std::vector<std::int64_t> data = {1, 2, 3, 4, 5};
    EXPECT_EQ(sum_sequential(data.data(), data.size()), 15);
    EXPECT_EQ(sum_sequential(data.data(), 0), 0);
}

TEST(Kernels, StridedTakesEveryKth) {
    const std::vector<std::int64_t> data = {1, 2, 3, 4, 5, 6, 7};
    EXPECT_EQ(sum_strided(data.data(), data.size(), 1), 28);
    EXPECT_EQ(sum_strided(data.data(), data.size(), 2), 16);  // 1+3+5+7
    EXPECT_EQ(sum_strided(data.data(), data.size(), 3), 12);  // 1+4+7
    EXPECT_EQ(sum_strided(data.data(), data.size(), 10), 1);  // just [0]
    EXPECT_EQ(sum_strided(data.data(), 0, 2), 0);
}

TEST(Kernels, GatherFollowsTheIndexArray) {
    const std::vector<std::int64_t> data = {10, 20, 30, 40};
    const std::vector<std::uint32_t> idx = {3, 3, 0, 2};
    EXPECT_EQ(sum_gather(data.data(), idx.data(), idx.size()), 110);
    EXPECT_EQ(sum_gather(data.data(), idx.data(), 0), 0);
}

TEST(Kernels, ChaseWalksToNull) {
    std::vector<Node> nodes(4);
    // Chain: 0 -> 2 -> 1 -> 3 -> null, values 5, 7, 6, 8.
    nodes[0] = {5, &nodes[2]};
    nodes[2] = {7, &nodes[1]};
    nodes[1] = {6, &nodes[3]};
    nodes[3] = {8, nullptr};
    EXPECT_EQ(chase(&nodes[0]), 26);
    EXPECT_EQ(chase(nullptr), 0);
}

TEST(Kernels, AllAgreeOnTheSameData) {
    std::vector<std::int64_t> data(1'000);
    std::iota(data.begin(), data.end(), 1);
    const std::int64_t expected = 500'500;

    EXPECT_EQ(sum_sequential(data.data(), data.size()), expected);
    EXPECT_EQ(sum_strided(data.data(), data.size(), 1), expected);

    std::vector<std::uint32_t> idx(data.size());
    std::iota(idx.begin(), idx.end(), 0);
    EXPECT_EQ(sum_gather(data.data(), idx.data(), idx.size()), expected);
}

}  // namespace
