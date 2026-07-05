#pragma once

#include <cstddef>
#include <new>
#include <utility>

namespace course {

/// Fixed-capacity object pool with an intrusive LIFO free list threaded
/// through the free slots themselves (README THEORY §2).
///
/// Preconditions the pool does NOT check (documented, caller-owned):
/// release() takes only live pointers from this pool, once; all objects
/// are released before the pool is destroyed.
template <typename T>
class ObjectPool {
public:
    explicit ObjectPool(std::size_t capacity)
        : slots_(new Slot[capacity]), capacity_(capacity) {
        // TODO: thread every slot onto the free list (Hint 1).
    }

    ~ObjectPool() { delete[] slots_; }

    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;
    ObjectPool(ObjectPool&&) = delete;
    ObjectPool& operator=(ObjectPool&&) = delete;

    /// O(1): pop the most recently released slot (LIFO), construct a T in
    /// it. nullptr when exhausted.
    template <typename... Args>
    T* acquire(Args&&... args) {
        // TODO (Hint 2).
        ((void)args, ...);
        return nullptr;
    }

    /// O(1): destroy *p, push its slot onto the free list.
    void release(T* p) {
        // TODO (Hints 2 and 3 — the ORDER inside matters).
        (void)p;
    }

    std::size_t in_use() const {
        // TODO
        return 0;
    }

    std::size_t capacity() const { return capacity_; }

    /// Is p inside this pool's slot array?
    bool owns(const void* p) const {
        // TODO
        (void)p;
        return false;
    }

private:
    /// Two lifetimes, one storage: a slot is EITHER a free-list link OR a
    /// live T — never both (THEORY §2).
    union Slot {
        Slot() {}   // no member starts alive;
        ~Slot() {}  // the pool manages lifetimes manually
        Slot* next_free;
        alignas(T) unsigned char storage[sizeof(T)];
    };

    Slot* slots_ = nullptr;
    Slot* free_head_ = nullptr;
    std::size_t capacity_ = 0;
    std::size_t in_use_ = 0;
};

}  // namespace course
