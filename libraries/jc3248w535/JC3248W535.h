#pragma once

#include <Arduino.h>
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct jc3248w535_config_t jc3248w535_config_t;
typedef struct jc3248w535_handles_t jc3248w535_handles_t;

#ifdef __cplusplus
}
#endif

class JC3248W535 {
public:
    // Simple initialization with default settings
    static bool begin(int rotation = 0, int backlight = 100);
    
    // Advanced initialization with custom config
    static bool begin(const jc3248w535_config_t* config, jc3248w535_handles_t* handles = nullptr);
    
    // Utility functions
    static void setBacklight(int percent);
    static void setRotation(int degrees);
    static bool lock(uint32_t timeout_ms = 1000);
    static void unlock();
    
    // Get handles for advanced usage
    static lv_disp_t* getDisplay();
    static lv_indev_t* getInputDevice();
    
    // Check if initialized
    static bool isInitialized();
    
private:
    static bool _initialized;
    static jc3248W535_handles_t _handles;
};

// C API for compatibility
#ifdef __cplusplus
extern "C" {
#endif

// Simple C API
bool jc3248w535_init(int rotation, int backlight);
void jc3248w535_set_backlight(int percent);
void jc3248w535_set_rotation(int degrees);
bool jc3248w535_lock(uint32_t timeout_ms);
void jc3248w535_unlock(void);
lv_disp_t* jc3248w535_get_display(void);
lv_indev_t* jc3248w535_get_input_device(void);
bool jc3248w535_is_initialized(void);

#ifdef __cplusplus
}
#endif

