#include "course/dyn_array.hpp"

namespace course {

DynArray::DynArray(std::size_t n) {
    // TODO: n zero-initialized doubles.
    (void)n;
}

DynArray::DynArray(std::initializer_list<double> xs) {
    // TODO (Hint 3).
    (void)xs;
}

DynArray::DynArray(const DynArray& other) {
    // TODO: deep copy.
    (void)other;
}

DynArray& DynArray::operator=(const DynArray& other) {
    // TODO (Hint 2 — the treacherous one).
    (void)other;
    return *this;
}

DynArray::DynArray(DynArray&& other) noexcept {
    // TODO: steal and null out.
    (void)other;
}

DynArray& DynArray::operator=(DynArray&& other) noexcept {
    // TODO: free own buffer, steal other's; self-move must be safe.
    (void)other;
    return *this;
}

DynArray::~DynArray() {
    // TODO
}

}  // namespace course
