// Melior Specto na desce bez Wi‑Fi (UNO, Nano…) přes most v ESP (příklad SpectoBridge).
// Klíč a Wi‑Fi jsou v ESP, tady jen proměnné. Spojení: SoftwareSerial na pinech 10 (RX) a 11 (TX).
#include <MeliorSpecto.h>
#include <SoftwareSerial.h>

SoftwareSerial most(10, 11);
float teplota;
bool tl1;

void setup() {
  most.begin(57600);
  pinMode(LED_BUILTIN, OUTPUT);
  Specto.begin(most);
  Specto.link("teplota", teplota);
  Specto.link("tl1", tl1);
}

void loop() {
  teplota = analogRead(A0) * 0.1;
  digitalWrite(LED_BUILTIN, tl1);
  Specto.loop();
}
