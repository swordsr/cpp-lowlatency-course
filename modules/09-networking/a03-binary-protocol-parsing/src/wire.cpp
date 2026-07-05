#include "course/wire.hpp"

#include <cstring>

namespace course {

std::optional<Message> decode(std::string_view payload) {
    // TODO (Hint 2 — a ByteReader, a switch, and exact-length checks).
    (void)payload;
    return std::nullopt;
}

std::size_t FrameSplitter::feed(
    std::string_view chunk,
    const std::function<void(std::string_view)>& on_payload) {
    // TODO (Hint 3 — append, then loop while a full frame is pending).
    (void)chunk;
    (void)on_payload;
    return 0;
}

}  // namespace course
