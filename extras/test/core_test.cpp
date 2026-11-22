#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include "specto/SpectoCore.h"
#include "specto/SpectoWs.h"
#include "specto/SpectoHttp.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

using namespace specto;

static std::string num(double v) { char b[32]; size_t n = formatNumber(v, b, sizeof b); return std::string(b, n); }

static void numbers() {
    CHECK(num(0) == "0");
    CHECK(num(21.5) == "21.5");
    CHECK(num(-3) == "-3");
    CHECK(num(0.1) == "0.1");
    CHECK(num(1234567.25) == "1234567.25");
    CHECK(num(-0.000001) == "-0.000001");
    CHECK(num(NAN) == "null");
    CHECK(num(INFINITY) == "null");
    char tiny[3];
    CHECK(formatNumber(12345, tiny, sizeof tiny) == 0);
    double v;
    CHECK(parseNumber("21.5", v) && v == 21.5);
    CHECK(parseNumber("-4", v) && v == -4);
    CHECK(parseNumber("true", v) && v == 1);
    CHECK(parseNumber("false", v) && v == 0);
    CHECK(!parseNumber("abc", v) && !parseNumber("", v) && !parseNumber("1x", v));
}

static void fields() {
    const char* j = "{\"t\":\"w\",\"n\":\"tl1\",\"v\":1, \"msg\":\"a \\\"b\\\"\"}";
    char out[32];
    CHECK(jsonField(j, strlen(j), "t", out, sizeof out) && std::string(out) == "w");
    CHECK(jsonField(j, strlen(j), "n", out, sizeof out) && std::string(out) == "tl1");
    CHECK(jsonField(j, strlen(j), "v", out, sizeof out) && std::string(out) == "1");
    CHECK(jsonField(j, strlen(j), "msg", out, sizeof out) && std::string(out) == "a \"b\"");
    CHECK(!jsonField(j, strlen(j), "x", out, sizeof out));
    const char* k = "{\"msg\":\"\\\"t\\\":\\\"x\\\"\",\"t\":\"rate\"}";
    CHECK(jsonField(k, strlen(k), "t", out, sizeof out) && std::string(out) == "rate");
}

static int pinState[40];
static void pinWrite(int pin, bool on) { pinState[pin] = on; }
static int pinRead(int pin) { return pinState[pin]; }
static double lastCb = -1;
static void cb(double v) { lastCb = v; }

