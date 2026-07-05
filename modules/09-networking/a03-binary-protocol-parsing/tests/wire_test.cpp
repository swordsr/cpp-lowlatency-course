// The spec for m09a03. The encoder helpers below are the tests' own
// little CTP writer — they define the wire format byte for byte, so read
// them as part of the spec.
#include "course/wire.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace {

using namespace course;

// --- test-side encoders (fixture code, complete) -------------------------------

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

std::string encode_add(std::uint64_t id, std::int64_t price,
                       std::uint32_t qty, char side) {
    std::string p;
    p.push_back('A');
    put_u64(p, id);
    put_u64(p, static_cast<std::uint64_t>(price));
    put_u32(p, qty);
    p.push_back(side);
    return p;
}
std::string encode_execute(std::uint64_t id, std::uint32_t qty) {
    std::string p;
    p.push_back('E');
    put_u64(p, id);
    put_u32(p, qty);
    return p;
}
std::string encode_cancel(std::uint64_t id) {
    std::string p;
    p.push_back('X');
    put_u64(p, id);
    return p;
}
std::string frame(const std::string& payload) {
    std::string f;
    put_u16(f, static_cast<std::uint16_t>(payload.size()));
    f += payload;
    return f;
}

// --- ByteReader -----------------------------------------------------------------

TEST(ByteReaderTest, ReadsLittleEndian) {
    const std::string bytes{"\x34\x12\x78\x56\x34\x12", 6};
    ByteReader r{bytes};
    EXPECT_EQ(r.remaining(), 6u);
    EXPECT_EQ(r.read_u16(), 0x1234);
    EXPECT_EQ(r.read_u32(), 0x12345678u);
    EXPECT_EQ(r.remaining(), 0u);
}

TEST(ByteReaderTest, Reads64BitAndSigned) {
    std::string bytes;
    put_u64(bytes, 0x0102030405060708ULL);
    put_u64(bytes, static_cast<std::uint64_t>(std::int64_t{-1'551'000}));
    ByteReader r{bytes};
    EXPECT_EQ(r.read_u64(), 0x0102030405060708ULL);
    EXPECT_EQ(r.read_i64(), -1'551'000);
}

TEST(ByteReaderTest, MisalignedOffsetsAreFine) {
    // One u8 first pushes every later read off alignment — the memcpy
    // idiom must not care (asan/ubsan would catch a cast-based version).
    std::string bytes;
    bytes.push_back('\x01');
    put_u64(bytes, 42);
    ByteReader r{bytes};
    EXPECT_EQ(r.read_u8(), 1);
    EXPECT_EQ(r.read_u64(), 42u);
}

TEST(ByteReaderTest, TruncationIsNulloptAndCursorHolds) {
    const std::string bytes{"\x01\x02\x03", 3};
    ByteReader r{bytes};
    EXPECT_EQ(r.read_u32(), std::nullopt) << "3 bytes can't make a u32";
    EXPECT_EQ(r.remaining(), 3u) << "failed read must not consume";
    EXPECT_EQ(r.read_u16(), 0x0201) << "reader must still work after a miss";
    EXPECT_EQ(r.read_u16(), std::nullopt);
    EXPECT_EQ(r.read_u8(), 3);
    EXPECT_EQ(r.read_u8(), std::nullopt) << "empty";
}

// --- decode ---------------------------------------------------------------------

TEST(DecodeTest, AddOrder) {
    const auto msg = decode(encode_add(77, 1'551'000, 100, 'B'));
    ASSERT_TRUE(msg.has_value());
    const auto* add = std::get_if<AddOrder>(&*msg);
    ASSERT_NE(add, nullptr);
    EXPECT_EQ(add->order_id, 77u);
    EXPECT_EQ(add->price_ticks, 1'551'000);
    EXPECT_EQ(add->qty, 100u);
    EXPECT_EQ(add->side, 'B');
}

TEST(DecodeTest, NegativePricesSurvive) {
    const auto msg = decode(encode_add(1, -25, 5, 'S'));
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(std::get<AddOrder>(*msg).price_ticks, -25);
}

TEST(DecodeTest, ExecuteAndCancel) {
    const auto exec = decode(encode_execute(88, 40));
    ASSERT_TRUE(exec.has_value());
    EXPECT_EQ(std::get<Execute>(*exec).order_id, 88u);
    EXPECT_EQ(std::get<Execute>(*exec).exec_qty, 40u);

    const auto cxl = decode(encode_cancel(99));
    ASSERT_TRUE(cxl.has_value());
    EXPECT_EQ(std::get<Cancel>(*cxl).order_id, 99u);
}

