// Melior Specto: teplota na odkaz, tlačítko z odkazu na LED.
// Na webu vytvoř proměnné "teplota" (desetinné číslo, deska → odkaz) a "tl1" (bool, odkaz → deska).
#include <MeliorSpecto.h>
#include "secrets.h"

#ifndef LED_BUILTIN
#define LED_BUILTIN 2        // ESP32 DevKit: modrá LED na GPIO 2
#endif
const int LED = LED_BUILTIN;
float teplota;
bool tl1;

void setup() {
  Serial.begin(115200);
  pinMode(LED, OUTPUT);
  Specto.begin(WIFI_SSID, WIFI_PASS, SPECTO_KEY);
  Specto.link("teplota", teplota);
  Specto.link("tl1", tl1);
}

void loop() {
  teplota = analogRead(A0) * 0.1;     // sem dej své měření
  digitalWrite(LED, tl1);

  Specto.loop();

  static unsigned long vypsano = 0;
  if (millis() - vypsano > 5000) { vypsano = millis(); Serial.println(Specto.status()); }
}
