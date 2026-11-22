// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Daniel Kuběna (Melior) – see LICENSE and NOTICE
#pragma once

#if defined(ARDUINO_ARCH_ESP32)
#define SPECTO_NET_ESP32 1
#define SPECTO_BOARD "esp32"
#elif defined(ARDUINO_ARCH_ESP8266)
#define SPECTO_NET_ESP8266 1
#define SPECTO_BOARD "esp8266"
#elif defined(ARDUINO_UNOR4_WIFI)
#define SPECTO_NET_WIFIS3 1
#define SPECTO_BOARD "unor4wifi"
#elif defined(ARDUINO_PORTENTA_C33)
#define SPECTO_NET_WIFIC3 1
#define SPECTO_BOARD "portentac33"
#elif defined(ARDUINO_ARCH_RP2040) && !defined(ARDUINO_ARCH_MBED) && (defined(ARDUINO_RASPBERRY_PI_PICO_W) || defined(ARDUINO_RASPBERRY_PI_PICO_2W))
#define SPECTO_NET_PICOW 1
#define SPECTO_BOARD "picow"
#elif defined(ARDUINO_SAMD_NANO_33_IOT) || defined(ARDUINO_SAMD_MKRWIFI1010) || defined(ARDUINO_NANO_RP2040_CONNECT) || \
    defined(ARDUINO_SAMD_MKRVIDOR4000) || defined(ARDUINO_AVR_UNO_WIFI_REV2) || defined(ARDUINO_AVR_NANO_EVERY_WIFI)
#define SPECTO_NET_NINA 1
#define SPECTO_BOARD "nina"
#elif defined(ARDUINO_SAMD_MKR1000)
#define SPECTO_NET_WIFI101 1
#define SPECTO_BOARD "mkr1000"
#elif defined(ARDUINO_PORTENTA_H7_M7) || defined(ARDUINO_GIGA) || defined(ARDUINO_OPTA) || defined(ARDUINO_NICLA_VISION)
#define SPECTO_NET_MBED 1
#define SPECTO_BOARD "mbed"
#else
#define SPECTO_BOARD "other"
#endif

#if defined(SPECTO_NET_ESP32) || defined(SPECTO_NET_ESP8266) || defined(SPECTO_NET_WIFIS3) || defined(SPECTO_NET_WIFIC3) || \
    defined(SPECTO_NET_PICOW) || defined(SPECTO_NET_NINA) || defined(SPECTO_NET_WIFI101) || defined(SPECTO_NET_MBED)
#define SPECTO_HAS_WIFI 1
#else
#define SPECTO_HAS_WIFI 0
#endif

#ifndef SPECTO_BUFFER
#if defined(__AVR__) && !defined(__AVR_ATmega2560__) && !defined(__AVR_ATmega4809__)
#define SPECTO_BUFFER 336
#elif defined(__AVR__)
#define SPECTO_BUFFER 512
#else
#define SPECTO_BUFFER 1024
#endif
#endif

#ifndef SPECTO_IN_BUFFER
#if defined(SPECTO_SMALL)
#define SPECTO_IN_BUFFER 96
#else
#define SPECTO_IN_BUFFER (SPECTO_BUFFER / 2)
#endif
#endif
