#pragma once

// course::Jthread — std::jthread, portably.
//
// Apple's libc++ (through the Xcode toolchains on the CI runners and
// most Macs) has not shipped std::jthread, years after C++20; GCC's
// libstdc++ has. Course code therefore uses course::Jthread: the real
// thing where available, a minimal join-on-destruction wrapper where
// not. The semantics you should LEARN are std::jthread's (m07a01
// THEORY §1); this shim exists so the same tests build on both
// standard libraries — and knowing that library support lags the
// standard (and where to check: the libc++/libstdc++ status pages) is
// itself course material.
//
// The wrapper deliberately omits stop_token support — course code
// signals shutdown explicitly (atomics, close(), poison packets),
// which works everywhere.

#include <thread>
#include <type_traits>
#include <utility>

#if defined(__cpp_lib_jthread)

namespace course {
using Jthread = std::jthread;
}  // namespace course

#else

namespace course {

/// Join-on-destruction thread: the RAII half of std::jthread.
class Jthread {
public:
    Jthread() noexcept = default;

    template <typename F, typename... Args,
              typename = std::enable_if_t<
                  !std::is_same_v<std::remove_cvref_t<F>, Jthread>>>
    explicit Jthread(F&& f, Args&&... args)
        : thread_(std::forward<F>(f), std::forward<Args>(args)...) {}

    ~Jthread() {
        if (thread_.joinable()) thread_.join();
    }

    Jthread(Jthread&&) noexcept = default;
    Jthread& operator=(Jthread&& other) noexcept {
        if (this != &other) {
            if (thread_.joinable()) thread_.join();
            thread_ = std::move(other.thread_);
        }
        return *this;
    }

    Jthread(const Jthread&) = delete;
    Jthread& operator=(const Jthread&) = delete;

    void join() { thread_.join(); }
    bool joinable() const noexcept { return thread_.joinable(); }

private:
    std::thread thread_;
};

}  // namespace course

#endif
