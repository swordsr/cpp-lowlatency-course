#pragma once

#include <cstddef>
#include <utility>

namespace course {

/// Default deletion policy for UniquePtr.
template <typename T>
struct DefaultDelete {
    void operator()(T* p) const {
        // TODO: dispose of p the way new-allocated objects are disposed of.
        (void)p;
    }
};

/// Sole-ownership smart pointer: this course's std::unique_ptr.
/// Move-only. Moved-from state: empty (get() == nullptr).
/// Layout requirement: with an empty deleter, same size as a raw pointer.
template <typename T, typename D = DefaultDelete<T>>
class UniquePtr {
public:
    UniquePtr() = default;

    explicit UniquePtr(T* p) {
        // TODO: take ownership of p.
        (void)p;
    }

    UniquePtr(T* p, D d) {
        // TODO: take ownership of p, with deleter d.
        (void)p;
        (void)d;
    }

    UniquePtr(const UniquePtr&) = delete;
    UniquePtr& operator=(const UniquePtr&) = delete;

    UniquePtr(UniquePtr&& other) noexcept {
        // TODO: steal; leave other empty.
        (void)other;
    }

    UniquePtr& operator=(UniquePtr&& other) noexcept {
        // TODO: dispose of current pointee, steal other's. Self-move safe.
        (void)other;
        return *this;
    }

    ~UniquePtr() {
        // TODO: dispose of the pointee (if any) via the deleter.
    }

    /// Disowns and returns the raw pointer; no destruction happens.
    T* release() {
        // TODO
        return nullptr;
    }

    /// Destroys the current pointee (if any), then owns p.
    void reset(T* p = nullptr) {
        // TODO
        (void)p;
    }

    void swap(UniquePtr& other) noexcept {
        // TODO
        (void)other;
    }

    // --- trivial accessors (provided) ---
    T* get() const { return ptr_; }
    T& operator*() const { return *ptr_; }
    T* operator->() const { return ptr_; }
    explicit operator bool() const { return ptr_ != nullptr; }

private:
    T* ptr_ = nullptr;
    D deleter_{};  // NOTE: as written this doubles sizeof(UniquePtr) — Hint 3.
};

/// Constructs a T from the given arguments and returns an owning UniquePtr.
template <typename T, typename... Args>
UniquePtr<T> MakeUnique([[maybe_unused]] Args&&... args) {
    // TODO (Hint 2).
    return UniquePtr<T>{};
}

}  // namespace course
