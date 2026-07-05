#pragma once

#include <cstddef>
#include <new>
#include <utility>

namespace course {

/// Bump-pointer arena: one upfront block, O(1) aligned allocation, no
/// individual free — reset() reclaims everything at once (and runs NO
/// destructors; see README THEORY §2).
class Arena {
public:
    explicit Arena(std::size_t capacity_bytes)
        : buffer_(static_cast<unsigned char*>(::operator new(capacity_bytes))),
          capacity_(capacity_bytes) {}

    ~Arena() { ::operator delete(buffer_); }

    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;
    Arena(Arena&&) = delete;
    Arena& operator=(Arena&&) = delete;

    /// Aligned bump allocation; nullptr when the request doesn't fit.
    /// `align` is a power of two.
    void* allocate(std::size_t size, std::size_t align) {
        // TODO (Hints 1 and 2).
        (void)size;
        (void)align;
        return nullptr;
    }

    /// Allocate + construct a T in the arena; nullptr if out of space.
    /// The arena never destroys what you create (THEORY §2).
    template <typename T, typename... Args>
    T* create(Args&&... args) {
        // TODO: allocate(sizeof(T), alignof(T)), then placement-new,
        // forwarding args (your m02a04 muscles).
        ((void)args, ...);
        return nullptr;
    }

    /// Rewind to empty. No destructors run.
    void reset() {
        // TODO
    }

    std::size_t used() const {
        // TODO
        return 0;
    }

    std::size_t capacity() const { return capacity_; }

    /// Is p inside this arena's buffer?
    bool owns(const void* p) const {
        // TODO
        (void)p;
        return false;
    }

private:
    unsigned char* buffer_ = nullptr;
    std::size_t capacity_ = 0;
    std::size_t offset_ = 0;
};

/// Minimal C++17 allocator adapter over an Arena (README THEORY §4).
/// deallocate is a no-op; exhaustion throws std::bad_alloc.
template <typename T>
class ArenaAllocator {
public:
    using value_type = T;

    explicit ArenaAllocator(Arena& arena) : arena_(&arena) {}

    /// Rebinding constructor: lets the container convert
    /// ArenaAllocator<T> into ArenaAllocator<Node> (Hint 3).
    template <typename U>
    ArenaAllocator(const ArenaAllocator<U>& other) : arena_(other.arena()) {}

    /// n OBJECTS (not bytes). Throws std::bad_alloc when the arena is out.
    T* allocate(std::size_t n) {
        // TODO
        (void)n;
        throw std::bad_alloc{};
    }

    /// Arenas don't free individually — deliberately empty.
    void deallocate(T* /*p*/, std::size_t /*n*/) {}

    /// Same arena <=> allocators can free each other's memory.
    bool operator==(const ArenaAllocator& rhs) const {
        // TODO
        (void)rhs;
        return false;
    }

    Arena* arena() const { return arena_; }

private:
    Arena* arena_;
};

}  // namespace course
