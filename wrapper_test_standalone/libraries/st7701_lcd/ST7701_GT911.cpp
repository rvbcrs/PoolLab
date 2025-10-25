#include "ST7701_GT911.h"
#include "st7701_lcd.h"
#include "gt911_touch.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

static const char *TAG = "ST7701_GT911";

// Default configuration
#define ST7701_GT911_DEFAULT_CONFIG() \
    (st7701_gt911_config_t){ \
        21,  /* lcd_rst_pin */ \
        7,   /* touch_sda_pin */ \
        8,   /* touch_scl_pin */ \
        20,  /* touch_rst_pin */ \
        19,  /* touch_int_pin */ \
        0,   /* rotation */ \
        100, /* backlight_percent */ \
        480 * 320  /* buffer_size */ \
    }

// Static members
bool ST7701_GT911::_initialized = false;
st7701_gt911_handles_t ST7701_GT911::_handles = {nullptr, nullptr, nullptr, nullptr};
lv_disp_t* ST7701_GT911::_lvgl_display = nullptr;
lv_indev_t* ST7701_GT911::_lvgl_input = nullptr;

bool ST7701_GT911::begin(int rotation, int backlight) {
    if (_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return true;
    }
    
    st7701_gt911_config_t config = ST7701_GT911_DEFAULT_CONFIG();
    config.rotation = rotation;
    config.backlight_percent = backlight;
    
    return begin(&config, &_handles);
}

bool ST7701_GT911::begin(const st7701_gt911_config_t* config, st7701_gt911_handles_t* handles) {
    if (_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return true;
    }
    
    if (!config) {
        ESP_LOGE(TAG, "Config is null");
        return false;
    }
    
    st7701_gt911_handles_t* target_handles = handles ? handles : &_handles;
    
    // Initialize I2C for touch
    i2c_master_bus_config_t i2c_cfg = {
        .i2c_port = I2C_NUM_1,
        .sda_io_num = (gpio_num_t)config->touch_sda_pin,
        .scl_io_num = (gpio_num_t)config->touch_scl_pin,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {.enable_internal_pullup = true},
    };
    
    esp_err_t ret = i2c_new_master_bus(&i2c_cfg, (i2c_master_bus_handle_t*)&target_handles->i2c_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Initialize LCD
    target_handles->lcd = new st7701_lcd(config->lcd_rst_pin);
    if (!target_handles->lcd) {
        ESP_LOGE(TAG, "Failed to create LCD instance");
        return false;
    }
    
    target_handles->lcd_handles = new bsp_lcd_handles_t();
    ((st7701_lcd*)target_handles->lcd)->begin();
    ((st7701_lcd*)target_handles->lcd)->get_handle((bsp_lcd_handles_t*)target_handles->lcd_handles);
    
    // Set backlight
    if (config->backlight_percent >= 0) {
        ((st7701_lcd*)target_handles->lcd)->example_bsp_set_lcd_backlight(config->backlight_percent);
    }
    
    // Initialize touch
    target_handles->touch = new gt911_touch(
        config->touch_sda_pin, 
        config->touch_scl_pin, 
        config->touch_rst_pin, 
        config->touch_int_pin
    );
    if (!target_handles->touch) {
        ESP_LOGE(TAG, "Failed to create touch instance");
        return false;
    }
    
    ((gt911_touch*)target_handles->touch)->begin();
    ((gt911_touch*)target_handles->touch)->set_rotation(config->rotation);
    
    // Store handles
    if (!handles) {
        _handles = *target_handles;
    }
    
    _initialized = true;
    ESP_LOGI(TAG, "Initialized successfully (rotation: %d°, backlight: %d%%)", 
             config->rotation, config->backlight_percent);
    
    return true;
}

void ST7701_GT911::setBacklight(int percent) {
    if (!_initialized) {
        ESP_LOGW(TAG, "Not initialized");
        return;
    }
    
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    
    ((st7701_lcd*)_handles.lcd)->example_bsp_set_lcd_backlight(percent);
    ESP_LOGI(TAG, "Backlight set to %d%%", percent);
}

void ST7701_GT911::setRotation(int degrees) {
    if (!_initialized) {
        ESP_LOGW(TAG, "Not initialized");
        return;
    }
    
    ((gt911_touch*)_handles.touch)->set_rotation(degrees);
    ESP_LOGI(TAG, "Rotation set to %d°", degrees);
}

void ST7701_GT911::fillScreen(uint16_t color) {
    if (!_initialized) {
        ESP_LOGW(TAG, "Not initialized");
        return;
    }
    
    ((st7701_lcd*)_handles.lcd)->fillScreen(color);
}

void ST7701_GT911::drawBitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t* color_data) {
    if (!_initialized) {
        ESP_LOGW(TAG, "Not initialized");
        return;
    }
    
    ((st7701_lcd*)_handles.lcd)->draw16bitbergbbitmap(x, y, w, h, color_data);
}

