// The spec for a03-raii-first-contact.
#include "course/raii.hpp"

#include <gtest/gtest.h>

#include <fcntl.h>

#include <cerrno>
#include <chrono>
#include <fstream>
#include <string>
#include <system_error>
#include <thread>
#include <utility>

namespace {

using course::PosixFile;
using course::ScopeTimer;

// Creates a real file under gtest's temp dir and returns its path.
std::string write_fixture(const std::string& name, const std::string& body) {
    const std::string path = testing::TempDir() + name;
    std::ofstream out(path, std::ios::trunc | std::ios::binary);
    out << body;
    return path;
}

bool fd_is_open(int fd) { return ::fcntl(fd, F_GETFD) != -1; }

// --- PosixFile ---------------------------------------------------------------

TEST(PosixFile, OpensAndReadsWholeFile) {
    const auto path = write_fixture("m01a03_read.txt", "hello\nworld\n");
    PosixFile f{path};
    EXPECT_GE(f.fd(), 0);
    EXPECT_EQ(f.read_all(), "hello\nworld\n");
}

TEST(PosixFile, ReadsEmptyFile) {
    const auto path = write_fixture("m01a03_empty.txt", "");
    PosixFile f{path};
    EXPECT_EQ(f.read_all(), "");
}

TEST(PosixFile, ReadsFileLargerThanOneBuffer) {
    // Bigger than any sane single read buffer — forces the read(2) loop.
    const std::string body(1 << 20, 'q');  // 1 MiB
    const auto path = write_fixture("m01a03_big.txt", body);
    PosixFile f{path};
    EXPECT_EQ(f.read_all(), body);
}

TEST(PosixFile, ThrowsSystemErrorWithErrnoForMissingFile) {
    try {
        PosixFile f{"/definitely/not/a/real/path/m01a03"};
        FAIL() << "constructor must throw std::system_error";
    } catch (const std::system_error& e) {
        EXPECT_EQ(e.code().value(), ENOENT);
    }
}

TEST(PosixFile, DestructorClosesTheDescriptor) {
    const auto path = write_fixture("m01a03_close.txt", "x");
    int raw_fd = -1;
    {
        PosixFile f{path};
        raw_fd = f.fd();
        ASSERT_GE(raw_fd, 0);
        ASSERT_TRUE(fd_is_open(raw_fd));
    }  // <- RAII: close(2) must happen exactly here
    EXPECT_FALSE(fd_is_open(raw_fd)) << "fd still open after destruction";
}

TEST(PosixFile, MoveConstructorTransfersOwnership) {
    const auto path = write_fixture("m01a03_movec.txt", "payload");
    PosixFile a{path};
    const int raw_fd = a.fd();

    PosixFile b{std::move(a)};
    EXPECT_EQ(b.fd(), raw_fd);
    EXPECT_EQ(a.fd(), -1);  // moved-from is empty
    EXPECT_EQ(b.read_all(), "payload");

    // Destroying the moved-from object must NOT close b's fd.
    { PosixFile c{std::move(a)}; }
    EXPECT_TRUE(fd_is_open(raw_fd));
}

TEST(PosixFile, MoveAssignmentClosesTheOldDescriptor) {
    const auto path1 = write_fixture("m01a03_movea1.txt", "one");
    const auto path2 = write_fixture("m01a03_movea2.txt", "two");
    PosixFile a{path1};
    PosixFile b{path2};
    const int fd_a = a.fd();
    const int fd_b = b.fd();

    a = std::move(b);
    EXPECT_FALSE(fd_is_open(fd_a)) << "old fd leaked by move assignment";
    EXPECT_EQ(a.fd(), fd_b);
    EXPECT_EQ(b.fd(), -1);
    EXPECT_EQ(a.read_all(), "two");
}

// --- ScopeTimer ----------------------------------------------------------------

TEST(ScopeTimer, WritesElapsedOnlyAtScopeExit) {
    std::int64_t ns = -1;
    {
        ScopeTimer t{ns};
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        EXPECT_EQ(ns, -1) << "timer must not write before destruction";
    }
    // Slept >= 20ms; allow generous slack either side, but it must be
    // plausible wall time, not 0 and not garbage.
    EXPECT_GE(ns, 10'000'000) << "elapsed implausibly small";
    EXPECT_LT(ns, 10'000'000'000) << "elapsed implausibly large";
}

TEST(ScopeTimer, MeasuresEveryExitPath) {
    std::int64_t ns = -1;
    const auto timed_early_return = [&ns] {
        ScopeTimer t{ns};
        return 42;  // early return still triggers the destructor
    };
    EXPECT_EQ(timed_early_return(), 42);
    EXPECT_GE(ns, 0) << "early return path was not measured";
}

TEST(ScopeTimer, NestedTimersAreIndependent) {
    std::int64_t outer = -1, inner = -1;
    {
        ScopeTimer to{outer};
        {
            ScopeTimer ti{inner};
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    EXPECT_GE(inner, 0);
    EXPECT_GE(outer, inner) << "outer scope contains inner scope";
}

}  // namespace
