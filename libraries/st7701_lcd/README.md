# ST7701 + GT911 Display Library

Easy-to-use library for ST7701 MIPI-DSI display with GT911 touch support for ESP32-P4.

## Features

- **One-line initialization** - Get your display and touch running with just one function call
- **LVGL integration** - Full LVGL support with touch input
- **Arduino compatible** - Works with Arduino framework
- **ESP32-P4 optimized** - Specifically designed for ESP32-P4 with MIPI-DSI
- **C++ and C API** - Use from C++ or C code

## Quick Start

### C++ Usage

```cpp
#include <ST7701_GT911.h>

void setup() {
    // Initialize display and touch with one line!
    ST7701_GT911::begin(0, 80);  // 0° rotation, 80% backlight
    
    // Initialize LVGL
    ST7701_GT911::initLVGL();
    
    // Create your LVGL UI
    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "Hello World!");
    lv_obj_center(label);
}

void loop() {
    ST7701_GT911::lvglTask();
    delay(5);
}
```

### C Usage

```c
#include "ST7701_GT911.h"

void setup() {
    // Initialize display and touch with one line!
    st7701_gt911_init(0, 80);  // 0° rotation, 80% backlight
    
    // Initialize LVGL
    st7701_gt911_init_lvgl();
    
    // Create your LVGL UI
    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "Hello World!");
    lv_obj_center(label);
}

void loop() {
    st7701_gt911_lvgl_task();
    delay(5);
}
```

## API Reference

### C++ API

#### `ST7701_GT911::begin(rotation, backlight)`
Initialize the display and touch with default settings.
- `rotation`: Display rotation in degrees (0, 90, 180, 270)
- `backlight`: Backlight brightness (0-100%)
- Returns: `true` on success, `false` on failure

#### `ST7701_GT911::initLVGL()`
Initialize LVGL with the display and touch.
- Returns: `true` on success, `false` on failure

#### `ST7701_GT911::lvglTask()`
Call this in your main loop to handle LVGL tasks.

#### `ST7701_GT911::setBacklight(percent)`
Set backlight brightness.
- `percent`: Brightness level (0-100%)

#### `ST7701_GT911::setRotation(degrees)`
Set display rotation.
- `degrees`: Rotation in degrees (0, 90, 180, 270)

#### `ST7701_GT911::getTouch(x, y)`
Get touch coordinates.
- `x`, `y`: Pointers to store touch coordinates
- Returns: `true` if touched, `false` if not

#### `ST7701_GT911::fillScreen(color)`
Fill the entire screen with a color.
- `color`: 16-bit color value

#### `ST7701_GT911::drawBitmap(x, y, w, h, color_data)`
Draw a bitmap on the display.
- `x`, `y`: Position
- `w`, `h`: Width and height
- `color_data`: 16-bit color data array

#### `ST7701_GT911::getDisplay()`
Get the LVGL display handle.
- Returns: `lv_disp_t*` or `nullptr` if not initialized

#### `ST7701_GT911::getInputDevice()`
Get the LVGL input device handle.
- Returns: `lv_indev_t*` or `nullptr` if not initialized

#### `ST7701_GT911::getWidth()` / `ST7701_GT911::getHeight()`
Get display dimensions.
- Returns: Width/height in pixels

#### `ST7701_GT911::isInitialized()`
Check if the display is initialized.
- Returns: `true` if initialized

### C API

- `bool st7701_gt911_init(int rotation, int backlight)`
- `bool st7701_gt911_init_lvgl(void)`
- `void st7701_gt911_lvgl_task(void)`
- `void st7701_gt911_set_backlight(int percent)`
- `void st7701_gt911_set_rotation(int degrees)`
- `bool st7701_gt911_get_touch(uint16_t* x, uint16_t* y)`
- `void st7701_gt911_fill_screen(uint16_t color)`
- `void st7701_gt911_draw_bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t* color_data)`
- `lv_disp_t* st7701_gt911_get_display(void)`
- `lv_indev_t* st7701_gt911_get_input_device(void)`
- `uint16_t st7701_gt911_get_width(void)`
- `uint16_t st7701_gt911_get_height(void)`
- `bool st7701_gt911_is_initialized(void)`

## Hardware Configuration

The library uses these default pin assignments for ESP32-P4:

- **LCD Reset**: GPIO21
- **Touch SDA**: GPIO7
- **Touch SCL**: GPIO8
- **Touch Reset**: GPIO20
- **Touch Interrupt**: GPIO19

## Requirements

- ESP32-P4
- LVGL library
- Arduino framework

## Installation

1. Copy this library to your `libraries` folder
2. Include in your `platformio.ini`:
   ```ini
   lib_deps = 
       lvgl/lvgl @ ^8.4.0
   lib_ignore = 
       jc3248w535  # Exclude S3-specific libraries
   ```

## Examples

See the `examples/simple` folder for a complete working example.

## License

This library is part of the PoolLab project.


