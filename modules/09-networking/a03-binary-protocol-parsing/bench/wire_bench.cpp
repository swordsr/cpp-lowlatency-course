// Release preset. Decode throughput over a 1M-message synthetic tape.
// Target on Apple Silicon: >= 300 MB/s through splitter+decode. If
// you're 10x under, something allocates per message. Meaningless until
// implemented (stub delivers nothing).
#include "course/wire.hpp"

#include <benchmark/benchmark.h>

#include <cstdint>
#include <string>

namespace {

void put_u16(std::string& out, std::uint16_t v) {
    out.push_back(static_cast<char>(v & 0xFF));
    out.push_back(static_cast<char>((v >> 8) & 0xFF));
}
void put_u32(std::string& out, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
}
void put_u64(std::string& out, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) out.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
}

std::string make_tape(int messages) {
    std::string tape;
    tape.reserve(static_cast<std::size_t>(messages) * 24);
    for (int i = 0; i < messages; ++i) {
        std::string p;
        switch (i % 3) {
            case 0:
                p.push_back('A');
                put_u64(p, static_cast<std::uint64_t>(i));
                put_u64(p, static_cast<std::uint64_t>(i) * 100);
                put_u32(p, 100);
                p.push_back('B');
                break;
            case 1:
                p.push_back('E');
                put_u64(p, static_cast<std::uint64_t>(i));
                put_u32(p, 10);
                break;
            default:
                p.push_back('X');
                put_u64(p, static_cast<std::uint64_t>(i));
                break;
        }
        put_u16(tape, static_cast<std::uint16_t>(p.size()));
        tape += p;
    }
    return tape;
}

void BM_SplitAndDecodeTape(benchmark::State& state) {
    const std::string tape = make_tape(1'000'000);
    std::int64_t decoded = 0;
    for (auto _ : state) {
        course::FrameSplitter splitter;
        splitter.feed(tape, [&](std::string_view payload) {
            if (auto msg = course::decode(payload)) ++decoded;
        });
        benchmark::DoNotOptimize(decoded);
    }
    if (decoded == 0) {
        state.SkipWithError("splitter/decode not implemented yet");
        return;
    }
    state.SetBytesProcessed(state.iterations() *
                            static_cast<std::int64_t>(tape.size()));
    state.SetItemsProcessed(state.iterations() * 1'000'000);
}
BENCHMARK(BM_SplitAndDecodeTape)->Unit(benchmark::kMillisecond);

}  // namespace
