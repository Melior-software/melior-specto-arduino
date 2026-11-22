// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Daniel Kuběna (Melior) – see LICENSE and NOTICE
#include "SpectoCore.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace specto {

size_t formatNumber(double v, char* out, size_t cap) {
    char tmp[40];
    size_t n = 0;
    if (v != v || v > 1e15 || v < -1e15) {
        memcpy(tmp, "null", 4);
        n = 4;
    } else {
        bool neg = v < 0;
        if (neg) v = -v;
        uint64_t scaled = (uint64_t)(v * 1000000.0 + 0.5);
        uint64_t whole = scaled / 1000000, frac = scaled % 1000000;
        char digits[24];
        int d = 0;
        do { digits[d++] = (char)('0' + whole % 10); whole /= 10; } while (whole);
        if (neg && (d > 1 || digits[0] != '0' || frac)) tmp[n++] = '-';
        while (d) tmp[n++] = digits[--d];
        if (frac) {
            tmp[n++] = '.';
            char f[6];
            for (int i = 5; i >= 0; i--) { f[i] = (char)('0' + frac % 10); frac /= 10; }
            int last = 5;
            while (last > 0 && f[last] == '0') last--;
            for (int i = 0; i <= last; i++) tmp[n++] = f[i];
        }
    }
    if (n >= cap) return 0;
    memcpy(out, tmp, n);
    out[n] = 0;
    return n;
}

bool parseNumber(const char* s, double& v) {
    if (!strcmp(s, "true")) { v = 1; return true; }
    if (!strcmp(s, "false")) { v = 0; return true; }
    if (!*s) return false;
    char* end = nullptr;
    v = strtod(s, &end);
    return end && *end == 0 && v == v;
}

static const char* skipString(const char* p, const char* e) {
    while (p < e) {
        if (*p == '\\') { p += 2; continue; }
        if (*p == '"') return p + 1;
        p++;
    }
    return e;
}

