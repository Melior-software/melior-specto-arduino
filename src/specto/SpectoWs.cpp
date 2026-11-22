// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Daniel Kuběna (Melior) – see LICENSE and NOTICE
#include "SpectoWs.h"
#include <stdlib.h>
#include <string.h>

namespace specto {

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static size_t put(Io& io, const char* s) { return io.write((const uint8_t*)s, strlen(s)); }

int Ws::readByte(uint32_t until) {
    for (;;) {
        if (io.available() > 0) return io.read();
        if (!io.connected()) return -1;
        if ((int32_t)(millis() - until) >= 0) return -1;
    }
}

bool Ws::handshake(const char* host, const char* path, const char* key, uint32_t timeoutMs) {
    httpStatus = 0;
    isClosed = false;
    uint8_t nonce[16];
    for (int i = 0; i < 16; i += 4) {
        uint32_t r = random();
        for (int k = 0; k < 4; k++) nonce[i + k] = (uint8_t)(r >> (8 * k));
    }
    char wsKey[25];
    int o = 0;
    for (int i = 0; i < 16; i += 3) {
        uint32_t v = (uint32_t)nonce[i] << 16 | (i + 1 < 16 ? (uint32_t)nonce[i + 1] << 8 : 0) | (i + 2 < 16 ? nonce[i + 2] : 0);
        wsKey[o++] = B64[(v >> 18) & 63];
        wsKey[o++] = B64[(v >> 12) & 63];
        wsKey[o++] = i + 1 < 16 ? B64[(v >> 6) & 63] : '=';
        wsKey[o++] = i + 2 < 16 ? B64[v & 63] : '=';
    }
    wsKey[o] = 0;
    put(io, "GET "); put(io, path); put(io, " HTTP/1.1\r\nHost: "); put(io, host);
    put(io, "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: "); put(io, wsKey);
    put(io, "\r\nSec-WebSocket-Version: 13\r\nAuthorization: Device "); put(io, key);
    put(io, "\r\nUser-Agent: MeliorSpecto\r\n\r\n");

    uint32_t until = millis() + timeoutMs;
    char line[64];
    size_t n = 0;
    bool first = true;
    for (;;) {
        int c = readByte(until);
        if (c < 0) return false;
        if (c == '\r') continue;
        if (c != '\n') { if (n < sizeof line - 1) line[n++] = (char)c; continue; }
        line[n] = 0;
        if (first) {
            const char* sp = strchr(line, ' ');
            httpStatus = sp ? (int)strtol(sp + 1, nullptr, 10) : 0;
            first = false;
        } else if (n == 0) {
            break;
        }
        n = 0;
    }
    heard = millis();
    return httpStatus == 101;
}

bool Ws::sendFrame(uint8_t opcode, const uint8_t* data, size_t n) {
    if (isClosed || !io.connected()) return false;
    uint8_t head[8];
    size_t h = 0;
    head[h++] = 0x80 | opcode;
    if (n < 126) {
        head[h++] = 0x80 | (uint8_t)n;
    } else if (n <= 0xFFFF) {
        head[h++] = 0x80 | 126;
        head[h++] = (uint8_t)(n >> 8);
        head[h++] = (uint8_t)n;
    } else {
        return false;
    }
    uint32_t r = random();
    uint8_t mask[4] = {(uint8_t)r, (uint8_t)(r >> 8), (uint8_t)(r >> 16), (uint8_t)(r >> 24)};
    if (io.write(head, h) != h || io.write(mask, 4) != 4) return false;
    uint8_t chunk[64];
    for (size_t i = 0; i < n;) {
        size_t k = 0;
        for (; k < sizeof chunk && i < n; k++, i++) chunk[k] = data[i] ^ mask[i % 4];
        if (io.write(chunk, k) != k) return false;
    }
    return true;
}

bool Ws::sendText(const char* s, size_t n) { return sendFrame(0x1, (const uint8_t*)s, n); }
bool Ws::sendPing() { return sendFrame(0x9, nullptr, 0); }

bool Ws::poll(char* out, size_t cap, size_t& len, uint32_t waitMs) {
    len = 0;
    while (!isClosed && io.available() >= 2) {
        uint32_t until = millis() + waitMs;
        int b0 = readByte(until), b1 = readByte(until);
        if (b0 < 0 || b1 < 0) return false;
        uint8_t opcode = b0 & 0x0F;
        bool masked = b1 & 0x80;
        uint64_t n = b1 & 0x7F;
        if (n == 126) {
            int a = readByte(until), b = readByte(until);
            if (a < 0 || b < 0) return false;
            n = (uint64_t)a << 8 | (uint64_t)b;
        } else if (n == 127) {
            n = 0;
            for (int i = 0; i < 8; i++) { int c = readByte(until); if (c < 0) return false; n = n << 8 | (uint64_t)c; }
        }
        uint8_t mask[4] = {0, 0, 0, 0};
        if (masked) for (int i = 0; i < 4; i++) { int c = readByte(until); if (c < 0) return false; mask[i] = (uint8_t)c; }
        uint8_t ctl[125];
        bool fits = opcode >= 0x8 ? n <= sizeof ctl : n <= cap;
        for (uint64_t i = 0; i < n; i++) {
            int c = readByte(until + 2000);
            if (c < 0) return false;
            uint8_t v = (uint8_t)c ^ mask[i % 4];
            if (!fits) continue;
            if (opcode >= 0x8) ctl[i] = v; else out[i] = (char)v;
        }
        heard = millis();
        switch (opcode) {
        case 0x1:
            if (!fits || !(b0 & 0x80)) return false;
            len = (size_t)n;
            return true;
        case 0x8:
            sendFrame(0x8, nullptr, 0);
            isClosed = true;
            return false;
        case 0x9:
            sendFrame(0xA, ctl, fits ? (size_t)n : 0);
            break;
        default:
            break;
        }
    }
    return false;
}

}