bool ST7701_GT911::getTouch(uint16_t* x, uint16_t* y) {
    if (!_initialized) {
        ESP_LOGW(TAG, "Not initialized");
        return false;
    }
    
    return ((gt911_touch*)_handles.touch)->getTouch(x, y);
}

void ST7701_GT911::setTouchRotation(int degrees) {
    if (!_initialized) {
        ESP_LOGW(TAG, "Not initialized");
        return;
    }
    
    ((gt911_touch*)_handles.touch)->set_rotation(degrees);
}

void ST7701_GT911::lvglTask() {
    // Simple LVGL task handler - main.cpp handles the complex LVGL setup
    lv_timer_handler();
}

bool ST7701_GT911::lock(uint32_t timeout_ms) {
    // Simple implementation - could be enhanced with actual mutex
    return true;
}

void ST7701_GT911::unlock() {
    // Simple implementation - could be enhanced with actual mutex
}

lv_disp_t* ST7701_GT911::getDisplay() {
    return _lvgl_display;
}

lv_indev_t* ST7701_GT911::getInputDevice() {
    return _lvgl_input;
}

void* ST7701_GT911::getLCDHandle() {
    return _initialized ? _handles.lcd : nullptr;
}

void* ST7701_GT911::getTouchHandle() {
    return _initialized ? _handles.touch : nullptr;
}

uint16_t ST7701_GT911::getWidth() {
    return _initialized ? ((st7701_lcd*)_handles.lcd)->width() : 0;
}

uint16_t ST7701_GT911::getHeight() {
    return _initialized ? ((st7701_lcd*)_handles.lcd)->height() : 0;
}

bool ST7701_GT911::isInitialized() {
    return _initialized;
}

// C API implementation
extern "C" {

bool st7701_gt911_init(int rotation, int backlight) {
    return ST7701_GT911::begin(rotation, backlight);
}

void st7701_gt911_set_backlight(int percent) {
    ST7701_GT911::setBacklight(percent);
}

void st7701_gt911_set_rotation(int degrees) {
    ST7701_GT911::setRotation(degrees);
}

void st7701_gt911_fill_screen(uint16_t color) {
    ST7701_GT911::fillScreen(color);
}

void st7701_gt911_draw_bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t* color_data) {
    ST7701_GT911::drawBitmap(x, y, w, h, color_data);
}

bool st7701_gt911_get_touch(uint16_t* x, uint16_t* y) {
    return ST7701_GT911::getTouch(x, y);
}

void st7701_gt911_set_touch_rotation(int degrees) {
    ST7701_GT911::setTouchRotation(degrees);
}

void st7701_gt911_lvgl_task(void) {
    ST7701_GT911::lvglTask();
}

bool st7701_gt911_lock(uint32_t timeout_ms) {
    return ST7701_GT911::lock(timeout_ms);
}

void st7701_gt911_unlock(void) {
    ST7701_GT911::unlock();
}

lv_disp_t* st7701_gt911_get_display(void) {
    return ST7701_GT911::getDisplay();
}

lv_indev_t* st7701_gt911_get_input_device(void) {
    return ST7701_GT911::getInputDevice();
}

void* st7701_gt911_get_lcd_handle(void) {
    return ST7701_GT911::getLCDHandle();
}

void* st7701_gt911_get_touch_handle(void) {
    return ST7701_GT911::getTouchHandle();
}

uint16_t st7701_gt911_get_width(void) {
    return ST7701_GT911::getWidth();
}

uint16_t st7701_gt911_get_height(void) {
    return ST7701_GT911::getHeight();
}

bool st7701_gt911_is_initialized(void) {
    return ST7701_GT911::isInitialized();
}

} // extern "C"
