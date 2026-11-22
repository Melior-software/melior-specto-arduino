// Melior Specto – most pro desku bez Wi‑Fi (UNO, Mega, Nano, Leonardo…).
// Nahraj do ESP32 nebo ESP8266. ESP drží Wi‑Fi, klíč a spojení se serverem, deska s vizualizací posílá
// a dostává zprávy po sériové lince (příklad UnoPresMost).
//
// Zapojení (ESP32): GPIO 16 (RX2) ← TX desky, GPIO 17 (TX2) → RX desky, GND ↔ GND.
// UNO má 5V logiku: na lince UNO TX → ESP RX dej dělič napětí (např. 1 kΩ a 2 kΩ), ESP snese jen 3,3 V.
#include <MeliorSpecto.h>
#include "secrets.h"

#if defined(ARDUINO_ARCH_ESP32)
#define DESKA Serial2
#else
#define DESKA Serial      // ESP8266: hardwarová linka (při nahrávání desku odpoj)
#endif

void setup() {
#if defined(ARDUINO_ARCH_ESP32)
  DESKA.begin(57600, SERIAL_8N1, 16, 17);
#else
  DESKA.begin(57600);
#endif
  SpectoBridge.begin(WIFI_SSID, WIFI_PASS, SPECTO_KEY, DESKA);
}

void loop() {
  SpectoBridge.loop();
}
