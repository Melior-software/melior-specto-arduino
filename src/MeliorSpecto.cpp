// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Daniel Kuběna (Melior) – see LICENSE and NOTICE
#include "MeliorSpecto.h"
#include "specto/SpectoRoots.h"

#if defined(SPECTO_NET_ESP32)
#include <WiFi.h>
#include <WiFiClientSecure.h>
static WiFiClientSecure tls;
static void tlsSetup() { tls.setCACert(SPECTO_ROOTS); tls.setHandshakeTimeout(15); }
#elif defined(SPECTO_NET_ESP8266)
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
static BearSSL::WiFiClientSecure tls;
static BearSSL::X509List* anchors = nullptr;
static void tlsSetup() {
    if (!anchors) anchors = new BearSSL::X509List(SPECTO_ROOTS_EC);
    tls.setTrustAnchors(anchors);
    tls.setBufferSizes(4096, 512);
}
#elif defined(SPECTO_NET_PICOW)
#include <WiFi.h>
#include <WiFiClientSecure.h>
static WiFiClientSecure tls;
static BearSSL::X509List* anchors = nullptr;
static void tlsSetup() {
    if (!anchors) anchors = new BearSSL::X509List(SPECTO_ROOTS);
    tls.setTrustAnchors(anchors);
}
#elif defined(SPECTO_NET_WIFIS3)
#include <WiFiS3.h>
#include <WiFiSSLClient.h>
static WiFiSSLClient tls;
static void tlsSetup() { tls.setCACert(SPECTO_ROOTS); }
#elif defined(SPECTO_NET_WIFIC3)
#include <WiFiC3.h>
#include <WiFiSSLClient.h>
static WiFiSSLClient tls;
static void tlsSetup() { tls.setCACert(SPECTO_ROOTS); }
#elif defined(SPECTO_NET_NINA)
#include <WiFiNINA.h>
static WiFiSSLClient tls;
static void tlsSetup() {}
#elif defined(SPECTO_NET_WIFI101)
#include <WiFi101.h>
static WiFiSSLClient tls;
static void tlsSetup() {}
#elif defined(SPECTO_NET_MBED)
#include <WiFi.h>
#include <WiFiSSLClient.h>
static WiFiSSLClient tls;
static void tlsSetup() {}
#endif

MeliorSpectoClass Specto;

static char outBuf[SPECTO_BUFFER];
static char inBuf[SPECTO_IN_BUFFER];

static void pinWriteFn(int pin, bool on) { digitalWrite(pin, on ? HIGH : LOW); }
static int pinReadFn(int pin) { return digitalRead(pin); }

static const uint32_t PING_MS = 30000;
static const uint32_t SILENT_MS = 90000;
static const uint32_t WIFI_RETRY_MS = 15000;

#if SPECTO_HAS_WIFI
void MeliorSpectoClass::begin(const char* ssid, const char* password, const char* key) {
    this->ssid = ssid;
    this->password = password;
    tlsSetup();
    begin(tls, key);
    state = State::Wifi;
    wifiAsked = 0;
}
#endif

void MeliorSpectoClass::begin(Client& tlsClient, const char* key) {
    client = &tlsClient;
    io.c = client;
    this->key = key;
    core.pinWrite = pinWriteFn;
    core.pinRead = pinReadFn;
    randomSeed(micros());
    state = State::Connect;
    helloDue = true;
}

void MeliorSpectoClass::begin(Stream& bridge) {
    this->bridge = &bridge;
    core.pinWrite = pinWriteFn;
    core.pinRead = pinReadFn;
    state = State::Online;
    bridgeOnline = false;
    helloDue = true;
}

bool MeliorSpectoClass::add(const char* name, specto::Kind kind, void* ptr, int pin) {
    if (!core.add(name, kind, ptr, pin)) return false;
    helloDue = true;
    return true;
}

bool MeliorSpectoClass::link(const char* name, bool& v) { return add(name, specto::Kind::Bool, &v); }
bool MeliorSpectoClass::link(const char* name, int& v) { return add(name, specto::Kind::Int, &v); }
bool MeliorSpectoClass::link(const char* name, long& v) { return add(name, specto::Kind::Long, &v); }
bool MeliorSpectoClass::link(const char* name, float& v) { return add(name, specto::Kind::Float, &v); }
bool MeliorSpectoClass::link(const char* name, double& v) { return add(name, specto::Kind::Double, &v); }

bool MeliorSpectoClass::output(const char* name, int pin) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    return add(name, specto::Kind::Pin, nullptr, pin);
}

bool MeliorSpectoClass::input(const char* name, int pin, int mode) {
    pinMode(pin, mode);
    return add(name, specto::Kind::PinIn, nullptr, pin);
}

void MeliorSpectoClass::send(const char* name, double value) {
    specto::Var* v = core.find(name);
    if (!v) { if (!add(name, specto::Kind::Value, nullptr)) return; v = core.find(name); }
    if (v->kind == specto::Kind::Value) v->value = value;
}

