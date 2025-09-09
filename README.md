
# ESP32-C6 Waveshare Touch — Tuya Sniffer + On-Screen Viewer

**What this does**
- Sniffs UART between your host MCU and Tuya CB3S (both directions).
- Parses Tuya `0x55 0xAA` frames.
- Shows the latest frames on the **onboard 1.47" LCD** and prints to USB Serial.

**Board**
- Waveshare **ESP32-C6-Touch-LCD-1.47** (JD9853 LCD + AXS5106L touch). Uses Arduino_GFX to drive the LCD as ST7789 with proper offsets.
- Pins per Waveshare wiki: DC=45, CS=21, SCK=38, MOSI=39, RST=47, size 172x320, col offset 34.

**Wiring (sniff-only)**
- GND ↔ GND
- MCU → CB3S **RXD1 (pin 15)** → ESP32-C6 **RX_A (GPIO 4)**
- CB3S **TXD1 (pin 16)** → MCU → ESP32-C6 **RX_B (GPIO 5)** *(optional)*
- Do NOT connect ESP32 TX pins while sniffing.

**Build**
- Open in PlatformIO (VS Code).
- Select the `waveshare-esp32-c6-touch` environment.
- Serial monitor 115200.
- If output looks like gibberish, set `TUYA_BAUD` in `src/main.cpp` to **115200** and rebuild.

**Libraries**
- Uses `GFX Library for Arduino` (moononournation / Arduino_GFX). PlatformIO will fetch it automatically via `lib_deps`.

**Next steps**
- To emulate a Tuya module, route UART lines to ESP32 RX/TX, implement Tuya handshake and DP query, then show values nicely with LVGL.
