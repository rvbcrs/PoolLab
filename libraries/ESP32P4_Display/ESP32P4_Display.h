#ifndef ESP32P4_DISPLAY_H
#define ESP32P4_DISPLAY_H

#include <Arduino.h>
#include "lvgl.h"
#include "st7701_lcd.h"
#include "gt911_touch.h"
#include "driver/i2c_master.h"
#include "boards/pins_config_p4.h"

/**
 * @brief Simple wrapper for ESP32-P4 MIPI-DSI display initialization
 *
 * This class simplifies the initialization of the ST7701 LCD and GT911 touch controller
 * for ESP32-P4 boards. It handles all the complex setup internally.
 *
 * Usage:
 *   ESP32P4_Display display;
 *   display.begin();
 *   lv_disp_t* disp = display.initLVGL();
 */
class ESP32P4_Display {
public:
    /**
     * @brief Constructor - uses default pins from pins_config_p4.h
     */
    ESP32P4_Display();

    /**
     * @brief Initializes the display and touch hardware.
     * @return true if successful, false otherwise.
     */
    bool begin();

    /**
     * @brief Initializes LVGL display and input drivers.
     * @param rotation The desired rotation for the LVGL display.
     * @return Pointer to the LVGL display object, or nullptr if initialization fails.
     */
    lv_disp_t* initLVGL(lv_disp_rot_t rotation = LV_DISP_ROT_270);

    /**
     * @brief Get LCD handles for advanced usage.
     * @return Pointer to bsp_lcd_handles_t struct.
     */
    bsp_lcd_handles_t* getLCDHandles();

    /**
     * @brief Get touch driver instance.
     * @return Pointer to gt911_touch object.
     */
    gt911_touch* getTouch();

    /**
     * @brief Sets the backlight brightness.
     * @param percent Brightness percentage (0-100).
     */
    void setBacklight(uint8_t percent);

private:
    // Direct instances of LCD and touch drivers
    st7701_lcd _lcd;
    gt911_touch _touch;
    
    bsp_lcd_handles_t _lcd_handles;
    
    // LVGL related
    lv_disp_draw_buf_t _draw_buf;
    lv_color_t *_buf1;
    lv_color_t *_buf2;
    lv_disp_drv_t _disp_drv;
    lv_indev_drv_t _indev_drv;
    
    bool _initialized;
    bool _lvgl_initialized;
};

#endif // ESP32P4_DISPLAY_H
