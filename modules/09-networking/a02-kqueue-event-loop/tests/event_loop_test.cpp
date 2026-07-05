// The spec for m09a02 (macOS only). Pipes drive the loop: write to the
// pipe's write end -> the read end becomes readable -> your loop fires.
// Deterministic, no network, no sudo.
#include "course/event_loop.hpp"

#include <gtest/gtest.h>

#include <unistd.h>

#include <chrono>
#include <string>
#include <vector>

namespace {

using course::EventLoop;
using namespace std::chrono_literals;

struct Pipe {
    int rd = -1;
    int wr = -1;
    Pipe() {
        int fds[2];
        if (::pipe(fds) == 0) {
            rd = fds[0];
            wr = fds[1];
        }
    }
    ~Pipe() {
        if (rd >= 0) ::close(rd);
        if (wr >= 0) ::close(wr);
    }
    void write(const std::string& s) const {
        (void)::write(wr, s.data(), s.size());
    }
    std::string drain() const {
        char buf[256];
        const auto n = ::read(rd, buf, sizeof buf);
        return n > 0 ? std::string(buf, static_cast<std::size_t>(n)) : "";
    }
};

TEST(EventLoopTest, RunOnceTimesOutQuietly) {
    EventLoop loop;
    EXPECT_EQ(loop.run_once(10ms), 0) << "nothing watched, nothing fires";
}

TEST(EventLoopTest, ReadableFdFiresCallbackWithItsFd) {
    EventLoop loop;
    Pipe p;
    ASSERT_GE(p.rd, 0);

    int fired_fd = -1;
    ASSERT_TRUE(loop.watch_read(p.rd, [&](int fd) {
        fired_fd = fd;
        p.drain();
    }));

    EXPECT_EQ(loop.run_once(10ms), 0) << "no data yet — must not fire";
    p.write("x");
    EXPECT_EQ(loop.run_once(1000ms), 1);
    EXPECT_EQ(fired_fd, p.rd) << "callback must receive the ready fd";
}

TEST(EventLoopTest, LevelTriggeredKeepsFiringUntilDrained) {
    EventLoop loop;
    Pipe p;
    int fires = 0;
    ASSERT_TRUE(loop.watch_read(p.rd, [&](int) { ++fires; }));

    p.write("data");
    EXPECT_EQ(loop.run_once(1000ms), 1);
    EXPECT_EQ(loop.run_once(1000ms), 1)
        << "data still unread: level-triggered must report again";
    EXPECT_EQ(fires, 2);
    p.drain();
    EXPECT_EQ(loop.run_once(10ms), 0) << "drained: quiet again";
}

TEST(EventLoopTest, TwoFdsAreIndependent) {
    EventLoop loop;
    Pipe a;
    Pipe b;
    std::vector<int> fired;
    ASSERT_TRUE(loop.watch_read(a.rd, [&](int fd) {
        fired.push_back(fd);
        a.drain();
    }));
    ASSERT_TRUE(loop.watch_read(b.rd, [&](int fd) {
        fired.push_back(fd);
        b.drain();
    }));

    b.write("only b");
    EXPECT_EQ(loop.run_once(1000ms), 1);
    ASSERT_EQ(fired.size(), 1u);
    EXPECT_EQ(fired[0], b.rd) << "only the fd with data may fire";

    a.write("now a");
    b.write("and b");
    int handled = loop.run_once(1000ms);
    // Both may arrive in one wait or two, but both must arrive.
    if (handled == 1) handled += loop.run_once(1000ms);
    EXPECT_EQ(handled, 2);
    EXPECT_EQ(fired.size(), 3u);
}

TEST(EventLoopTest, UnwatchStopsCallbacks) {
    EventLoop loop;
    Pipe p;
    int fires = 0;
    ASSERT_TRUE(loop.watch_read(p.rd, [&](int) { ++fires; }));
    p.write("x");
    EXPECT_EQ(loop.run_once(1000ms), 1);
    ASSERT_EQ(fires, 1);

    ASSERT_TRUE(loop.unwatch(p.rd));
    p.write("y");
    EXPECT_EQ(loop.run_once(50ms), 0) << "unwatched fd must be silent";
    EXPECT_EQ(fires, 1);
}

TEST(EventLoopTest, UnwatchDuringCallbackIsSafe) {
    EventLoop loop;
    Pipe p;
    int fires = 0;
    ASSERT_TRUE(loop.watch_read(p.rd, [&](int fd) {
        ++fires;
        loop.unwatch(fd);  // mutate the loop from inside it (Hint 3)
    }));
    p.write("x");
    EXPECT_EQ(loop.run_once(1000ms), 1);
    EXPECT_EQ(loop.run_once(50ms), 0);
    EXPECT_EQ(fires, 1);
}

TEST(EventLoopTest, OneShotTimerFiresOnceAfterDelay) {
    EventLoop loop;
    int fires = 0;
    const auto start = std::chrono::steady_clock::now();
    loop.add_timer(50ms, [&] { ++fires; });

    EXPECT_EQ(loop.run_once(5ms), 0) << "too early";
    while (fires == 0 &&
           std::chrono::steady_clock::now() - start < 2s) {
        loop.run_once(100ms);
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_EQ(fires, 1);
    EXPECT_GE(elapsed, 40ms) << "fired implausibly early";
    EXPECT_EQ(loop.run_once(100ms), 0) << "one-shot: never again";
    EXPECT_EQ(fires, 1);
}

TEST(EventLoopTest, StopFromCallbackExitsRun) {
    EventLoop loop;
    Pipe p;
    int fires = 0;
    ASSERT_TRUE(loop.watch_read(p.rd, [&](int) {
        ++fires;
        p.drain();
        loop.stop();
    }));
    p.write("go");
    loop.run();  // must return because the callback stopped it
    EXPECT_EQ(fires, 1);
}

}  // namespace
