#include "ESP32P4_Display.h"
#include "st7701_lcd.h"
#include "gt911_touch.h"
#include "boards/pins_config_p4.h"

// Global instance
ESP32P4_Display Display;

// LVGL callback functions (based on demo project)
static bool lvgl_port_flush_dpi_panel_ready_callback(esp_lcd_panel_handle_t panel_io, esp_lcd_dpi_panel_event_data_t *edata, void *user_ctx)
{
    lv_disp_drv_t *disp_drv = (lv_disp_drv_t *)user_ctx;
    assert(disp_drv != NULL);
    lv_disp_flush_ready(disp_drv);
    return false;
}

// Display flush callback (based on demo project)
void esp32p4_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;
    Display.drawBitmap(offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, &color_p->full);
}

// Touch read callback (based on demo project)
void esp32p4_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
    bool touched;
    uint16_t touchX, touchY;

    touched = Display.getTouch(&touchX, &touchY);

    if (!touched) {
        data->state = LV_INDEV_STATE_REL;
    } else {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = touchX;
        data->point.y = touchY;
    }
}

// Rotation callback (based on demo project)
static void lvgl_port_update_callback(lv_disp_drv_t *drv)
{
    switch (drv->rotated) {
    case LV_DISP_ROT_NONE:
        Display.setTouchRotation(0);
        break;
    case LV_DISP_ROT_90:
        Display.setTouchRotation(1);
        break;
    case LV_DISP_ROT_180:
        Display.setTouchRotation(2);
        break;
    case LV_DISP_ROT_270:
        Display.setTouchRotation(3);
        break;
    }
}

// Constructor with default pins
ESP32P4_Display::ESP32P4_Display() 
    : _lcd_rst(LCD_RST), _tp_sda(TP_I2C_SDA), _tp_scl(TP_I2C_SCL), _tp_rst(TP_RST), _tp_int(TP_INT)
{
    _initialized = false;
    _lvgl_initialized = false;
    lv_display = nullptr;
    lv_buf1 = nullptr;
    lv_buf2 = nullptr;
    i2c_handle = NULL;
}

// Constructor with custom pins
ESP32P4_Display::ESP32P4_Display(int8_t lcd_rst, int8_t tp_sda, int8_t tp_scl, int8_t tp_rst, int8_t tp_int)
    : _lcd_rst(lcd_rst), _tp_sda(tp_sda), _tp_scl(tp_scl), _tp_rst(tp_rst), _tp_int(tp_int)
{
    _initialized = false;
    _lvgl_initialized = false;
    lv_display = nullptr;
    lv_buf1 = nullptr;
    lv_buf2 = nullptr;
    i2c_handle = NULL;
}

bool ESP32P4_Display::begin()
{
    Serial.println("ESP32P4_Display: Initializing...");
    
    // Initialize I2C bus (based on demo project)
    i2c_master_bus_config_t i2c_bus_conf = {
        .i2c_port = I2C_NUM_1,
        .sda_io_num = (gpio_num_t)_tp_sda,
        .scl_io_num = (gpio_num_t)_tp_scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {
            .enable_internal_pullup = 1,
        },
    };
    
    esp_err_t ret = i2c_new_master_bus(&i2c_bus_conf, &i2c_handle);
    if (ret != ESP_OK) {
        Serial.printf("ESP32P4_Display: I2C init failed: %s\n", esp_err_to_name(ret));
        return false;
    }
    
    return begin(i2c_handle);
}

bool ESP32P4_Display::begin(i2c_master_bus_handle_t i2c_handle)
{
    this->i2c_handle = i2c_handle;
    
    // Initialize display
    if (!initDisplay()) {
        Serial.println("ESP32P4_Display: Display init failed");
        return false;
    }
    
    // Initialize touch
    if (!initTouch()) {
        Serial.println("ESP32P4_Display: Touch init failed");
        return false;
    }
    
    _initialized = true;
    Serial.println("ESP32P4_Display: Initialization complete!");
    return true;
}

bool ESP32P4_Display::initDisplay()
{
    // Create LCD instance (using existing ST7701 library)
    static st7701_lcd lcd(_lcd_rst);
    
    // Initialize LCD
    lcd.begin();
    // Ensure backlight is ON using board pin (independent of library macro)
    #ifdef LCD_LED
    if (LCD_LED >= 0) {
        pinMode(LCD_LED, OUTPUT);
        digitalWrite(LCD_LED, 1);
    }
    #endif
    
    // Convert between handle types
    bsp_lcd_handles_t bsp_handles;
    lcd.get_handle(&bsp_handles);
    
    // Copy to our handle structure
    lcd_handles.mipi_dsi_bus = bsp_handles.mipi_dsi_bus;
    lcd_handles.io = bsp_handles.io;
    lcd_handles.panel = bsp_handles.panel;
    lcd_handles.control = bsp_handles.control;
    
    // Enable backlight
    setBacklight(100);
    
    return true;
}

