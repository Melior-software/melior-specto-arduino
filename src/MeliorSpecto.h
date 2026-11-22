// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Daniel Kuběna (Melior) – see LICENSE and NOTICE
#pragma once
#include <Arduino.h>
#include <Client.h>
#include "specto/SpectoCore.h"
#include "specto/SpectoWs.h"
#include "specto/SpectoHttp.h"
#include "specto/SpectoBoards.h"

#define MELIOR_SPECTO_VERSION "0.2.2"

class MeliorSpectoClass {
public:
#if SPECTO_HAS_WIFI
    void begin(const char* ssid, const char* password, const char* key);
#else
    template <class A, class B, class C> void begin(A, B, C) {
        static_assert(sizeof(A) == 0, "Melior Specto: tahle deska nema Wi-Fi s TLS. Pouzij Specto.begin(klient, klic) "
                                      "s vlastnim TLS spojenim, nebo desku pripoj pres ESP (priklad Bridge).");
    }
#endif
    void begin(Client& tlsClient, const char* key);
    void begin(Stream& bridge);

    bool link(const char* name, bool& variable);
    bool link(const char* name, int& variable);
    bool link(const char* name, long& variable);
    bool link(const char* name, float& variable);
    bool link(const char* name, double& variable);

    bool output(const char* name, int pin);
    bool input(const char* name, int pin, int mode = INPUT_PULLUP);

    void send(const char* name, double value);
    double receive(const char* name);
    bool onWrite(const char* name, void (*callback)(double value));

    void loop();

    bool connected() const { return state == State::Online && (!bridge || bridgeOnline); }
    const char* status() const;

    void setServer(const char* host, uint16_t port = 443) { this->host = host; this->port = port; }

private:
    enum class State : uint8_t { Idle, Wifi, Connect, Online, Wait };
    struct ClientIo : specto::Io {
        Client* c = nullptr;
        int available() override { return c ? c->available() : 0; }
        int read() override { return c ? c->read() : -1; }
        size_t write(const uint8_t* b, size_t n) override { return c ? c->write(b, n) : 0; }
        bool connected() override { return c && c->connected(); }
    };

    bool add(const char* name, specto::Kind kind, void* ptr, int pin = -1);
    void connect();
    void drop(uint32_t retryMs);
    void sendValues(uint32_t now);
    bool out(const char* s, size_t n);
    void received(char* msg, size_t len, uint32_t now);
    Stream* bridge = nullptr;
    bool bridgeOnline = false;
    size_t lineLen = 0;

    specto::Core core;
    ClientIo io;
    specto::Ws ws{io, specto_millis, specto_random};
    specto::Http http{io, specto_millis};
    bool useHttp = false;
    uint8_t wsFails = 0;
    void httpExchange(uint32_t now, bool values);
    Client* client = nullptr;
    const char* key = nullptr;
    const char* host = "specto.melior.software";
    uint16_t port = 443;
#if SPECTO_HAS_WIFI
    const char* ssid = nullptr;
    const char* password = nullptr;
    uint32_t wifiAsked = 0;
#endif
    State state = State::Idle;
    uint32_t waitUntil = 0;
    uint32_t backoff = 5000;
    uint32_t lastPing = 0;
    bool helloDue = true;

    static uint32_t specto_millis() { return millis(); }
    static uint32_t specto_random() { return (uint32_t)random(0x7FFFFFFF) ^ ((uint32_t)micros() << 16); }
};

extern MeliorSpectoClass Specto;

#if SPECTO_HAS_WIFI
class MeliorSpectoBridgeClass {
public:
    void begin(const char* ssid, const char* password, const char* key, Stream& board);
    void loop();
    bool connected() const { return online; }
    const char* status() const;
private:
    struct ClientIo : specto::Io {
        Client* c = nullptr;
        int available() override { return c ? c->available() : 0; }
        int read() override { return c ? c->read() : -1; }
        size_t write(const uint8_t* b, size_t n) override { return c ? c->write(b, n) : 0; }
        bool connected() override { return c && c->connected(); }
    };
    static uint32_t ms() { return millis(); }
    static uint32_t rnd() { return (uint32_t)random(0x7FFFFFFF) ^ ((uint32_t)micros() << 16); }
    ClientIo io;
    specto::Ws ws{io, ms, rnd};
    Stream* board = nullptr;
    const char* ssid = nullptr;
    const char* password = nullptr;
    const char* key = nullptr;
    bool online = false;
    uint32_t waitUntil = 0, backoff = 5000, wifiAsked = 0, lastPing = 0;
    size_t lineLen = 0;
};
extern MeliorSpectoBridgeClass SpectoBridge;
#endif