TEST(DecodeTest, RejectsMalformedPayloads) {
    EXPECT_EQ(decode(""), std::nullopt) << "no type byte";
    EXPECT_EQ(decode("Q"), std::nullopt) << "unknown type";
    EXPECT_EQ(decode(encode_add(1, 2, 3, 'B').substr(0, 10)), std::nullopt)
        << "truncated AddOrder";
    EXPECT_EQ(decode(encode_cancel(1) + "extra"), std::nullopt)
        << "trailing bytes: exact length is the law (THEORY §1)";
}

// --- FrameSplitter -----------------------------------------------------------------

TEST(FrameSplitterTest, OneWholeFrame) {
    FrameSplitter splitter;
    std::vector<std::string> payloads;
    const auto n = splitter.feed(frame(encode_cancel(5)),
                                 [&](std::string_view p) {
                                     payloads.emplace_back(p);
                                 });
    EXPECT_EQ(n, 1u);
    ASSERT_EQ(payloads.size(), 1u);
    EXPECT_EQ(payloads[0], encode_cancel(5));
}

TEST(FrameSplitterTest, ThreeFramesInOneChunk) {
    FrameSplitter splitter;
    const std::string tape = frame(encode_add(1, 10, 1, 'B')) +
                             frame(encode_execute(1, 1)) +
                             frame(encode_cancel(1));
    std::vector<std::string> payloads;
    EXPECT_EQ(splitter.feed(tape,
                            [&](std::string_view p) {
                                payloads.emplace_back(p);
                            }),
              3u);
    ASSERT_EQ(payloads.size(), 3u);
    EXPECT_EQ(payloads[0][0], 'A');
    EXPECT_EQ(payloads[1][0], 'E');
    EXPECT_EQ(payloads[2][0], 'X');
}

TEST(FrameSplitterTest, ByteByByteFeedStillWorks) {
    // The production test: every possible split point at once.
    FrameSplitter splitter;
    const std::string tape = frame(encode_add(7, 700, 70, 'S')) +
                             frame(encode_cancel(7));
    std::vector<std::string> payloads;
    std::size_t delivered = 0;
    for (char c : tape) {
        delivered += splitter.feed(std::string_view{&c, 1},
                                   [&](std::string_view p) {
                                       payloads.emplace_back(p);
                                   });
    }
    EXPECT_EQ(delivered, 2u);
    ASSERT_EQ(payloads.size(), 2u);
    const auto msg = decode(payloads[0]);
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(std::get<AddOrder>(*msg).order_id, 7u);
}

TEST(FrameSplitterTest, SplitInsideTheLengthPrefix) {
    FrameSplitter splitter;
    const std::string f = frame(encode_execute(3, 30));
    std::vector<std::string> payloads;
    auto sink = [&](std::string_view p) { payloads.emplace_back(p); };

    EXPECT_EQ(splitter.feed(f.substr(0, 1), sink), 0u)
        << "half a length prefix is not a frame";
    EXPECT_EQ(splitter.feed(f.substr(1), sink), 1u);
    ASSERT_EQ(payloads.size(), 1u);
    EXPECT_EQ(payloads[0], encode_execute(3, 30));
}

TEST(FrameSplitterTest, ZeroLengthPayloadIsDelivered) {
    FrameSplitter splitter;
    std::string empty_frame;
    put_u16(empty_frame, 0);
    int calls = 0;
    EXPECT_EQ(splitter.feed(empty_frame,
                            [&](std::string_view p) {
                                ++calls;
                                EXPECT_TRUE(p.empty());
                            }),
              1u);
    EXPECT_EQ(calls, 1) << "deliver it; decode() is who rejects it";
}

TEST(FrameSplitterTest, EndToEndWithDecode) {
    FrameSplitter splitter;
    const std::string tape = frame(encode_add(11, 1'000, 10, 'B')) +
                             frame(encode_execute(11, 4)) +
                             frame(encode_cancel(11));
    int adds = 0, execs = 0, cancels = 0;
    splitter.feed(tape, [&](std::string_view p) {
        const auto msg = decode(p);
        ASSERT_TRUE(msg.has_value());
        if (std::holds_alternative<AddOrder>(*msg)) ++adds;
        if (std::holds_alternative<Execute>(*msg)) ++execs;
        if (std::holds_alternative<Cancel>(*msg)) ++cancels;
    });
    EXPECT_EQ(adds, 1);
    EXPECT_EQ(execs, 1);
    EXPECT_EQ(cancels, 1);
}

}  // namespace
