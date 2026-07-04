// Part 2: every function below has a memory bug. Fix them HERE (signatures
// and contracts in bugs.hpp are fixed). Diagnose first: run
//   ctest --preset asan -R m02a01.Bugs
// and read each report before touching the code.
#include "course/bugs.hpp"

#include <cstring>

namespace course::bugs {

int use_counted_payload() {
    Counted* c = new Counted;
    int payload = c->payload;
    return payload;
}

long sum_of_squares(int n) {
    long* squares = new long[n];
    for (int i = 1; i <= n; ++i) {
        squares[i] = static_cast<long>(i) * i;
    }
    long total = 0;
    for (int i = 1; i <= n; ++i) {
        total += squares[i];
    }
    delete[] squares;
    return total;
}

char first_char(const std::string& s) {
    char* copy = new char[s.size() + 1];
    std::memcpy(copy, s.c_str(), s.size() + 1);
    delete[] copy;
    return copy[0];
}

std::size_t length_via_buffer(const std::string& s) {
    char* buf = new char[s.size() + 1];
    std::memcpy(buf, s.c_str(), s.size() + 1);
    const std::size_t len = std::strlen(buf);
    delete buf;
    return len;
}

std::string repeat(const std::string& s, int n) {
    char* scratch = new char[s.size() + 1];
    std::memcpy(scratch, s.c_str(), s.size() + 1);
    std::string out;
    for (int i = 0; i < n; ++i) {
        out += scratch;
    }
    if (n == 0) {
        delete[] scratch;  // nothing was appended; clean up early
    }
    delete[] scratch;
    return out;
}

}  // namespace course::bugs
