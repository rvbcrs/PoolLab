#ifndef ESP32P4_DISPLAY_H
#define ESP32P4_DISPLAY_H

#include <Arduino.h>
#include "lvgl.h"
#include "driver/i2c_master.h"
#include "esp_lcd_types.h"
#include "esp_lcd_mipi_dsi.h"

// Display configuration
#define ESP32P4_LCD_H_RES 480
#define ESP32P4_LCD_V_RES 800

// Pin definitions (can be overridden)
#ifndef ESP32P4_LCD_RST
#define ESP32P4_LCD_RST -1
#endif

#ifndef ESP32P4_TP_I2C_SDA
#define ESP32P4_TP_I2C_SDA 7
#endif

#ifndef ESP32P4_TP_I2C_SCL
#define ESP32P4_TP_I2C_SCL 8
#endif

#ifndef ESP32P4_TP_RST
#define ESP32P4_TP_RST -1
#endif

#ifndef ESP32P4_TP_INT
#define ESP32P4_TP_INT -1
#endif

typedef struct {
    esp_lcd_dsi_bus_handle_t    mipi_dsi_bus;  /*!< MIPI DSI bus handle */
    esp_lcd_panel_io_handle_t   io;            /*!< ESP LCD IO handle */
    esp_lcd_panel_handle_t      panel;         /*!< ESP LCD panel (color) handle */
    esp_lcd_panel_handle_t      control;       /*!< ESP LCD panel (control) handle */
} esp32p4_lcd_handles_t;

class ESP32P4_Display
{
public:
    ESP32P4_Display();
    ESP32P4_Display(int8_t lcd_rst, int8_t tp_sda, int8_t tp_scl, int8_t tp_rst = -1, int8_t tp_int = -1);
    
    // Simple initialization - just call this!
    bool begin();
    
    // Advanced initialization with custom I2C handle
    bool begin(i2c_master_bus_handle_t i2c_handle);
    
    // LVGL integration
    lv_disp_t* initLVGL();
    void lvglLoop();
    
    // Display functions
    void drawBitmap(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end, uint16_t *color_data);
    void fillScreen(uint16_t color);
    void setBacklight(uint32_t level);
    
    // Touch functions
    bool getTouch(uint16_t *x, uint16_t *y);
    void setTouchRotation(uint8_t rotation);
    
    // Getters
    uint16_t getWidth() { return ESP32P4_LCD_H_RES; }
    uint16_t getHeight() { return ESP32P4_LCD_V_RES; }
    esp32p4_lcd_handles_t* getHandles() { return &lcd_handles; }
    
    // Internal functions (based on demo project)
    void enableDSIPhyPower();
    void initBacklight();
    void setBacklightLevel(uint32_t level);
    void getHandle(esp32p4_lcd_handles_t *ret_handles);

private:
    int8_t _lcd_rst;
    int8_t _tp_sda;
    int8_t _tp_scl;
    int8_t _tp_rst;
    int8_t _tp_int;
    
    esp32p4_lcd_handles_t lcd_handles;
    i2c_master_bus_handle_t i2c_handle;
    
    bool _initialized;
    bool _lvgl_initialized;
    
    // LVGL objects
    lv_disp_t* lv_display;
    lv_disp_draw_buf_t lv_draw_buf;
    lv_color_t *lv_buf1;
    lv_color_t *lv_buf2;
    
    // Internal functions
    bool initDisplay();
    bool initTouch();
    bool initLVGLBuffers();
    void registerLVGLCallbacks(lv_disp_drv_t *disp_drv);
};

// Global instance for easy access
extern ESP32P4_Display Display;

#endif // ESP32P4_DISPLAY_H