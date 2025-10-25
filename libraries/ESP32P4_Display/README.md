# ESP32P4_Display Wrapper Library

Een eenvoudige wrapper library voor ESP32-P4 MIPI-DSI display (ST7701 + GT911) met LVGL ondersteuning.

## Features

- ✅ Eenvoudige initialisatie van ST7701 LCD driver
- ✅ GT911 touch controller ondersteuning  
- ✅ LVGL integratie met automatische buffer allocatie
- ✅ Backlight controle
- ✅ Rotatie ondersteuning
- ✅ Volledig getest en werkend

## Installatie

1. Kopieer de `ESP32P4_Display` library naar je project:
   ```bash
   cp -r libraries/ESP32P4_Display /path/to/your/project/libraries/
   ```

2. Voeg dependencies toe aan je `platformio.ini`:
   ```ini
   lib_deps = 
       lvgl/lvgl @ ^8.4.0
       ESP32P4_Display
       GT911 Touch Driver
       ST7701 LCD Driver
   
   build_flags = 
       -D ARDUINO_USB_MODE=1
       -D ARDUINO_USB_CDC_ON_BOOT=1
       -D LV_CONF_INCLUDE_SIMPLE
       -I src
       -I libraries/ESP32P4_Display
       -I libraries/st7701_lcd
       -I libraries/gt911_touch
       -D LCD_H_RES=480
       -D LCD_V_RES=800
       -D BOARD_ESP32P4_43
       -D CORE_DEBUG_LEVEL=3
   ```

3. Voeg `lv_conf.h` toe aan je `src/` directory (zie `examples/simple/` voor een voorbeeld)

## Gebruik

```cpp
#include <Arduino.h>
#include "ESP32P4_Display.h"
#include "lvgl.h"

ESP32P4_Display display;

void setup() {
    Serial.begin(115200);
    lv_init();
    
    // Hardware initialisatie
    if (!display.begin()) {
        Serial.println("Display init failed!");
        return;
    }
    
    // LVGL initialisatie
    lv_disp_t* disp = display.initLVGL(LV_DISP_ROT_270);
    if (!disp) {
        Serial.println("LVGL init failed!");
        return;
    }
    
    // Backlight instellen
    display.setBacklight(80);
    
    // Je LVGL UI code hier...
    lv_obj_t* label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "Hello World!");
    lv_obj_center(label);
}

void loop() {
    lv_timer_handler();
    delay(5);
}
```

## API

### `bool begin()`
Initialiseert de LCD en touch hardware. Retourneert `true` bij succes.

### `lv_disp_t* initLVGL(lv_disp_rot_t rotation)`
Initialiseert LVGL met de opgegeven rotatie. Retourneert display handle bij succes.

### `void setBacklight(uint8_t brightness)`
Stelt de backlight in (0-100%).

### `bsp_lcd_handles_t* getLCDHandles()`
Geeft toegang tot de LCD handles voor geavanceerd gebruik.

### `gt911_touch* getTouch()`
Geeft toegang tot de touch driver voor geavanceerd gebruik.

## Voorbeelden

Zie `examples/simple/` voor een volledig werkend voorbeeld.

## Hardware

- ESP32-P4 Function EV Board
- ST7701 MIPI-DSI LCD (480x800)
- GT911 touch controller
- I2C verbinding voor touch

## Licentie

MIT License