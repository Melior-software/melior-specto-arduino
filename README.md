# Melior Specto pro Arduino IDE

Vizualizace a ovládání tvé desky na odkazu `specto.melior.software/<název>` – bez aplikace Melior.
Projekt, desky, proměnné, obrazovku, PIN a interval nastavíš na [specto.melior.software](https://specto.melior.software/app/),
do sketche vložíš jen klíč.

## Rychlý start

1. Na specto.melior.software → **Začít tvořit** (účet Melior ID) → **＋ Nový projekt** → **Desky a nastavení** → **Přidat desku**. Zkopíruj klíč (ukáže se jen jednou).
2. V Arduino IDE otevři **Soubor → Příklady → Melior Specto → Zaklad**, do `secrets.h` napiš Wi‑Fi a klíč, nahraj.
3. Na stránce projektu u desky klikni **Převzít z desky**, nastav směr proměnných a ulož. Odkaz hned ukazuje hodnoty.
4. **Upravit obrazovku** otevře editor (měřáky, přepínače, trendy, alarmy, uživatelé s PINem…).

```cpp
#include <MeliorSpecto.h>
#include "secrets.h"

float teplota;
bool tl1;

void setup() {
  Specto.begin(WIFI_SSID, WIFI_PASS, SPECTO_KEY);
  Specto.link("teplota", teplota);
  Specto.link("tl1", tl1);
}

void loop() {
  teplota = analogRead(A0) * 0.1;
  digitalWrite(5, tl1);
  Specto.loop();
}
```

`secrets.h` obsahuje `WIFI_SSID`, `WIFI_PASS` a `SPECTO_KEY`. Proměnná `teplota` jde z desky na odkaz, `tl1` z odkazu
na desku (směr nastavíš na webu). `Specto.loop()` volej v každém průchodu `loop()`.

## Příklady

| Příklad | Co ukazuje |
|---|---|
| **Zaklad** | teplota na odkaz a tlačítko z odkazu na LED (ESP32 DevKit: LED na GPIO 2). Na webu vytvoř proměnné `teplota` (desetinné číslo, deska → odkaz) a `tl1` (bool, odkaz → deska). |
| **Piny** | bez proměnných: relé na pinu 5 ovládané z odkazu, tlačítko na pinu 4 (vnitřní pull-up) na odkaz |
| **VlastniPripojeni** | vlastní spojení s TLS, tady ESP32 s `WiFiClientSecure`. `setInsecure()` je jen pro ukázku, ve skutečnosti nastav kořenový certifikát. |
| **SpectoBridge** | most pro desku bez Wi‑Fi, nahraj do ESP32 nebo ESP8266. ESP drží Wi‑Fi, klíč a spojení se serverem. |
| **UnoPresMost** | deska bez Wi‑Fi (UNO, Nano…) přes most: jen proměnné, `SoftwareSerial` na pinech 10 (RX) a 11 (TX) |

Zapojení mostu (ESP32): GPIO 16 (RX2) ← TX desky, GPIO 17 (TX2) → RX desky, GND ↔ GND. UNO má 5V logiku:
na lince UNO TX → ESP RX dej dělič napětí (např. 1 kΩ a 2 kΩ), ESP snese jen 3,3 V. Na ESP8266 jde most přes
hardwarovou linku `Serial`, při nahrávání desku odpoj.

## Funkce

| Funkce | Co dělá |
|---|---|
| `Specto.begin(ssid, heslo, klic)` | připojí Wi‑Fi a server (desky s Wi‑Fi) |
| `Specto.begin(klient, klic)` | vlastní spojení s TLS (Ethernet, modem…) |
| `Specto.begin(linka)` | deska bez Wi‑Fi přes most v ESP (příklady SpectoBridge a UnoPresMost) |
| `Specto.link(nazev, promenna)` | propojí proměnnou `bool`, `int`, `long`, `float` nebo `double` |
| `Specto.output(nazev, pin)` | zápis z odkazu rovnou na pin |
| `Specto.input(nazev, pin)` | stav pinu na odkaz |
| `Specto.send(nazev, hodnota)` / `Specto.receive(nazev)` | hodnota bez propojené proměnné |
| `Specto.onWrite(nazev, funkce)` | zavolá funkci po zápisu z odkazu |
| `Specto.connected()`, `Specto.status()` | stav spojení (text česky, třeba pro `Serial.println`) |

Jak často se posílá, řídí server: **každou sekundu, když je odkaz otevřený**, jinak v intervalu z webu (nejméně
1 minuta). Když se změní přepínač (`bool`), pošle deska hodnotu hned (nejvýš 20× za hodinu). Rychlejší posílání
server odmítne.

## Desky

ESP32 (všechny varianty, Nano ESP32), ESP8266, UNO R4 WiFi, Raspberry Pi Pico W / 2 W, Nano 33 IoT,
MKR WiFi 1010, MKR1000, Nano RP2040 Connect, UNO WiFi Rev2, Portenta H7 a desky s vlastním TLS spojením.
UNO, Nano, Mega a Leonardo přes most v ESP32 / ESP8266. Na UNO a Nanu nejvýš 8 proměnných s názvy do 16 znaků.

Pro Nano 33 IoT, MKR WiFi 1010, Nano RP2040 Connect a UNO WiFi Rev2 nabídne Arduino IDE knihovnu WiFiNINA,
pro MKR1000 WiFi101. Portenta C33 potřebuje navíc knihovnu Arduino_SE05X (Správce knihoven).

Když síť nepustí WebSocket (některé firemní a školní proxy), přejde knihovna sama na HTTPS: funguje stejně, jen zápisy
z odkazu přijdou s dalším odesláním hodnot.

## Bezpečnost

Klíč patří jen do `secrets.h`; při sdílení sketche tento soubor vynech. Když klíč unikne, vytvoř na webu nový –
starý přestane platit hned. Zápisy z odkazu propustí server jen s PINem nebo přihlášeným uživatelem a jen
v typu a rozsahu nastaveném na webu.

## Licence

Apache License 2.0 – viz `LICENSE` a `NOTICE`. Knihovnu smíš používat, upravovat i zabudovat do vlastních
(i placených) výrobků; zachovej text licence a upozornění. Licence nedává právo používat názvy „Melior“
a „Melior Specto“ ani loga – upravenou verzi nevydávej za oficiální.
