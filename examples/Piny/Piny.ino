// Melior Specto bez proměnných: relé na pinu 5 ovládané z odkazu, stav tlačítka na pinu 4 na odkaz.
#include <MeliorSpecto.h>
#include "secrets.h"

void setup() {
  Specto.begin(WIFI_SSID, WIFI_PASS, SPECTO_KEY);
  Specto.output("rele", 5);      // zápis z odkazu přímo na pin
  Specto.input("tlacitko", 4);   // stav pinu na odkaz (vnitřní pull-up)
}

void loop() {
  Specto.loop();
}
