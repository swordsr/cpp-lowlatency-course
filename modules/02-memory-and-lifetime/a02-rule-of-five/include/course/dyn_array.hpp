#pragma once

#include <cstddef>
#include <initializer_list>

namespace course {

/// Owning, fixed-length heap array of double. The Rule of Five, by hand.
/// Moved-from state: empty (size() == 0, data() == nullptr).
class DynArray {
public:
    DynArray() = default;
    explicit DynArray(std::size_t n);           // n zeros
    DynArray(std::initializer_list<double> xs);

    DynArray(const DynArray& other);            // deep copy
    DynArray& operator=(const DynArray& other); // self-assignment safe
    DynArray(DynArray&& other) noexcept;
    DynArray& operator=(DynArray&& other) noexcept;
    ~DynArray();

    std::size_t size() const { return size_; }
    double* data() { return data_; }
    const double* data() const { return data_; }

    double& operator[](std::size_t i) { return data_[i]; }
    const double& operator[](std::size_t i) const { return data_[i]; }

    double* begin() { return data_; }
    double* end() { return data_ + size_; }
    const double* begin() const { return data_; }
    const double* end() const { return data_ + size_; }

private:
    double* data_ = nullptr;
    std::size_t size_ = 0;
};

}  // namespace course
