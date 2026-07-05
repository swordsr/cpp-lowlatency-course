#pragma once

#include <cstddef>
#include <new>
#include <utility>

namespace course {

/// Vector with inline storage for the first N elements (SBO). Spills to
/// the heap at N+1 and stays there. Copy/move deleted by design — the
/// storage duality is this assignment's lesson; you built the rest in
/// m02a04.
template <typename T, std::size_t N>
class SmallVector {
    static_assert(N > 0, "inline capacity must be at least 1");

public:
    SmallVector() = default;

    ~SmallVector() {
        // TODO: destroy elements; free the heap buffer iff spilled.
    }

    SmallVector(const SmallVector&) = delete;
    SmallVector& operator=(const SmallVector&) = delete;
    SmallVector(SmallVector&&) = delete;
    SmallVector& operator=(SmallVector&&) = delete;

    void push_back(const T& value) {
        // TODO
        (void)value;
    }

    void push_back(T&& value) {
        // TODO
        (void)value;
    }

    template <typename... Args>
    T& emplace_back(Args&&... args) {
        // TODO (spill recipe: Hint 2)
        ((void)args, ...);
        return *data();  // placeholder — UB until implemented
    }

    void pop_back() {
        // TODO
    }

    void clear() {
        // TODO: destroy all elements; stay in the current storage mode.
    }

    /// True while elements live in the inline buffer (Hint 1).
    bool is_inline() const {
        // TODO
        return true;
    }

    std::size_t size() const { return size_; }

    std::size_t capacity() const {
        // TODO: N while inline, the heap capacity after the spill.
        return N;
    }

    T* data() {
        // TODO (Hint 1 — the single source of truth).
        return reinterpret_cast<T*>(inline_storage_);
    }
    const T* data() const {
        // TODO
        return reinterpret_cast<const T*>(inline_storage_);
    }

    T& operator[](std::size_t i) { return data()[i]; }
    const T& operator[](std::size_t i) const { return data()[i]; }
    T* begin() { return data(); }
    T* end() { return data() + size_; }
    const T* begin() const { return data(); }
    const T* end() const { return data() + size_; }

private:
    alignas(T) unsigned char inline_storage_[N * sizeof(T)];
    T* heap_ = nullptr;  // nullptr <=> inline mode
    std::size_t size_ = 0;
    std::size_t heap_capacity_ = 0;
};

}  // namespace course
