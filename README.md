## PoolLab — ESP32‑C6 Touch LCD pool monitor (Tuya sniffer, Zigbee/WiFi, MQTT)

### Wat het doet
- **Tuya sniffer**: leest de UART tussen je host‑MCU en Tuya CB3S, parseert `0x55 0xAA` frames en toont de laatste waarden op het 1.47" LCD en via USB‑serial.
- **Twee modi**: schakel tussen **Zigbee** en **WiFi/MQTT** via een UI‑toggle. De keuze wordt **persistent opgeslagen** (NVS) en bij boot toegepast.
- **MQTT integratie**: publiceert pH/ORP/temperatuur en biedt Home Assistant auto‑discovery + command topics om drempels te wijzigen.
- **Zigbee commissioning**: lange druk op BOOT (>3s) start commissioning; WiFi wordt tijdelijk uitgezet; na afloop wordt je vorige modus hersteld.

### Hardware/Board
- Getest op Waveshare **ESP32‑C6‑Touch‑LCD‑1.47**. LCD wordt als ST7789 aangestuurd met Arduino_GFX, 172x320 en kolom‑offset 34.
- De pinmapping voor het display staat in `src/main.cpp` (Arduino_GFX bus en ST7789 constructie). Pas deze aan als je een andere revisie hebt.

### Sniff‑bekabeling (alleen lezen)
- **GND ↔ GND**
- MCU → CB3S RXD1 (pin 15) → ESP32‑C6 `RX_A` (GPIO 4)
- CB3S TXD1 (pin 16) → MCU → ESP32‑C6 `RX_B` (GPIO 5) (optioneel)
- Verbind ESP32 TX niet wanneer je alleen wilt sniffen.

### Motor driver (SparkFun ROB‑14450 / TB6612FNG)
- Voeding en massa:
  - **VCC** ↔ ESP32 3V3 (logica‑voeding 3.3 V)
  - **GND** ↔ GND (gemeenschappelijke massa met ESP32 en motorspanning verplicht)
  - **VM** ↔ motorspanning (bijv. 6–12 V, afhankelijk van pomp‑motoren)
- Besturingspinnen naar ESP32‑C6 (zoals ingesteld in `src/main.cpp`):
  - **STBY** ↔ GPIO 3 (`TB_STBY`)
  - **AIN1** ↔ GPIO 7 (`M1_IN1`)
  - **AIN2** ↔ GPIO 8 (`M1_IN2`)
  - **PWMA** ↔ GPIO 5 (`M1_PWM`)
  - **BIN1** ↔ GPIO 4 (`M2_IN1`)
  - **BIN2** ↔ GPIO 6 (`M2_IN2`)
  - **PWMB** ↔ GPIO 9 (`M2_PWM`)
- Motoruitgangen:
  - **A01/A02** ↔ Motor 1 (pH‑pomp)
  - **B01/B02** ↔ Motor 2 (ORP‑pomp)

Notities
- Standaard PWM is 10 kHz (8‑bit). Snelheden per motor zijn instelbaar in de UI en worden opgeslagen in NVS.
- `GPIO 9` is een BOOT‑pin op veel C6‑boards. In de broncode staat `MOTOR_ENABLE = false` om boot‑conflicten te vermijden. Wil je de motorsturing inschakelen, zet `MOTOR_ENABLE` op `true` en overweeg `M2_PWM` naar een andere vrije PWM‑capabele GPIO te verplaatsen als je problemen ziet met booten.
- **STBY** moet hoog zijn om de driver te activeren; in deze setup wordt dit door de ESP32 via `TB_STBY` geregeld.

### Bouwen en flashen
- Open de map in PlatformIO (VS Code).
- Gebruik het environment `esp32-c6-devkitc-1` (zie `platformio.ini`).
- Seriële monitor: 115200 baud.
- CLI (optioneel):
  - Build: `pio run -e esp32-c6-devkitc-1`
  - Upload: `pio run -t upload -e esp32-c6-devkitc-1`

### Configuratie (WiFi/MQTT)
- WiFi via captive portal (aanbevolen):
  - Bij lege WiFi‑gegevens of na meerdere mislukte connect‑pogingen start automatisch een AP `PoolLab-Setup`.
  - Verbind met dat AP en open een willekeurige URL; het formulier “WiFi setup” verschijnt. Vul SSID/wachtwoord in en kies Save & Reboot.
  - De gegevens worden in NVS opgeslagen en bij boot gebruikt. Handmatig starten kan via Settings → Network → Configure WiFi.
