// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Daniel Kuběna (Melior) – see LICENSE and NOTICE
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace specto {

struct Io {
    virtual ~Io() {}
    virtual int available() = 0;
    virtual int read() = 0;
    virtual size_t write(const uint8_t* b, size_t n) = 0;
    virtual bool connected() = 0;
};

typedef uint32_t (*MillisFn)();
typedef uint32_t (*RandomFn)();

class Ws {
public:
    Ws(Io& io, MillisFn millis, RandomFn random) : io(io), millis(millis), random(random) {}

    bool handshake(const char* host, const char* path, const char* key, uint32_t timeoutMs = 8000);
    int status() const { return httpStatus; }

    bool sendText(const char* s, size_t n);
    bool sendPing();

    bool poll(char* out, size_t cap, size_t& len, uint32_t waitMs = 200);

    bool closed() const { return isClosed; }
    uint32_t lastHeard() const { return heard; }

private:
    bool sendFrame(uint8_t opcode, const uint8_t* data, size_t n);
    int readByte(uint32_t until);

    Io& io;
    MillisFn millis;
    RandomFn random;
    int httpStatus = 0;
    bool isClosed = false;
    uint32_t heard = 0;
};

}
