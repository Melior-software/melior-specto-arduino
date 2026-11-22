// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Daniel Kuběna (Melior) – see LICENSE and NOTICE
#pragma once
#include <stddef.h>
#include <stdint.h>

#if defined(__AVR__) && !defined(__AVR_ATmega2560__) && !defined(__AVR_ATmega4809__)
#define SPECTO_SMALL 1
#endif
#ifndef SPECTO_MAX_VARS
#if defined(SPECTO_SMALL)
#define SPECTO_MAX_VARS 8
#else
#define SPECTO_MAX_VARS 32
#endif
#endif
#ifndef SPECTO_NAME_MAX
#if defined(SPECTO_SMALL)
#define SPECTO_NAME_MAX 16
#else
#define SPECTO_NAME_MAX 24
#endif
#endif

namespace specto {

enum class Kind : uint8_t { Bool, Int, Long, Float, Double, Pin, PinIn, Value };

typedef void (*WriteFn)(double value);
typedef void (*PinWriteFn)(int pin, bool on);
typedef int (*PinReadFn)(int pin);

struct Var {
    static const size_t NAME = SPECTO_NAME_MAX;
    char name[NAME + 1];
    Kind kind;
    void* ptr;
    int pin;
    double value;
    bool sent;
    WriteFn onWrite;
};

class Core {
public:
    static const size_t MAX_VARS = SPECTO_MAX_VARS;
    static const uint32_t WATCHED_MS = 1000;
    static const uint32_t IDLE_MS = 60000;
    static const uint8_t RESERVE_PER_HOUR = 20;

    PinWriteFn pinWrite = nullptr;
    PinReadFn pinRead = nullptr;

    bool add(const char* name, Kind kind, void* ptr, int pin = -1);
    Var* find(const char* name);
    size_t count() const { return n; }

    double read(const Var& v) const;
    void write(Var& v, double value);

    size_t hello(char* out, size_t cap, const char* lib, const char* board) const;
    size_t values(char* out, size_t cap);

    bool handle(const char* json, size_t len);

    uint32_t due(uint32_t now) const;
    void sent(uint32_t now);
    bool boolChanged() const;
    bool takeReserve(uint32_t now);

    uint32_t rateMs = WATCHED_MS;
    uint32_t holdMs = 0;
    void hold(uint32_t now, uint32_t ms) { lastSent = now + ms - rateMs; everSent = true; }
#if defined(SPECTO_SMALL)
    char errorCode[12] = {0};
    char errorMessage[40] = {0};
#else
    char errorCode[24] = {0};
    char errorMessage[96] = {0};
#endif

private:
    Var vars[MAX_VARS];
    size_t n = 0;
    uint32_t lastSent = 0;
    bool everSent = false;
    uint32_t reserveHour = 0;
    uint8_t reserveUsed = 0;
};

size_t formatNumber(double v, char* out, size_t cap);
bool jsonField(const char* json, size_t len, const char* key, char* out, size_t cap);
bool parseNumber(const char* s, double& v);

}