double MeliorSpectoClass::receive(const char* name) {
    specto::Var* v = core.find(name);
    if (!v) { add(name, specto::Kind::Value, nullptr); return 0; }
    return core.read(*v);
}

bool MeliorSpectoClass::onWrite(const char* name, void (*callback)(double)) {
    specto::Var* v = core.find(name);
    if (!v) return false;
    v->onWrite = callback;
    return true;
}

void MeliorSpectoClass::drop(uint32_t retryMs) {
    if (client) client->stop();
    state = State::Wait;
    waitUntil = millis() + retryMs;
}

void MeliorSpectoClass::connect() {
    if (!client->connect(host, port)) {
        drop(backoff);
        backoff = backoff < 60000 ? backoff * 2 : 60000;
        return;
    }
    if (!useHttp && !ws.handshake(host, "/v1/board/ws", key)) {
        int st = ws.status();
        bool keyRefused = st == 400 || st == 401 || st == 403;
#if !defined(SPECTO_SMALL)
        if (!keyRefused && ++wsFails >= 2) useHttp = true;
#endif
        drop(keyRefused ? 300000UL : backoff);
        backoff = backoff < 60000 ? backoff * 2 : 60000;
        return;
    }
    backoff = 5000;
    core.errorCode[0] = 0;
    core.errorMessage[0] = 0;
    state = State::Online;
    helloDue = true;
    lastPing = millis();
}

bool MeliorSpectoClass::out(const char* s, size_t n) {
    if (bridge) {
        if (!bridgeOnline) return false;
        bridge->write((const uint8_t*)s, n);
        bridge->write('\n');
        return true;
    }
    return ws.sendText(s, n);
}

void MeliorSpectoClass::httpExchange(uint32_t now, bool values) {
#if defined(SPECTO_SMALL)
    (void)now; (void)values;
    drop(backoff);
#else
    size_t n = 0;
    if (helloDue) {
        n = core.hello(outBuf, sizeof outBuf - 2, MELIOR_SPECTO_VERSION, SPECTO_BOARD);
        if (n) outBuf[n++] = '\n';
    }
    size_t v = values ? core.values(outBuf + n, sizeof outBuf - n - 1) : 0;
    n += v;
    if (!client->connected() && !client->connect(host, port)) { drop(backoff); return; }
    char* answer = outBuf;
    size_t len = 0;
    bool keep = false;
    int st = http.post(host, "/v1/board/link", key, outBuf, n, answer, sizeof outBuf, len, keep);
    if (st == 400 || st == 401 || st == 403) { drop(300000UL); return; }
    if (st != 200) { drop(backoff); backoff = backoff < 60000 ? backoff * 2 : 60000; return; }
    helloDue = false;
    if (v) core.sent(now);
    for (size_t a = 0, b = 0; b <= len; b++) {
        if (b == len || answer[b] == '\n') {
            if (b > a && b - a < sizeof inBuf) { memcpy(inBuf, answer + a, b - a); received(inBuf, b - a, now); }
            a = b + 1;
        }
    }
    if (!keep) client->stop();
#endif
}

void MeliorSpectoClass::sendValues(uint32_t now) {
    if (useHttp) { httpExchange(now, true); return; }
    size_t n = core.values(outBuf, sizeof outBuf);
    if (n && out(outBuf, n)) core.sent(now);
}

void MeliorSpectoClass::received(char* msg, size_t len, uint32_t now) {
    msg[len] = 0;
    if (bridge && strstr(msg, "\"t\":\"bridge\"")) {
        bridgeOnline = strstr(msg, "\"online\"") != nullptr;
        if (bridgeOnline) helloDue = true;
        return;
    }
    core.handle(msg, len);
    if (core.holdMs) { core.hold(now, core.holdMs); core.holdMs = 0; }
}

