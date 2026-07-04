#pragma once

#include <cstdint>

namespace course {

/// Streaming (single-pass, O(1) memory) statistics over a sequence of
/// doubles. Value type: copying yields an independent aggregate.
///
/// Precondition: mean/min/max/variance require count() > 0.
class RunningStats {
public:
    /// Folds one observation into the aggregate. O(1).
    void add(double x) {
        // TODO: Welford update (README THEORY §4).
        (void)x;
    }

    /// Folds another aggregate into this one, as if its whole stream had
    /// been add()ed here. `other` is not modified.
    void merge(const RunningStats& other) {
        // TODO: parallel merge (Hint 3).
        (void)other;
    }

    std::int64_t count() const {
        return 0;  // TODO
    }

    double mean() const {
        return 0.0;  // TODO
    }

    double min() const {
        return 0.0;  // TODO
    }

    double max() const {
        return 0.0;  // TODO
    }

    /// Population variance (divides by n, not n-1).
    double variance() const {
        return 0.0;  // TODO
    }

private:
    // TODO: choose your representation (Hint 1).
};

}  // namespace course