- MQTT in `src/main.cpp` (handmatig aanpassen indien nodig):
  - `MQTT_HOST`, `MQTT_PORT`, `MQTT_USER`, `MQTT_PASS`, `MQTT_CLIENTID`

### OTA (Over‑the‑Air updates)
- OTA wordt automatisch geactiveerd nadat het device een IP heeft gekregen.
- Hostnaam: `poollab-XXXXXX` (laatste 3 bytes van chip‑ID). Upload via PlatformIO “Upload using network” of een OTA‑tool compatible met ArduinoOTA.

### Modi en persistentie
- De UI‑schakelaar roept `storage.setMode(...)` aan en schakelt direct radios:
  - Zigbee: WiFi wordt uitgezet (`WIFI_OFF`). Als je al gebonden was, wordt de Zigbee‑stack gestart.
  - WiFi/MQTT: WiFi STA wordt aangezet en MQTT gestart.
- Bij boot wordt de opgeslagen modus eerst uit NVS geladen, daarna pas WiFi/Zigbee gestart. Je keuze blijft dus behouden over reboots.
- Tijdens Zigbee‑commissioning (lange druk op BOOT) wordt de modemodus tijdelijk geforceerd naar Zigbee. Na afloop wordt je vorige modus hersteld en weggeschreven.

### MQTT
- Topics (states):
  - `pool/sensor/ph`
  - `pool/sensor/orp`
  - `pool/sensor/temp`
- Home Assistant discovery topics:
  - `homeassistant/sensor/pool_ph/config`
  - `homeassistant/sensor/pool_orp/config`
  - `homeassistant/sensor/pool_temp/config`
- Command/config topics (schrijven om drempels te zetten, config echo’t terug):
  - `pool/cmd/ph_min`  ↔ `pool/cfg/ph_min`
  - `pool/cmd/ph_max`  ↔ `pool/cfg/ph_max`
  - `pool/cmd/orp_min` ↔ `pool/cfg/orp_min`
  - `pool/cmd/orp_max` ↔ `pool/cfg/orp_max`

### WebUI (via WiFi)
- Home: tegels in donkere LVGL‑stijl met live updates (WebSocket) voor pH, ORP en Temp.
- Settings: Mode (Zigbee/WiFi), pH min/max, ORP min/max, motor‑snelheden; opslaan → NVS + directe applicatie.
- URLs:
  - `http://<ip>/`
  - `http://<ip>/settings`
  - WebSocket op poort 81 (client wordt automatisch geopend door de homepage).

### Projectstructuur (belangrijkste componenten)
- `src/main.cpp`: Orkestratie. UI‑handlers, bootvolgorde, WiFi/MQTT, Zigbee‑commissioning, Tuya‑feed, periodieke publish.
- `src/core/Storage.{h,cpp}`: Dunne wrapper rond `Preferences` (NVS). Slaat drempels, motorsnelheden en modemodus op (`poolcfg` namespace).
- `src/core/DisplayBridge.{h,cpp}`: Brug tussen Arduino_GFX en LVGL (display registratie, thema, input).
- `src/domain/Metrics.{h,cpp}`: Houdt laatste pH/ORP/temperatuur bij; bron voor UI/MQTT.
- `src/io/MqttClient.{h,cpp}`: PubSubClient‑gebaseerde client; discovery, state‑publish, command‑topics → NVS.
- `src/io/Tuya.{h,cpp}`: Configuratie en parsing van Tuya‑frames; voedt `Metrics`.
- `src/io/ZigbeeClient.{h,cpp}`: Abstractie voor Arduino Zigbee (indien beschikbaar); commissioning‑venster e.d.
- `src/ui/UI.{h,cpp}`: Eenvoudige LVGL UI met toggles/sliders; handlers configureerbaar vanuit `main.cpp`.
- `src/fonts`, `src/images`: UI assets.

### Troubleshooting
- WiFi “NO_AP_FOUND”: controleer SSID/ontvangst. De code probeert auto‑reconnect en periodieke hard restarts.
- “STA already disconnected”: informatief (safe to ignore) als WiFi al uit stond.
- NVS volledig wissen: gebruik “Erase Flash” in PlatformIO upload instellingen (let op: alle opgeslagen waardes weg).

### Dependencies (zie `platformio.ini`)
- `GFX Library for Arduino` (moononournation / Arduino_GFX)
- `Adafruit GFX Library`
- `PubSubClient`
- `lvgl`
- Zigbee libs via framework‑linker flags indien aanwezig