void MeliorSpectoClass::loop() {
    uint32_t now = millis();
    switch (state) {
    case State::Idle:
        return;
    case State::Wifi:
#if SPECTO_HAS_WIFI
        if (WiFi.status() == WL_CONNECTED) { state = State::Connect; return; }
        if (!wifiAsked || now - wifiAsked > WIFI_RETRY_MS) {
            wifiAsked = now ? now : 1;
            WiFi.begin(ssid, password);
        }
#endif
        return;
    case State::Wait:
        if ((int32_t)(now - waitUntil) < 0) return;
        state = State::Connect;
    case State::Connect:
#if SPECTO_HAS_WIFI
        if (ssid && WiFi.status() != WL_CONNECTED) { state = State::Wifi; return; }
#endif
        connect();
        return;
    case State::Online:
        break;
    }

    if (bridge) {
        while (bridge->available() > 0) {
            int c = bridge->read();
            if (c < 0) break;
            if (c == '\n' || c == '\r') {
                if (lineLen && lineLen < sizeof inBuf) received(inBuf, lineLen, now);
                lineLen = 0;
            } else if (lineLen < sizeof inBuf - 1) {
                inBuf[lineLen++] = (char)c;
            } else {
                lineLen = sizeof inBuf;
            }
        }
    } else if (useHttp) {
        if (helloDue) httpExchange(now, false);
    } else {
        if (!io.connected() || ws.closed() || now - ws.lastHeard() > SILENT_MS) {
            const char* c = core.errorCode;
            bool onPurpose = !strcmp(c, "paused") || !strcmp(c, "limit") || !strcmp(c, "blocked") || !strcmp(c, "refused");
            drop(onPurpose ? 300000UL : backoff);
            return;
        }
        size_t len;
        while (ws.poll(inBuf, sizeof inBuf - 1, len)) received(inBuf, len, now);
    }
    if (helloDue && !useHttp) {
        size_t n = core.hello(outBuf, sizeof outBuf, MELIOR_SPECTO_VERSION, bridge ? SPECTO_BOARD "+bridge" : SPECTO_BOARD);
        if (n && out(outBuf, n)) helloDue = false;
    }
    if (core.due(now) == 0) {
        sendValues(now);
    } else if (core.rateMs > specto::Core::WATCHED_MS && core.boolChanged() && core.takeReserve(now)) {
        sendValues(now);
    }
    if (!bridge && !useHttp && now - lastPing > PING_MS) { ws.sendPing(); lastPing = now; }
}

const char* MeliorSpectoClass::status() const {
    if (core.errorMessage[0]) return core.errorMessage;
    switch (state) {
    case State::Idle: return "Nespuštěno (zavolej Specto.begin)";
    case State::Wifi: return "Připojuji Wi‑Fi…";
    case State::Connect: return "Připojuji se k serveru…";
    case State::Wait: return ws.status() == 400 || ws.status() == 401 || ws.status() == 403 ? "Server odmítl klíč desky (zkontroluj ho na webu)" : "Čekám na další pokus o připojení…";
    case State::Online: return bridge && !bridgeOnline ? "Čekám na most (ESP)…" : useHttp ? "Online (přes HTTPS)" : "Online";
    }
    return "";
}

#if SPECTO_HAS_WIFI
MeliorSpectoBridgeClass SpectoBridge;

static char bridgeLine[SPECTO_BUFFER];
static char bridgeIn[SPECTO_BUFFER];

void MeliorSpectoBridgeClass::begin(const char* ssid, const char* password, const char* key, Stream& board) {
    this->ssid = ssid;
    this->password = password;
    this->key = key;
    this->board = &board;
    tlsSetup();
    io.c = &tls;
    randomSeed(micros());
}

void MeliorSpectoBridgeClass::loop() {
    uint32_t now = millis();
    if (!online) {
        if (WiFi.status() != WL_CONNECTED) {
            if (!wifiAsked || now - wifiAsked > WIFI_RETRY_MS) { wifiAsked = now ? now : 1; WiFi.begin(ssid, password); }
            return;
        }
        if ((int32_t)(now - waitUntil) < 0) return;
        if (tls.connect("specto.melior.software", 443) && ws.handshake("specto.melior.software", "/v1/board/ws", key)) {
            online = true;
            backoff = 5000;
            lastPing = now;
            board->print("{\"t\":\"bridge\",\"s\":\"online\"}\n");
        } else {
            tls.stop();
            int st = ws.status();
            waitUntil = now + (st == 400 || st == 401 || st == 403 ? 300000UL : backoff);
            backoff = backoff < 60000 ? backoff * 2 : 60000;
        }
        return;
    }
    if (!io.connected() || ws.closed() || now - ws.lastHeard() > SILENT_MS) {
        tls.stop();
        online = false;
        waitUntil = now + backoff;
        board->print("{\"t\":\"bridge\",\"s\":\"offline\"}\n");
        return;
    }
    while (board->available() > 0) {
        int c = board->read();
        if (c < 0) break;
        if (c == '\n' || c == '\r') {
            if (lineLen && lineLen < sizeof bridgeLine) ws.sendText(bridgeLine, lineLen);
            lineLen = 0;
        } else if (lineLen < sizeof bridgeLine) {
            bridgeLine[lineLen++] = (char)c;
        }
    }
    size_t len;
    while (ws.poll(bridgeIn, sizeof bridgeIn, len)) { board->write((const uint8_t*)bridgeIn, len); board->write('\n'); }
    if (now - lastPing > PING_MS) { ws.sendPing(); lastPing = now; }
}

const char* MeliorSpectoBridgeClass::status() const {
    if (online) return "Online";
    if (WiFi.status() != WL_CONNECTED) return "Připojuji Wi‑Fi…";
    return ws.status() == 400 || ws.status() == 401 || ws.status() == 403 ? "Server odmítl klíč desky (zkontroluj ho na webu)" : "Připojuji se k serveru…";
}
#endif