bool ESP32P4_Display::initTouch()
{
    // Create touch instance (using existing GT911 library)
    static gt911_touch touch(_tp_sda, _tp_scl, _tp_rst, _tp_int);
    
    // Initialize touch
    touch.begin();
    touch.set_rotation(0);
    
    return true;
}

lv_disp_t* ESP32P4_Display::initLVGL()
{
    if (!_initialized) {
        Serial.println("ESP32P4_Display: Must call begin() first!");
        return nullptr;
    }
    
    if (_lvgl_initialized) {
        return lv_display;
    }
    
    Serial.println("ESP32P4_Display: Initializing LVGL...");
    
    // Initialize LVGL
    lv_init();
    
    // Initialize LVGL buffers (based on demo project)
    if (!initLVGLBuffers()) {
        Serial.println("ESP32P4_Display: LVGL buffer init failed");
        return nullptr;
    }
    
    // Create display driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = ESP32P4_LCD_H_RES;
    disp_drv.ver_res = ESP32P4_LCD_V_RES;
    disp_drv.flush_cb = esp32p4_disp_flush;
    disp_drv.draw_buf = &lv_draw_buf;
    disp_drv.full_refresh = false;  // Based on demo project
    // Landscape by default
    disp_drv.sw_rotate = 1;
    disp_drv.rotated = LV_DISP_ROT_270;
    
    // Register display
    lv_display = lv_disp_drv_register(&disp_drv);
    lv_disp_set_rotation(lv_display, LV_DISP_ROT_270);
    
    // Create input driver
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = esp32p4_touchpad_read;
    lv_indev_drv_register(&indev_drv);
    
    // Register DPI panel callbacks (based on demo project)
    registerLVGLCallbacks(&disp_drv);
    
    _lvgl_initialized = true;
    Serial.println("ESP32P4_Display: LVGL initialized successfully!");
    
    return lv_display;
}

bool ESP32P4_Display::initLVGLBuffers()
{
    // Allocate buffers in PSRAM (based on demo project)
    size_t buffer_size = sizeof(lv_color_t) * ESP32P4_LCD_H_RES * ESP32P4_LCD_V_RES;
    
    lv_buf1 = (lv_color_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
    lv_buf2 = (lv_color_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
    
    if (!lv_buf1 || !lv_buf2) {
        Serial.println("ESP32P4_Display: Failed to allocate LVGL buffers");
        return false;
    }
    
    lv_disp_draw_buf_init(&lv_draw_buf, lv_buf1, lv_buf2, ESP32P4_LCD_H_RES * ESP32P4_LCD_V_RES);
    
    return true;
}

void ESP32P4_Display::registerLVGLCallbacks(lv_disp_drv_t *disp_drv)
{
    esp_lcd_dpi_panel_event_callbacks_t cbs = {0};
    cbs.on_color_trans_done = lvgl_port_flush_dpi_panel_ready_callback;
    esp_lcd_dpi_panel_register_event_callbacks(lcd_handles.panel, &cbs, disp_drv);
}

void ESP32P4_Display::lvglLoop()
{
    if (_lvgl_initialized) {
        lv_timer_handler();
    }
}

void ESP32P4_Display::drawBitmap(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end, uint16_t *color_data)
{
    // Use existing ST7701 library
    static st7701_lcd lcd(_lcd_rst);
    lcd.lcd_draw_bitmap(x_start, y_start, x_end, y_end, color_data);
}

void ESP32P4_Display::fillScreen(uint16_t color)
{
    // Create a solid color buffer
    uint16_t *buffer = (uint16_t*)malloc(ESP32P4_LCD_H_RES * sizeof(uint16_t));
    if (buffer) {
        for (int i = 0; i < ESP32P4_LCD_H_RES; i++) {
            buffer[i] = color;
        }
        
        for (int y = 0; y < ESP32P4_LCD_V_RES; y++) {
            drawBitmap(0, y, ESP32P4_LCD_H_RES - 1, y, buffer);
        }
        
        free(buffer);
    }
}

void ESP32P4_Display::setBacklight(uint32_t level)
{
    // Use existing ST7701 library
    static st7701_lcd lcd(_lcd_rst);
    lcd.example_bsp_set_lcd_backlight(level);
}

bool ESP32P4_Display::getTouch(uint16_t *x, uint16_t *y)
{
    // Use existing GT911 library
    static gt911_touch touch(_tp_sda, _tp_scl, _tp_rst, _tp_int);
    return touch.getTouch(x, y);
}

void ESP32P4_Display::setTouchRotation(uint8_t rotation)
{
    // Use existing GT911 library
    static gt911_touch touch(_tp_sda, _tp_scl, _tp_rst, _tp_int);
    touch.set_rotation(rotation);
}

void ESP32P4_Display::getHandle(esp32p4_lcd_handles_t *ret_handles)
{
    *ret_handles = lcd_handles;
}