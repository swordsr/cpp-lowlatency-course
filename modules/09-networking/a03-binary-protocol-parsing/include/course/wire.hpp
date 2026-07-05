#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace course {

/// Bounds-checked little-endian cursor over a byte view. Reads advance
/// on success; a failed read leaves the cursor where it was. Zero-copy:
/// the reader owns nothing (the view must outlive it).
class ByteReader {
public:
    explicit ByteReader(std::string_view bytes) : bytes_(bytes) {}

    std::size_t remaining() const {
        // TODO
        return 0;
    }

    std::optional<std::uint8_t> read_u8() {
        // TODO (Hint 1)
        return std::nullopt;
    }

    std::optional<std::uint16_t> read_u16() {
        // TODO — the memcpy idiom (THEORY §2)
        return std::nullopt;
    }

    std::optional<std::uint32_t> read_u32() {
        // TODO
        return std::nullopt;
    }

    std::optional<std::uint64_t> read_u64() {
        // TODO
        return std::nullopt;
    }

    std::optional<std::int64_t> read_i64() {
        // TODO
        return std::nullopt;
    }

private:
    std::string_view bytes_;
    std::size_t offset_ = 0;
};

// --- CTP messages (README THEORY §1) ----------------------------------------

struct AddOrder {
    std::uint64_t order_id;
    std::int64_t price_ticks;
    std::uint32_t qty;
    char side;  // 'B' or 'S'
};

struct Execute {
    std::uint64_t order_id;
    std::uint32_t exec_qty;
};

struct Cancel {
    std::uint64_t order_id;
};

using Message = std::variant<AddOrder, Execute, Cancel>;

/// Decodes one CTP payload (type byte + fields, EXACT length).
/// Unknown type, short payload, or trailing bytes => nullopt.
std::optional<Message> decode(std::string_view payload);

/// Reassembles u16-length-prefixed frames from arbitrary chunk
/// boundaries and invokes the callback once per complete payload.
/// Returns the number of payloads delivered from this feed() call.
/// The payload view is valid ONLY during the callback (THEORY §4).
class FrameSplitter {
public:
    std::size_t feed(std::string_view chunk,
                     const std::function<void(std::string_view)>& on_payload);

private:
    std::string pending_;
};

}  // namespace course