static void table() {
    Core c;
    c.pinWrite = pinWrite;
    c.pinRead = pinRead;
    float teplota = 21.5f;
    bool tl1 = false;
    int pocet = 7;
    CHECK(c.add("teplota", Kind::Float, &teplota));
    CHECK(c.add("tl1", Kind::Bool, &tl1));
    CHECK(c.add("pocet", Kind::Int, &pocet));
    CHECK(c.add("rele", Kind::Pin, nullptr, 5));
    CHECK(c.add("tlacitko", Kind::PinIn, nullptr, 4));
    CHECK(!c.add("tl1", Kind::Bool, &tl1));
    CHECK(!c.add("", Kind::Bool, &tl1));
    CHECK(!c.add("a\"b", Kind::Bool, &tl1));
    CHECK(!c.add("0123456789012345678901234", Kind::Bool, &tl1));

    char buf[512];
    size_t n = c.hello(buf, sizeof buf, "1.0.0", "esp32");
    std::string h(buf, n);
    CHECK(h == "{\"t\":\"hello\",\"lib\":\"1.0.0\",\"board\":\"esp32\",\"vars\":[{\"n\":\"teplota\",\"y\":\"f\"},"
               "{\"n\":\"tl1\",\"y\":\"b\"},{\"n\":\"pocet\",\"y\":\"i\"},{\"n\":\"rele\",\"y\":\"b\"},{\"n\":\"tlacitko\",\"y\":\"b\"}]}");

    pinState[4] = 1;
    n = c.values(buf, sizeof buf);
    CHECK(std::string(buf, n) == "{\"t\":\"v\",\"v\":{\"teplota\":21.5,\"tl1\":false,\"pocet\":7,\"rele\":false,\"tlacitko\":true}}");
    char small[20];
    CHECK(c.values(small, sizeof small) == 0);

    const char* w = "{\"t\":\"w\",\"n\":\"tl1\",\"v\":1}";
    CHECK(c.handle(w, strlen(w)) && tl1);
    const char* w2 = "{\"t\":\"w\",\"n\":\"pocet\",\"v\":41.6}";
    CHECK(c.handle(w2, strlen(w2)) && pocet == 42);
    const char* w3 = "{\"t\":\"w\",\"n\":\"rele\",\"v\":true}";
    CHECK(c.handle(w3, strlen(w3)) && pinState[5] == 1);
    const char* w4 = "{\"t\":\"w\",\"n\":\"nic\",\"v\":1}";
    CHECK(!c.handle(w4, strlen(w4)));
    const char* w5 = "{\"t\":\"w\",\"n\":\"teplota\",\"v\":\"x\"}";
    CHECK(!c.handle(w5, strlen(w5)) && teplota == 21.5f);
    c.find("tl1")->onWrite = cb;
    const char* w6 = "{\"t\":\"w\",\"n\":\"tl1\",\"v\":0}";
    CHECK(c.handle(w6, strlen(w6)) && !tl1 && lastCb == 0);
    const char* w7 = "{\"t\":\"w\",\"n\":\"tlacitko\",\"v\":0}";
    CHECK(!c.handle(w7, strlen(w7)) && pinState[4] == 1);

    const char* r = "{\"t\":\"rate\",\"ms\":60000}";
    CHECK(c.handle(r, strlen(r)) && c.rateMs == 60000);
    const char* r2 = "{\"t\":\"rate\",\"ms\":10}";
    CHECK(c.handle(r2, strlen(r2)) && c.rateMs == Core::WATCHED_MS);
    const char* e = "{\"t\":\"err\",\"code\":\"limit\",\"msg\":\"Denní limit vyčerpán.\"}";
    CHECK(c.handle(e, strlen(e)) && std::string(c.errorCode) == "limit");
    CHECK(!c.handle("{", 1) && !c.handle("{\"t\":\"zzz\"}", 11));
    c.errorMessage[0] = 0;
    const char* wt = "{\"t\":\"err\",\"code\":\"wait\",\"ms\":42000,\"msg\":\"Příliš brzy\"}";
    CHECK(c.handle(wt, strlen(wt)) && c.holdMs == 42000 && c.errorCode[0] == 0 && c.errorMessage[0] == 0);
    c.rateMs = 60000;
    c.hold(100000, c.holdMs);
    CHECK(c.due(100000) == 42000 && c.due(142000) == 0);
}

static void timing() {
    Core c;
    bool b = false;
    c.add("b", Kind::Bool, &b);
    CHECK(c.due(5000) == 0);
    char buf[128];
    c.values(buf, sizeof buf);
    c.sent(5000);
    c.rateMs = 60000;
    CHECK(c.due(5000) == 60000 && c.due(35000) == 30000 && c.due(65000) == 0);
    CHECK(c.due(4000) == 0);
    CHECK(!c.boolChanged());
    b = true;
    CHECK(c.boolChanged());
    for (int i = 0; i < Core::RESERVE_PER_HOUR; i++) CHECK(c.takeReserve(10000));
    CHECK(!c.takeReserve(10000));
    CHECK(c.takeReserve(10000 + 3600000UL));
}

struct FakeIo : Io {
    std::string in, out;
    size_t at = 0;
    bool open = true;
    int available() override { return (int)(in.size() - at); }
    int read() override { return at < in.size() ? (uint8_t)in[at++] : -1; }
    size_t write(const uint8_t* b, size_t n) override { out.append((const char*)b, n); return n; }
    bool connected() override { return open; }
};

static uint32_t clock_ms = 0;
static uint32_t fakeMillis() { return clock_ms; }
static uint32_t rnd = 1;
static uint32_t fakeRandom() { rnd = rnd * 1103515245u + 12345u; return rnd; }

