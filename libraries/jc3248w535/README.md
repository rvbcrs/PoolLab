# JC3248W535 Display Library

Easy-to-use library for the JC3248W535 3.5" capacitive touch display with LVGL support.

## Features

- **One-line initialization** - Get your display running with just one function call
- **LVGL integration** - Full LVGL support with touch input
- **Arduino compatible** - Works with Arduino framework
- **ESP32-S3 optimized** - Specifically designed for ESP32-S3
- **C++ and C API** - Use from C++ or C code

## Quick Start

### C++ Usage

```cpp
#include <JC3248W535.h>

void setup() {
    // Initialize display with one line!
    JC3248W535::begin(0, 80);  // 0° rotation, 80% backlight
    
    // Create your LVGL UI
    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "Hello World!");
    lv_obj_center(label);
}

void loop() {
    lv_timer_handler();
    delay(5);
}
```

### C Usage

```c
#include "JC3248W535.h"

void setup() {
    // Initialize display with one line!
    jc3248w535_init(0, 80);  // 0° rotation, 80% backlight
    
    // Create your LVGL UI
    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "Hello World!");
    lv_obj_center(label);
}

void loop() {
    lv_timer_handler();
    delay(5);
}
```

## API Reference

### C++ API

#### `JC3248W535::begin(rotation, backlight)`
Initialize the display with default settings.
- `rotation`: Display rotation in degrees (0, 90, 180, 270)
- `backlight`: Backlight brightness (0-100%)
- Returns: `true` on success, `false` on failure

#### `JC3248W535::setBacklight(percent)`
Set backlight brightness.
- `percent`: Brightness level (0-100%)

#### `JC3248W535::lock(timeout_ms)`
Lock the display for thread-safe operations.
- `timeout_ms`: Maximum time to wait for lock
- Returns: `true` if locked successfully

#### `JC3248W535::unlock()`
Unlock the display.

#### `JC3248W535::getDisplay()`
Get the LVGL display handle.
- Returns: `lv_disp_t*` or `nullptr` if not initialized

#### `JC3248W535::getInputDevice()`
Get the LVGL input device handle.
- Returns: `lv_indev_t*` or `nullptr` if not initialized

#### `JC3248W535::isInitialized()`
Check if the display is initialized.
- Returns: `true` if initialized

### C API

- `bool jc3248w535_init(int rotation, int backlight)`
- `void jc3248w535_set_backlight(int percent)`
- `bool jc3248w535_lock(uint32_t timeout_ms)`
- `void jc3248w535_unlock(void)`
- `lv_disp_t* jc3248w535_get_display(void)`
- `lv_indev_t* jc3248w535_get_input_device(void)`
- `bool jc3248w535_is_initialized(void)`

## Advanced Usage

For advanced configuration, you can use the full BSP API:

```cpp
#include <JC3248W535.h>
#include "jc3248w535.h"

void setup() {
    // Custom configuration
    jc3248w535_config_t config = JC3248W535_DEFAULT_CONFIG(LV_DISP_ROT_90);
    config.buffer_size = 320 * 240;  // Custom buffer size
    config.backlight_percent = 50;   // 50% backlight
    
    jc3248w535_handles_t handles;
    if (JC3248W535::begin(&config, &handles)) {
        Serial.println("Display initialized with custom config");
    }
}
```

## Requirements

- ESP32-S3
- LVGL library
- Arduino framework

## Installation

1. Copy this library to your `libraries` folder
2. Include in your `platformio.ini`:
   ```ini
   lib_deps = 
       lvgl/lvgl @ ^8.4.0
   lib_ignore = 
       gt911_touch  # Exclude P4-specific libraries
   ```

## Examples

See the `examples/simple` folder for a complete working example.

## License

This library is part of the PoolLab project.










