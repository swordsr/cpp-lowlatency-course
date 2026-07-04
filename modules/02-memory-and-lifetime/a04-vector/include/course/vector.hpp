#pragma once

#include <cstddef>
#include <new>
#include <utility>

namespace course {

/// A std::vector-alike built on raw storage + placement new.
/// Invariant: objects are alive exactly in [data_, data_ + size_);
/// [data_ + size_, data_ + capacity_) is raw storage.
/// Growth policy: max(1, 2 * capacity()). reserve(n) allocates exactly n.
template <typename T>
class Vector {
public:
    Vector() = default;

    Vector(const Vector& other) {
        // TODO: deep copy (size() elements, capacity may equal size()).
        (void)other;
    }

    Vector& operator=(const Vector& other) {
        // TODO: self-assignment safe.
        (void)other;
        return *this;
    }

    Vector(Vector&& other) noexcept {
        // TODO: steal all three members.
        (void)other;
    }

    Vector& operator=(Vector&& other) noexcept {
        // TODO: destroy own contents, steal other's. Self-move safe.
        (void)other;
        return *this;
    }

    ~Vector() {
        // TODO: destroy elements, then free storage (Hint 3).
    }

    void push_back(const T& value) {
        // TODO (Hint 2 for the aliasing case).
        (void)value;
    }

    void push_back(T&& value) {
        // TODO
        (void)value;
    }

    /// Constructs directly in place; no copies or moves of T.
    /// Returns a reference to the new element.
    template <typename... Args>
    T& emplace_back(Args&&... args) {
        // TODO
        ((void)args, ...);
        return *data_;  // placeholder — UB until implemented; tests crash here, that's your red bar
    }

    void pop_back() {
        // TODO: destroy the last element.
    }

    /// Destroys all elements; capacity is retained.
    void clear() {
        // TODO
    }

    /// Ensures capacity() >= n (allocates exactly n; never shrinks).
    void reserve(std::size_t n) {
        // TODO (Hint 1).
        (void)n;
    }

    void swap(Vector& other) noexcept {
        // TODO
        (void)other;
    }

    std::size_t size() const { return size_; }
    std::size_t capacity() const { return capacity_; }
    bool empty() const { return size_ == 0; }

    T& operator[](std::size_t i) { return data_[i]; }
    const T& operator[](std::size_t i) const { return data_[i]; }
    T& front() { return data_[0]; }
    T& back() { return data_[size_ - 1]; }

    T* begin() { return data_; }
    T* end() { return data_ + size_; }
    const T* begin() const { return data_; }
    const T* end() const { return data_ + size_; }
    T* data() { return data_; }
    const T* data() const { return data_; }

private:
    // TODO: you'll want a private reallocate(new_cap) (Hint 1).

    T* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t capacity_ = 0;
};

}  // namespace course
