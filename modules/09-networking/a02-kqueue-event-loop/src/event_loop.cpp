#include "course/event_loop.hpp"

#include <sys/event.h>
#include <unistd.h>

#include <stdexcept>

namespace course {

EventLoop::EventLoop() : kq_(::kqueue()) {
    if (kq_ < 0) throw std::runtime_error{"kqueue() failed"};
}

EventLoop::~EventLoop() {
    if (kq_ >= 0) ::close(kq_);
}

bool EventLoop::watch_read(int fd, std::function<void(int)> on_readable) {
    // TODO: EV_SET(..., fd, EVFILT_READ, EV_ADD, ...) + kevent register;
    // remember the callback (Hint 1).
    (void)fd;
    (void)on_readable;
    return false;
}

bool EventLoop::unwatch(int fd) {
    // TODO: EV_DELETE + forget the callback.
    (void)fd;
    return false;
}

void EventLoop::add_timer(std::chrono::milliseconds delay,
                          std::function<void()> on_fire) {
    // TODO: EVFILT_TIMER, EV_ADD | EV_ONESHOT, data = ms (Hint 2).
    (void)delay;
    (void)on_fire;
}

int EventLoop::run_once(std::chrono::milliseconds max_wait) {
    // TODO: kevent wait with a timespec timeout, then dispatch each fired
    // event by (filter, ident) — copy callbacks out before invoking
    // (Hint 3).
    (void)max_wait;
    return 0;
}

void EventLoop::run() {
    // TODO: running_ = true; run_once(...) until stop() clears it.
}

void EventLoop::stop() {
    // TODO
}

}  // namespace course
