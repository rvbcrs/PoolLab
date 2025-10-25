#include "ESP32P4_Display.h"
#include <esp_lcd_panel_ops.h>

ESP32P4_Display::ESP32P4_Display()
    : _lcd(LCD_RST)  // Initialize LCD with reset pin
    , _touch(TP_I2C_SDA, TP_I2C_SCL, TP_RST, TP_INT)  // Initialize touch with pins
    , _buf1(nullptr)
    , _buf2(nullptr)
    , _initialized(false)
    , _lvgl_initialized(false)
{
    Serial.println("ESP32P4_Display: Constructor called");
    memset(&_lcd_handles, 0, sizeof(_lcd_handles));
    memset(&_draw_buf, 0, sizeof(_draw_buf));
    memset(&_disp_drv, 0, sizeof(_disp_drv));
    memset(&_indev_drv, 0, sizeof(_indev_drv));
}

bool ESP32P4_Display::begin() {
    Serial.println("ESP32P4_Display: begin() called - initializing LCD and touch");
    
    // Don't initialize I2C here - use the existing one from main.cpp
    // The main.cpp will initialize I2C bus 1, and GT911 will use it via i2c_master_get_bus_handle(1)
    
    // Initialize LCD
    _lcd.begin();
    Serial.println("ESP32P4_Display: LCD initialized");
    
    // Initialize touch (will use existing I2C bus from main.cpp)
    _touch.begin();  // Will call i2c_master_get_bus_handle(1) internally
    _touch.set_rotation(0);  // Keep touch in portrait mode, we'll transform in callback
    Serial.println("ESP32P4_Display: Touch initialized");
    
    // Get LCD handles
    _lcd.get_handle(&_lcd_handles);
    Serial.println("ESP32P4_Display: LCD handles obtained");
    
    _initialized = true;
    Serial.println("ESP32P4_Display: Initialization complete!");
    return true;
}

lv_disp_t* ESP32P4_Display::initLVGL(lv_disp_rot_t rotation) {
    Serial.println("ESP32P4_Display: initLVGL() called - initializing LVGL");
    
    if (!_initialized) {
        Serial.println("ESP32P4_Display: Display not initialized, call begin() first");
        return nullptr;
    }
    
    // Allocate LVGL buffers in PSRAM
    size_t buffer_size = LCD_V_RES * 100 * sizeof(lv_color_t);
    _buf1 = (lv_color_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
    _buf2 = (lv_color_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
    
    if (!_buf1 || !_buf2) {
        Serial.println("ESP32P4_Display: Failed to allocate LVGL buffers");
        return nullptr;
    }
    
    Serial.println("ESP32P4_Display: LVGL buffers allocated");
    
    // Initialize draw buffer
    lv_disp_draw_buf_init(&_draw_buf, _buf1, _buf2, LCD_V_RES * 100);
    
    // Initialize display driver
    lv_disp_drv_init(&_disp_drv);
    _disp_drv.hor_res = LCD_H_RES;  // 480
    _disp_drv.ver_res = LCD_V_RES;  // 800
    _disp_drv.draw_buf = &_draw_buf;
    _disp_drv.full_refresh = false;
    _disp_drv.sw_rotate = 1;  // Enable software rotation
    _disp_drv.rotated = rotation;  // Use provided rotation
    _disp_drv.flush_cb = [](lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
        const int offsetx1 = area->x1;
        const int offsetx2 = area->x2;
        const int offsety1 = area->y1;
        const int offsety2 = area->y2;
        
        // Get the wrapper instance from user data
        ESP32P4_Display* wrapper = (ESP32P4_Display*)disp->user_data;
        if (wrapper && wrapper->getLCDHandles()->panel) {
            // Use esp_lcd_panel_draw_bitmap to draw to the display
            esp_lcd_panel_draw_bitmap(wrapper->getLCDHandles()->panel, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_p);
        }
        
        // Signal that the flush is complete
        lv_disp_flush_ready(disp);
    };
    _disp_drv.user_data = this;  // Pass wrapper instance for LCD access
    
    lv_disp_t* disp = lv_disp_drv_register(&_disp_drv);
    if (!disp) {
        Serial.println("ESP32P4_Display: Failed to register LVGL display driver");
        return nullptr;
    }
    
    Serial.println("ESP32P4_Display: LVGL display driver registered");
    
    // Register DPI panel callback
    esp_lcd_dpi_panel_event_callbacks_t cbs = {0};
    cbs.on_color_trans_done = [](esp_lcd_panel_handle_t panel_io, esp_lcd_dpi_panel_event_data_t *edata, void *user_ctx) -> bool {
        lv_disp_drv_t *drv = (lv_disp_drv_t *)user_ctx;
        if (drv) lv_disp_flush_ready(drv);
        return false;
    };
    esp_lcd_dpi_panel_register_event_callbacks(_lcd_handles.panel, &cbs, &_disp_drv);
    
    // Register touch input
    lv_indev_drv_init(&_indev_drv);
    _indev_drv.type = LV_INDEV_TYPE_POINTER;
    _indev_drv.read_cb = [](lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
        (void)indev_driver;
        
        // Get touch data from the touch driver
        static gt911_touch* touch_instance = nullptr;
        if (!touch_instance) {
            // Get the touch instance from the wrapper
            // This is a bit hacky but works for now
            ESP32P4_Display* wrapper = (ESP32P4_Display*)indev_driver->user_data;
            if (wrapper) {
                touch_instance = wrapper->getTouch();
            }
        }
        
        if (touch_instance) {
            uint16_t x, y;
            bool touched = touch_instance->getTouch(&x, &y);
            if (touched) {
                data->state = LV_INDEV_STATE_PR;
                data->point.x = x;
                data->point.y = y;
            } else {
                data->state = LV_INDEV_STATE_REL;
            }
        } else {
            data->state = LV_INDEV_STATE_REL;
        }
    };
    _indev_drv.user_data = this;  // Pass wrapper instance for touch access
    lv_indev_drv_register(&_indev_drv);
    
    _lvgl_initialized = true;
    Serial.println("ESP32P4_Display: LVGL initialization complete!");
    return disp;
}

void ESP32P4_Display::setBacklight(uint8_t percent) {
    Serial.printf("ESP32P4_Display: setBacklight(%d%%) called\n", percent);
    if (_initialized && _lcd_handles.panel) {
        _lcd.example_bsp_set_lcd_backlight(percent);
    } else {
        Serial.println("ESP32P4_Display: Display not initialized, cannot set backlight");
    }
}

bsp_lcd_handles_t* ESP32P4_Display::getLCDHandles() {
    return &_lcd_handles;
}

gt911_touch* ESP32P4_Display::getTouch() {
    return &_touch;
}
