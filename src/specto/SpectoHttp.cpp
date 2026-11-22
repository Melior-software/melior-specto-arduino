// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Daniel Kuběna (Melior) – see LICENSE and NOTICE
#include "SpectoHttp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace specto {

int Http::readByte(uint32_t until) {
    for (;;) {
        if (io.available() > 0) return io.read();
        if (!io.connected()) return -1;
        if ((int32_t)(millis() - until) >= 0) return -1;
    }
}

static size_t put(Io& io, const char* s) { return io.write((const uint8_t*)s, strlen(s)); }

static bool startsWithNoCase(const char* s, const char* prefix) {
    for (; *prefix; s++, prefix++) {
        char a = *s, b = *prefix;
        if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
        if (b >= 'A' && b <= 'Z') b = (char)(b + 32);
        if (a != b) return false;
    }
    return true;
}

int Http::post(const char* host, const char* path, const char* key, const char* body, size_t n,
               char* out, size_t cap, size_t& len, bool& keepOpen, uint32_t timeoutMs) {
    len = 0;
    keepOpen = false;
    char length[16];
    snprintf(length, sizeof length, "%u", (unsigned)n);
    put(io, "POST "); put(io, path); put(io, " HTTP/1.1\r\nHost: "); put(io, host);
    put(io, "\r\nAuthorization: Device "); put(io, key);
    put(io, "\r\nContent-Type: text/plain\r\nUser-Agent: MeliorSpecto\r\nContent-Length: "); put(io, length);
    put(io, "\r\n\r\n");
    if (n && io.write((const uint8_t*)body, n) != n) return 0;

    uint32_t until = millis() + timeoutMs;
    char line[96];
    size_t ln = 0;
    int status = 0;
    long contentLength = -1;
    bool first = true, chunked = false;
    keepOpen = true;
    for (;;) {
        int c = readByte(until);
        if (c < 0) return status;
        if (c == '\r') continue;
        if (c != '\n') { if (ln < sizeof line - 1) line[ln++] = (char)c; continue; }
        line[ln] = 0;
        if (first) {
            const char* sp = strchr(line, ' ');
            status = sp ? (int)strtol(sp + 1, nullptr, 10) : 0;
            first = false;
        } else if (ln == 0) {
            break;
        } else if (startsWithNoCase(line, "content-length:")) {
            contentLength = strtol(line + 15, nullptr, 10);
        } else if (startsWithNoCase(line, "connection:")) {
            const char* v = line + 11;
            while (*v == ' ') v++;
            if (startsWithNoCase(v, "close")) keepOpen = false;
        } else if (startsWithNoCase(line, "transfer-encoding:")) {
            chunked = strstr(line, "chunked") != nullptr;
        }
        ln = 0;
    }
    auto take = [&](int c) { if (len + 1 < cap) out[len++] = (char)c; };
    if (chunked) {
        for (;;) {
            char hex[12];
            size_t h = 0;
            int c;
            while ((c = readByte(until)) >= 0 && c != '\n') if (c != '\r' && h < sizeof hex - 1) hex[h++] = (char)c;
            if (c < 0) break;
            hex[h] = 0;
            long size = strtol(hex, nullptr, 16);
            if (size <= 0) { readByte(until); readByte(until); break; }
            for (long i = 0; i < size; i++) { c = readByte(until); if (c < 0) break; take(c); }
            readByte(until); readByte(until);
        }
    } else if (contentLength >= 0) {
        for (long i = 0; i < contentLength; i++) { int c = readByte(until); if (c < 0) break; take(c); }
    } else {
        keepOpen = false;
        for (int c; (c = readByte(until)) >= 0;) take(c);
    }
    if (cap) out[len] = 0;
    return status;
}

}
