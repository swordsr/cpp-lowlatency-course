#pragma once

#include <cstddef>
#include <string>

namespace course::bugs {

/// Instrumented payload for the leak test: counts live instances so leaks
/// are detectable without LeakSanitizer. Do not modify.
struct Counted {
    Counted() { ++alive; }
    ~Counted() { --alive; }
    Counted(const Counted&) = delete;
    Counted& operator=(const Counted&) = delete;

    static inline int alive = 0;
    int payload = 7;
};

/// Each of these has a bug. Fix the DEFINITIONS in src/bugs.cpp; the
/// declarations and behavior contracts here are fixed.

/// Returns the payload of a temporarily-created Counted. Must leave
/// Counted::alive unchanged. BUG: leaks.
int use_counted_payload();

/// Returns the sum of squares 1..n (n >= 1). BUG: writes past the end.
long sum_of_squares(int n);

/// Returns the first character of the string, via a heap copy.
/// BUG: reads freed memory.
char first_char(const std::string& s);

/// Returns the length of the string using a heap char array.
/// BUG: mismatched delete.
std::size_t length_via_buffer(const std::string& s);

/// Concatenates s with itself n times (n >= 0). BUG: a path frees twice.
std::string repeat(const std::string& s, int n);

}  // namespace course::bugs
