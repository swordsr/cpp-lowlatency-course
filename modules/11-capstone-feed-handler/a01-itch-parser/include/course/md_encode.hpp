#pragma once

// GIVEN, complete: the exchange's side of the CMD wire. Tests, the
// replayer tool, and your own experiments build datagrams with this —
// it is the executable spec your decoder is tested against. Read it;
// don't edit it.

#include <cstdint>
#include <string>
#include <string_view>

namespace course::md {

inline void put_u16(std::string& out, std::uint16_t v) {
    out.push_back(static_cast<char>(v & 0xFF));
    out.push_back(static_cast<char>((v >> 8) & 0xFF));
}
inline void put_u32(std::string& out, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
}
inline void put_u64(std::string& out, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) out.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
}

/// Builds one CMD datagram: header + framed messages.
class PacketBuilder {
public:
    explicit PacketBuilder(std::uint64_t first_seq) : first_seq_(first_seq) {}

    PacketBuilder& add(std::uint64_t order_id, char side, std::uint32_t qty,
                       std::int64_t price) {
        std::string p;
        p.push_back('A');
        put_u64(p, order_id);
        p.push_back(side);
        put_u32(p, qty);
        put_u64(p, static_cast<std::uint64_t>(price));
        frame(p);
        return *this;
    }

    PacketBuilder& execute(std::uint64_t order_id, std::uint32_t qty) {
        std::string p;
        p.push_back('E');
        put_u64(p, order_id);
        put_u32(p, qty);
        frame(p);
        return *this;
    }

    PacketBuilder& del(std::uint64_t order_id) {
        std::string p;
        p.push_back('D');
        put_u64(p, order_id);
        frame(p);
        return *this;
    }

    /// Appends raw bytes as one framed "message" — for malformed-input
    /// tests.
    PacketBuilder& raw_message(std::string_view payload) {
        std::string p{payload};
        frame(p);
        return *this;
    }

    /// The finished datagram. count == number of framed messages.
    std::string bytes() const {
        std::string out;
        put_u64(out, first_seq_);
        put_u16(out, count_);
        out += body_;
        return out;
    }

private:
    void frame(const std::string& payload) {
        put_u16(body_, static_cast<std::uint16_t>(payload.size()));
        body_ += payload;
        ++count_;
    }

    std::uint64_t first_seq_;
    std::uint16_t count_ = 0;
    std::string body_;
};

/// A heartbeat: count 0, carrying the sender's next sequence number.
inline std::string heartbeat(std::uint64_t next_seq) {
    return PacketBuilder{next_seq}.bytes();
}

}  // namespace course::md
