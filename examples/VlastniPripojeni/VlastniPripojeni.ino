// Melior Specto přes vlastní spojení: jakýkoli Client s TLS (Ethernet + SSLClient, modem přes TinyGSM…).
// Tady pro ukázku ESP32 s WiFiClientSecure – Wi‑Fi si sketch řídí sám.
#include <MeliorSpecto.h>
#include "secrets.h"
#if defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <WiFiClientSecure.h>
WiFiClientSecure spojeni;
#endif

int pocet = 0;

void setup() {
#if defined(ARDUINO_ARCH_ESP32)
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  spojeni.setInsecure();          // jen pro ukázku – ve skutečnosti nastav kořenový certifikát
  Specto.begin(spojeni, SPECTO_KEY);
#endif
  Specto.link("pocet", pocet);
}

void loop() {
  static unsigned long t = 0;
  if (millis() - t > 1000) { t = millis(); pocet++; }
  Specto.loop();
}