bool jsonField(const char* json, size_t len, const char* key, char* out, size_t cap) {
    const char* p = json;
    const char* e = json + len;
    size_t klen = strlen(key);
    int depth = 0;
    while (p < e) {
        char c = *p;
        if (c == '{' || c == '[') { depth++; p++; continue; }
        if (c == '}' || c == ']') { depth--; p++; continue; }
        if (c != '"') { p++; continue; }
        const char* s = p + 1;
        const char* after = skipString(s, e);
        const char* q = after;
        while (q < e && (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n')) q++;
        bool isKey = q < e && *q == ':';
        if (isKey && depth == 1 && (size_t)(after - s - 1) == klen && !memcmp(s, key, klen)) {
            q++;
            while (q < e && *q == ' ') q++;
            size_t o = 0;
            if (q < e && *q == '"') {
                for (const char* r = q + 1; r < e && *r != '"'; r++) {
                    char ch = *r;
                    if (ch == '\\' && r + 1 < e) {
                        r++;
                        ch = *r == 'n' ? '\n' : *r == 't' ? '\t' : *r;
                    }
                    if (o + 1 < cap) out[o++] = ch;
                }
            } else {
                for (; q < e && *q != ',' && *q != '}' && *q != ' '; q++) if (o + 1 < cap) out[o++] = *q;
            }
            if (cap) out[o] = 0;
            return true;
        }
        p = isKey ? q + 1 : after;
    }
    return false;
}

bool Core::add(const char* name, Kind kind, void* ptr, int pin) {
    if (!name || n >= MAX_VARS) return false;
    size_t l = strlen(name);
    if (l == 0 || l > Var::NAME || strpbrk(name, "\"\\") || find(name)) return false;
    for (size_t i = 0; i < l; i++) if ((unsigned char)name[i] < 0x20) return false;
    Var& v = vars[n++];
    memcpy(v.name, name, l + 1);
    v.kind = kind;
    v.ptr = ptr;
    v.pin = pin;
    v.value = 0;
    v.sent = false;
    v.onWrite = nullptr;
    v.value = read(v);
    return true;
}

Var* Core::find(const char* name) {
    for (size_t i = 0; i < n; i++) if (!strcmp(vars[i].name, name)) return &vars[i];
    return nullptr;
}

double Core::read(const Var& v) const {
    switch (v.kind) {
    case Kind::Bool: return *(bool*)v.ptr ? 1 : 0;
    case Kind::Int: return *(int*)v.ptr;
    case Kind::Long: return (double)*(long*)v.ptr;
    case Kind::Float: return *(float*)v.ptr;
    case Kind::Double: return *(double*)v.ptr;
    case Kind::PinIn: return pinRead ? (pinRead(v.pin) ? 1 : 0) : 0;
    case Kind::Pin: case Kind::Value: return v.value;
    }
    return 0;
}

static bool isBool(Kind k) { return k == Kind::Bool || k == Kind::Pin || k == Kind::PinIn; }

void Core::write(Var& v, double x) {
    switch (v.kind) {
    case Kind::Bool: *(bool*)v.ptr = x != 0; break;
    case Kind::Int: *(int*)v.ptr = (int)lround(x); break;
    case Kind::Long: *(long*)v.ptr = lround(x); break;
    case Kind::Float: *(float*)v.ptr = (float)x; break;
    case Kind::Double: *(double*)v.ptr = x; break;
    case Kind::Pin: v.value = x != 0 ? 1 : 0; if (pinWrite) pinWrite(v.pin, x != 0); break;
    case Kind::Value: v.value = x; break;
    case Kind::PinIn: return;
    }
    if (v.onWrite) v.onWrite(isBool(v.kind) ? (x != 0 ? 1 : 0) : x);
}

namespace {
struct Out {
    char* p; size_t cap; size_t n = 0; bool ok = true;
    Out(char* p, size_t cap) : p(p), cap(cap) {}
    void s(const char* t) { size_t l = strlen(t); if (n + l >= cap) { ok = false; return; } memcpy(p + n, t, l); n += l; p[n] = 0; }
    void num(double v) { char b[32]; size_t l = formatNumber(v, b, sizeof b); if (!l) { ok = false; return; } s(b); }
    size_t done() { return ok ? n : 0; }
};
}

static const char* typeLetter(Kind k) {
    switch (k) {
    case Kind::Bool: case Kind::Pin: case Kind::PinIn: return "b";
    case Kind::Int: case Kind::Long: return "i";
    default: return "f";
    }
}

size_t Core::hello(char* out, size_t cap, const char* lib, const char* board) const {
    Out o(out, cap);
    o.s("{\"t\":\"hello\",\"lib\":\""); o.s(lib); o.s("\",\"board\":\""); o.s(board); o.s("\",\"vars\":[");
    for (size_t i = 0; i < n; i++) {
        if (i) o.s(",");
        o.s("{\"n\":\""); o.s(vars[i].name); o.s("\",\"y\":\""); o.s(typeLetter(vars[i].kind)); o.s("\"}");
    }
    o.s("]}");
    return o.done();
}

size_t Core::values(char* out, size_t cap) {
    Out o(out, cap);
    o.s("{\"t\":\"v\",\"v\":{");
    for (size_t i = 0; i < n; i++) {
        double x = read(vars[i]);
        if (i) o.s(",");
        o.s("\""); o.s(vars[i].name); o.s("\":");
        if (isBool(vars[i].kind)) o.s(x != 0 ? "true" : "false"); else o.num(x);
    }
    o.s("}}");
    if (!o.ok) return 0;
    for (size_t i = 0; i < n; i++) { vars[i].value = read(vars[i]); vars[i].sent = true; }
    return o.done();
}

bool Core::handle(const char* json, size_t len) {
    char t[12], buf[100];
    if (!jsonField(json, len, "t", t, sizeof t)) return false;
    if (!strcmp(t, "rate")) {
        double ms;
        if (!jsonField(json, len, "ms", buf, sizeof buf) || !parseNumber(buf, ms)) return false;
        rateMs = ms < WATCHED_MS ? WATCHED_MS : ms > 86400000.0 ? 86400000UL : (uint32_t)ms;
        return true;
    }
    if (!strcmp(t, "w")) {
        char name[Var::NAME + 2];
        double x;
        if (!jsonField(json, len, "n", name, sizeof name) || !jsonField(json, len, "v", buf, sizeof buf) || !parseNumber(buf, x)) return false;
        Var* v = find(name);
        if (!v || v->kind == Kind::PinIn) return false;
        write(*v, x);
        return true;
    }
    if (!strcmp(t, "err")) {
        if (!jsonField(json, len, "code", errorCode, sizeof errorCode)) errorCode[0] = 0;
        if (!strcmp(errorCode, "wait")) {
            double ms;
            holdMs = jsonField(json, len, "ms", buf, sizeof buf) && parseNumber(buf, ms) && ms > 0 ? (ms > 3600000.0 ? 3600000UL : (uint32_t)ms) : rateMs;
            errorCode[0] = 0;
            return true;
        }
        if (!jsonField(json, len, "msg", errorMessage, sizeof errorMessage)) errorMessage[0] = 0;
        return true;
    }
    return false;
}

uint32_t Core::due(uint32_t now) const {
    if (!everSent) return 0;
    uint32_t elapsed = now - lastSent;
    if (elapsed >= rateMs) return 0;
    return rateMs - elapsed;
}

void Core::sent(uint32_t now) { lastSent = now; everSent = true; }

bool Core::boolChanged() const {
    for (size_t i = 0; i < n; i++)
        if (isBool(vars[i].kind) && vars[i].sent && read(vars[i]) != vars[i].value) return true;
    return false;
}

bool Core::takeReserve(uint32_t now) {
    if (now - reserveHour >= 3600000UL) { reserveHour = now; reserveUsed = 0; }
    if (reserveUsed >= RESERVE_PER_HOUR) return false;
    reserveUsed++;
    return true;
}

}
