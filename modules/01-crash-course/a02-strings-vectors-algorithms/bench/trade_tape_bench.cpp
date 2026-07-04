// Your zero-copy split() vs a deliberately naive copying tokenizer.
// Build the release preset, then:
//   ./build/release/modules/01-crash-course/a02-strings-vectors-algorithms/m01a02_bench
#include "course/trade_tape.hpp"

#include <benchmark/benchmark.h>

#include <string>
#include <vector>

namespace {

// The baseline every naive parser starts as: one std::string PER FIELD.
std::vector<std::string> split_copying(const std::string& text, char sep) {
    std::vector<std::string> out;
    std::string current;
    for (char c : text) {
        if (c == sep) {
            out.push_back(current);  // copy
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    out.push_back(current);
    return out;
}

std::string make_tape_line() {
    // Long symbol defeats SSO so the copying cost is honest.
    return "1700000000123456789,ALPHABETSOUPCORP_XNAS,1551000,100,B";
}

void BM_SplitZeroCopy(benchmark::State& state) {
    const std::string line = make_tape_line();
    for (auto _ : state) {
        auto parts = course::split(line, ',');
        benchmark::DoNotOptimize(parts);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(line.size()));
}
BENCHMARK(BM_SplitZeroCopy);

void BM_SplitCopying(benchmark::State& state) {
    const std::string line = make_tape_line();
    for (auto _ : state) {
        auto parts = split_copying(line, ',');
        benchmark::DoNotOptimize(parts);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(line.size()));
}
BENCHMARK(BM_SplitCopying);

void BM_ParseTape1kLines(benchmark::State& state) {
    std::string tape;
    for (int i = 0; i < 1000; ++i) tape += make_tape_line() + "\n";
    for (auto _ : state) {
        auto trades = course::parse_tape(tape);
        benchmark::DoNotOptimize(trades);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(tape.size()));
}
BENCHMARK(BM_ParseTape1kLines);

}  // namespace
