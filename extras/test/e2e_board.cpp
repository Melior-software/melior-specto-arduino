#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include "specto/SpectoCore.h"
#include "specto/SpectoWs.h"

using namespace specto;

static uint32_t nowMs() {
    using namespace std::chrono;
    return (uint32_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
static uint32_t rnd() { static std::mt19937 g(42); return g(); }

struct SocketIo : Io {
    int fd = -1;
    std::string buf;
    size_t at = 0;
    void fill() {
        if (fd < 0) return;
        pollfd p{fd, POLLIN, 0};
        while (poll(&p, 1, 0) > 0 && (p.revents & POLLIN)) {
            char tmp[2048];
            ssize_t n = recv(fd, tmp, sizeof tmp, 0);
            if (n <= 0) { close(fd); fd = -1; return; }
            buf.append(tmp, (size_t)n);
        }
    }
    int available() override { fill(); return (int)(buf.size() - at); }
    int read() override { if (!available()) return -1; return (unsigned char)buf[at++]; }
    size_t write(const uint8_t* b, size_t n) override { return fd < 0 ? 0 : (size_t)send(fd, b, n, 0); }
    bool connected() override { return fd >= 0 || at < buf.size(); }
};

int main(int argc, char** argv) {
    if (argc < 5) { std::fprintf(stderr, "usage: e2e_board host port key seconds\n"); return 2; }
    SocketIo io;
    addrinfo hints{}, *res;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(argv[1], argv[2], &hints, &res) != 0) return 3;
    io.fd = socket(res->ai_family, res->ai_socktype, 0);
    if (connect(io.fd, res->ai_addr, res->ai_addrlen) != 0) { std::perror("connect"); return 3; }
    Ws ws(io, nowMs, rnd);
    if (!ws.handshake(argv[1], "/v1/board/ws", argv[3])) { std::printf("HANDSHAKE %d\n", ws.status()); return 4; }
    std::printf("ONLINE\n");
    Core core;
    float teplota = 20;
    bool tl1 = false;
    int navic = 1;
    core.add("teplota", Kind::Float, &teplota);
    core.add("tl1", Kind::Bool, &tl1);
    core.add("navic", Kind::Int, &navic);
    char buf[1024];
    size_t n = core.hello(buf, sizeof buf, "e2e", "host");
    ws.sendText(buf, n);
    uint32_t end = nowMs() + (uint32_t)std::atoi(argv[4]) * 1000;
    int sentCount = 0;
    bool early = std::getenv("E2E_EARLY") != nullptr;
    while ((int32_t)(nowMs() - end) < 0 && !ws.closed()) {
        size_t len;
        while (ws.poll(buf, sizeof buf - 1, len)) {
            buf[len] = 0;
            std::printf("RECV %s\n", buf);
            core.handle(buf, len);
            if (std::strstr(buf, "\"t\":\"w\"")) std::printf("WRITE tl1=%d\n", (int)tl1);
        }
        if (core.due(nowMs()) == 0 || early) {
            teplota += 0.5f;
            n = core.values(buf, sizeof buf);
            if (ws.sendText(buf, n)) { core.sent(nowMs()); sentCount++; std::printf("SENT %s rate=%u\n", buf, core.rateMs); }
            if (early) usleep(200000);
        }
        usleep(20000);
        std::fflush(stdout);
    }
    std::printf("END closed=%d sent=%d\n", (int)ws.closed(), sentCount);
    return 0;
}
