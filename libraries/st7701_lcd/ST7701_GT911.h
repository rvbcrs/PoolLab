#pragma once

#include <Arduino.h>
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Configuration structure
typedef struct st7701_gt911_config_t {
    int lcd_rst_pin;
    int touch_sda_pin;
    int touch_scl_pin;
    int touch_rst_pin;
    int touch_int_pin;
    int rotation;
    int backlight_percent;
    uint32_t buffer_size;
} st7701_gt911_config_t;

// Handles structure
typedef struct st7701_gt911_handles_t {
    void* lcd;
    void* touch;
    void* lcd_handles;
    void* i2c_handle;
} st7701_gt911_handles_t;

#ifdef __cplusplus
}
#endif

class ST7701_GT911 {
public:
    // Simple initialization with default settings
    static bool begin(int rotation = 0, int backlight = 100);
    
    // Advanced initialization with custom config
    static bool begin(const st7701_gt911_config_t* config, st7701_gt911_handles_t* handles = nullptr);
    
    // Display functions
    static void setBacklight(int percent);
    static void setRotation(int degrees);
    static void fillScreen(uint16_t color);
    static void drawBitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t* color_data);
    
    // Touch functions
    static bool getTouch(uint16_t* x, uint16_t* y);
    static void setTouchRotation(int degrees);
    
    // LVGL integration - simplified, main.cpp handles the complex setup
    static void lvglTask();
    
    // Utility functions
    static bool lock(uint32_t timeout_ms = 1000);
    static void unlock();
    
    // Get handles for advanced usage
    static lv_disp_t* getDisplay();
    static lv_indev_t* getInputDevice();
    static void* getLCDHandle();
    static void* getTouchHandle();
    
    // Display info
    static uint16_t getWidth();
    static uint16_t getHeight();
    
    // Check if initialized
    static bool isInitialized();
    
private:
    static bool _initialized;
    static st7701_gt911_handles_t _handles;
    static lv_disp_t* _lvgl_display;
    static lv_indev_t* _lvgl_input;
};

// C API for compatibility
#ifdef __cplusplus
extern "C" {
#endif

// Simple C API
bool st7701_gt911_init(int rotation, int backlight);
void st7701_gt911_set_backlight(int percent);
void st7701_gt911_set_rotation(int degrees);
void st7701_gt911_fill_screen(uint16_t color);
void st7701_gt911_draw_bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t* color_data);
bool st7701_gt911_get_touch(uint16_t* x, uint16_t* y);
void st7701_gt911_set_touch_rotation(int degrees);
void st7701_gt911_lvgl_task(void);
bool st7701_gt911_lock(uint32_t timeout_ms);
void st7701_gt911_unlock(void);
lv_disp_t* st7701_gt911_get_display(void);
lv_indev_t* st7701_gt911_get_input_device(void);
void* st7701_gt911_get_lcd_handle(void);
void* st7701_gt911_get_touch_handle(void);
uint16_t st7701_gt911_get_width(void);
uint16_t st7701_gt911_get_height(void);
bool st7701_gt911_is_initialized(void);

#ifdef __cplusplus
}
#endif
