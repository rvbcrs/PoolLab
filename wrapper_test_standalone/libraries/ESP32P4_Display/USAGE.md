# ESP32P4_Display Usage Guide

## Quick Start (3 steps)

### Step 1: Copy the library
Copy the `ESP32P4_Display` folder to your project's `libraries/` directory.

### Step 2: Add to platformio.ini
```ini
[env:esp32-p4-43]
platform = espressif32
board = esp32-p4-evboard
framework = arduino

lib_deps = 
    lvgl
    ESP32P4_Display

build_flags = 
    -I libraries/ESP32P4_Display
    -I libraries/st7701_lcd
    -I libraries/gt911_touch
    -DLCD_H_RES=480
    -DLCD_V_RES=800
```

### Step 3: Use in your code
```cpp
#include <Arduino.h>
#include "ESP32P4_Display.h"
#include "lvgl.h"

ESP32P4_Display display;

void setup() {
    lv_init();                                    // 1. Initialize LVGL
    display.begin();                              // 2. Initialize display hardware
    display.initLVGL(LV_DISP_ROT_270);           // 3. Initialize LVGL drivers
    
    // Your LVGL code here...
}

void loop() {
    lv_timer_handler();
    delay(5);
}
```

## What the wrapper does for you

### Before (complex setup):
```cpp
// 50+ lines of complex initialization code
// I2C setup
// LCD driver setup
// Touch driver setup
// LVGL buffer allocation
// LVGL display driver registration
// LVGL input driver registration
// Callback functions
// Error handling
// ... and much more
```

### After (simple wrapper):
```cpp
// Just 3 lines!
lv_init();
display.begin();
display.initLVGL(LV_DISP_ROT_270);
```

## Examples

### Minimal Example
```cpp
#include <Arduino.h>
#include "ESP32P4_Display.h"
#include "lvgl.h"

ESP32P4_Display display;

void setup() {
    lv_init();
    display.begin();
    display.initLVGL(LV_DISP_ROT_270);
    
    lv_obj_t* label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "Hello World!");
    lv_obj_center(label);
}

void loop() {
    lv_timer_handler();
    delay(5);
}
```

### With Touch Interaction
```cpp
#include <Arduino.h>
#include "ESP32P4_Display.h"
#include "lvgl.h"

ESP32P4_Display display;

void setup() {
    lv_init();
    display.begin();
    display.initLVGL(LV_DISP_ROT_270);
    
    // Create a button
    lv_obj_t* btn = lv_btn_create(lv_scr_act());
    lv_obj_center(btn);
    
    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, "Touch Me!");
    lv_obj_center(label);
    
    // Add click event
    lv_obj_add_event_cb(btn, [](lv_event_t* e) {
        Serial.println("Button clicked!");
    }, LV_EVENT_CLICKED, NULL);
}

void loop() {
    lv_timer_handler();
    delay(5);
}
```

## Advanced Usage

### Access to underlying drivers
```cpp
// Get LCD handles for advanced operations
bsp_lcd_handles_t* handles = display.getLCDHandles();

// Get touch driver for custom touch handling
gt911_touch* touch = display.getTouch();
```

### Backlight control
```cpp
display.setBacklight(50);  // 50% brightness
display.setBacklight(100); // 100% brightness
display.setBacklight(0);   // Off
```

### Different rotations
```cpp
display.initLVGL(LV_DISP_ROT_0);    // No rotation
display.initLVGL(LV_DISP_ROT_90);   // 90 degrees
display.initLVGL(LV_DISP_ROT_180);  // 180 degrees
display.initLVGL(LV_DISP_ROT_270);  // 270 degrees (default)
```

## Troubleshooting

### "Display not working"
- Make sure I2C is initialized before calling `display.begin()`
- Check pin connections in `pins_config_p4.h`

### "Touch not working"
- Verify GT911 touch controller connections
- Check I2C communication

### "LVGL crashes"
- Ensure PSRAM is available
- Check if LVGL buffers are allocated properly

### "Compilation errors"
- Make sure all required libraries are in `lib_deps`
- Check that build flags include the necessary paths
- Verify that `LCD_H_RES` and `LCD_V_RES` are defined

## Migration from manual setup

If you have existing code with manual display initialization:

1. **Remove** all manual LCD/touch initialization code
2. **Remove** all manual LVGL setup code
3. **Add** the 3 wrapper lines
4. **Keep** your existing LVGL UI code

The wrapper handles all the complex setup automatically!
