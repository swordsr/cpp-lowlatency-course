// GIVEN, complete: a synthetic CMD feed source for driving your pipeline
// by hand and for a04's experiment. Sends a random-walk market to a UDP
// port at a target rate.
//
//   ./m11a03_replayer <port> [packets_per_second=1000] [seconds=10] [gap_every=0]
//
// gap_every=N deliberately skips a sequence number every N packets so
// you can watch your recovery logic live (you'll need a snapshot source
// to fully recover — or just watch is_stale() flip and trading halt).
#include "course/md_encode.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <thread>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr,
                     "usage: %s <port> [pps=1000] [seconds=10] [gap_every=0]\n",
                     argv[0]);
        return 1;
    }
    const auto port = static_cast<std::uint16_t>(std::atoi(argv[1]));
    const long pps = argc > 2 ? std::atol(argv[2]) : 1000;
    const long seconds = argc > 3 ? std::atol(argv[3]) : 10;
    const long gap_every = argc > 4 ? std::atol(argv[4]) : 0;

    const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return 1;
    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(port);
    dst.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> drift(-1, 1);
    std::uniform_int_distribution<std::uint32_t> qty(1, 20);

    std::uint64_t seq = 1;
    std::uint64_t next_order_id = 1;
    std::int64_t mid = 10'000;
    const auto interval = std::chrono::nanoseconds(1'000'000'000L / pps);
    const auto end = std::chrono::steady_clock::now() +
                     std::chrono::seconds(seconds);
    long sent = 0;

    auto next_send = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() < end) {
        mid += drift(rng);
        // Two adds around the mid; previous pair deleted: a walking book.
        course::md::PacketBuilder pkt{seq};
        if (next_order_id > 2) {
            pkt.del(next_order_id - 2).del(next_order_id - 1);
        }
        pkt.add(next_order_id++, 'B', qty(rng), mid - 1)
            .add(next_order_id++, 'S', qty(rng), mid + 1);
        const std::uint64_t count = (next_order_id > 4) ? 4 : 2;

        if (gap_every > 0 && sent > 0 && sent % gap_every == 0) {
            ++seq;  // swallow one sequence number: a deliberate gap
        }
        const auto bytes = pkt.bytes();
        ::sendto(fd, bytes.data(), bytes.size(), 0,
                 reinterpret_cast<const sockaddr*>(&dst), sizeof dst);
        seq += count;
        ++sent;

        next_send += interval;
        std::this_thread::sleep_until(next_send);
    }
    std::printf("sent %ld packets (final seq %llu)\n", sent,
                static_cast<unsigned long long>(seq));
    ::close(fd);
    return 0;
}
