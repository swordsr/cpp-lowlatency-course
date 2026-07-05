#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <unordered_map>

namespace course {

/// Single-threaded kqueue readiness loop (macOS/BSD). All callbacks run
/// on the thread that calls run()/run_once() — that's the concurrency
/// model: one thread, no locks, don't block in callbacks.
class EventLoop {
public:
    EventLoop();   // given: creates the kqueue fd (throws on failure)
    ~EventLoop();  // given: closes it

    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    /// Level-triggered read watch. Re-watching a watched fd replaces its
    /// callback. Returns false if the kernel refused the registration.
    bool watch_read(int fd, std::function<void(int)> on_readable);

    /// Deregisters; the callback will not run again.
    bool unwatch(int fd);

    /// One-shot timer; fires on the loop thread after ~delay.
    void add_timer(std::chrono::milliseconds delay,
                   std::function<void()> on_fire);

    /// One kevent wait (at most max_wait) + dispatch. Returns the number
    /// of events handled; 0 means the wait timed out.
    int run_once(std::chrono::milliseconds max_wait);

    /// run_once until stop() — which is legal to call from a callback.
    void run();
    void stop();

private:
    int kq_ = -1;
    bool running_ = false;
    std::uint64_t next_timer_id_ = 1;
    std::unordered_map<int, std::function<void(int)>> read_callbacks_;
    std::unordered_map<std::uint64_t, std::function<void()>> timer_callbacks_;
};

}  // namespace course