static void websocket() {
    FakeIo io;
    Ws ws(io, fakeMillis, fakeRandom);
    io.in = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n\r\n";
    CHECK(ws.handshake("specto.melior.software", "/v1/board/ws", "abc.def"));
    CHECK(io.out.find("GET /v1/board/ws HTTP/1.1\r\n") == 0);
    CHECK(io.out.find("Host: specto.melior.software\r\n") != std::string::npos);
    CHECK(io.out.find("Authorization: Device abc.def\r\n") != std::string::npos);
    CHECK(io.out.find("Sec-WebSocket-Version: 13\r\n") != std::string::npos);

    io.out.clear();
    CHECK(ws.sendText("{\"t\":\"v\"}", 9));
    CHECK(io.out.size() == 2 + 4 + 9);
    CHECK((uint8_t)io.out[0] == 0x81 && (uint8_t)io.out[1] == (0x80 | 9));
    std::string un;
    for (size_t i = 0; i < 9; i++) un += (char)(io.out[6 + i] ^ io.out[2 + (i % 4)]);
    CHECK(un == "{\"t\":\"v\"}");

    io.out.clear();
    std::string big(130, 'x');
    CHECK(ws.sendText(big.c_str(), big.size()));
    CHECK((uint8_t)io.out[1] == (0x80 | 126) && (uint8_t)io.out[2] == 0 && (uint8_t)io.out[3] == 130);

    io.in = std::string("\x89\x02hi", 4) + std::string("\x81\x0c{\"t\":\"rate\"}", 14);
    io.at = 0;
    io.out.clear();
    char msg[64];
    size_t len = 0;
    CHECK(ws.poll(msg, sizeof msg, len) && std::string(msg, len) == "{\"t\":\"rate\"}");
    CHECK(io.out.size() == 2 + 4 + 2 && (uint8_t)io.out[0] == 0x8A);
    io.in = std::string("\x88\x00", 2);
    io.at = 0;
    CHECK(!ws.poll(msg, sizeof msg, len) && ws.closed());

    Ws ws2(io, fakeMillis, fakeRandom);
    io.in = std::string("\x81\x05hello", 7) + std::string("\x81\x02ok", 4);
    io.at = 0;
    char tiny[4];
    CHECK(!ws2.poll(tiny, sizeof tiny, len));
    CHECK(ws2.poll(tiny, sizeof tiny, len) && std::string(tiny, len) == "ok");

    FakeIo io3;
    Ws ws3(io3, fakeMillis, fakeRandom);
    io3.in = "HTTP/1.1 401 Unauthorized\r\n\r\n";
    CHECK(!ws3.handshake("h", "/", "k"));
    CHECK(ws3.status() == 401);
}

static void http() {
    FakeIo io;
    Http h(io, fakeMillis);
    char out[64];
    size_t len = 0;
    bool keep = false;
    io.in = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\ncontent-length: 23\r\n\r\n{\"t\":\"rate\",\"ms\":1000}\n";
    CHECK(h.post("specto.melior.software", "/v1/board/link", "a.b", "x\n", 2, out, sizeof out, len, keep) == 200);
    CHECK(std::string(out, len) == "{\"t\":\"rate\",\"ms\":1000}\n" && keep);
    CHECK(io.out.find("POST /v1/board/link HTTP/1.1\r\n") == 0 && io.out.find("Content-Length: 2\r\n\r\nx\n") != std::string::npos);
    CHECK(io.out.find("Authorization: Device a.b\r\n") != std::string::npos);
    FakeIo io2;
    Http h2(io2, fakeMillis);
    io2.in = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n5\r\nhello\r\n3\r\n ok\r\n0\r\n\r\n";
    CHECK(h2.post("h", "/", "k", "", 0, out, sizeof out, len, keep) == 200 && std::string(out, len) == "hello ok" && !keep);
    FakeIo io3;
    Http h3(io3, fakeMillis);
    io3.in = "HTTP/1.1 401 Unauthorized\r\nContent-Length: 0\r\n\r\n";
    CHECK(h3.post("h", "/", "k", "", 0, out, sizeof out, len, keep) == 401 && len == 0);
    FakeIo io4;
    Http h4(io4, fakeMillis);
    io4.in = "HTTP/1.1 200 OK\r\nContent-Length: 100\r\n\r\n" + std::string(100, 'x');
    char small[8];
    CHECK(h4.post("h", "/", "k", "", 0, small, sizeof small, len, keep) == 200 && len == 7);
}

int main() {
    http();
    numbers();
    fields();
    table();
    timing();
    websocket();
    if (failures) { std::printf("%d failed\n", failures); return 1; }
    std::printf("all passed\n");
    return 0;
}
