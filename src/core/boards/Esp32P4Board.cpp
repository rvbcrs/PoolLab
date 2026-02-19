#include "Esp32P4Board.h"
#include <Arduino.h>
#include <lvgl.h>
#include <esp_log.h>

#if defined(BOARD_ESP32P4_43)
#include "boards/pins_config_p4.h"
#include "st7701_lcd.h"
#include "gt911_touch.h"
#include "driver/i2c_master.h"

// File-scope display/touch objects (owned here, not in main.cpp)
static st7701_lcd            s_lcd(LCD_RST);
static gt911_touch           s_touch(TP_I2C_SDA, TP_I2C_SCL, TP_RST, TP_INT);
static bsp_lcd_handles_t     s_lcd_panels;
// Non-static: GT911 driver retrieves it via i2c_master_get_bus_handle(I2C_NUM_1)
i2c_master_bus_handle_t      p4_i2c_handle = NULL;

// LVGL draw buffers (allocated in PSRAM during initDisplay)
static lv_disp_draw_buf_t    s_draw_buf;
static lv_color_t           *s_buf  = nullptr;
static lv_color_t           *s_buf1 = nullptr;

// Sync touch rotation with display rotation
static void p4_rotation_cb(lv_disp_drv_t *drv) {
    switch (drv->rotated) {
    case LV_DISP_ROT_NONE:   s_touch.set_rotation(0); break;
    case LV_DISP_ROT_90:     s_touch.set_rotation(1); break;
    case LV_DISP_ROT_180:    s_touch.set_rotation(2); break;
    case LV_DISP_ROT_270:    s_touch.set_rotation(3); break;
    }
}
#endif // BOARD_ESP32P4_43

namespace core {

void Esp32P4Board::earlyInit() {
    // P4: display and touch init happen in initDisplay(); nothing needed here
}

void Esp32P4Board::initPeripherals() {
    // P4 uses ESP-IDF i2c_master API for touch (I2C_NUM_1); do not call Wire.begin()
    // C6 UART bridge: GPIO16 (RX) / GPIO17 (TX) — configured by ZigbeeClient when needed
}

BoardPins Esp32P4Board::pins() const {
    return BoardPins{
        .lcdBl     = -1,   // TBD — adjust for actual board wiring
        .btn1      = -1, .btn2   = -1,
        .motorStby = 11,
        .m1in1     = 12, .m1in2  = 13, .m1pwm = 14,
        .m2in1     = 15, .m2in2  = 36, .m2pwm = 37,
        .i2sBclk   = -1, .i2sLrc = -1, .i2sDin = -1, // no speaker on P4 yet
    };
}

lv_disp_t* Esp32P4Board::initDisplay() {
#if defined(BOARD_ESP32P4_43)
    ESP_LOGI("BOARD", "P4: Initialising MIPI-DSI display (480x800)");

    // ── I2C for touch (I2C_NUM_1) ──────────────────────────────────────────
    i2c_master_bus_config_t i2c_cfg = {
        .i2c_port            = I2C_NUM_1,
        .sda_io_num          = (gpio_num_t)TP_I2C_SDA,
        .scl_io_num          = (gpio_num_t)TP_I2C_SCL,
        .clk_source          = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt   = 7,
        .intr_priority       = 0,
        .trans_queue_depth   = 0,
        .flags               = {.enable_internal_pullup = 1},
    };
    i2c_new_master_bus(&i2c_cfg, &p4_i2c_handle);

    // ── LCD + touch ────────────────────────────────────────────────────────
    s_lcd.begin();
    s_touch.begin();           // uses i2c_master_get_bus_handle(I2C_NUM_1) internally
    s_touch.set_rotation(0);   // portrait; transform in the LVGL read_cb
    s_lcd.get_handle(&s_lcd_panels);

    ESP_LOGI("BOARD", "P4: LCD + touch initialised");

    // ── LVGL init ──────────────────────────────────────────────────────────
    lv_init();

    size_t buf_bytes = sizeof(lv_color_t) * LCD_V_RES * 100; // 800 × 100 lines
    s_buf  = (lv_color_t *)heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM);
    s_buf1 = (lv_color_t *)heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM);
    assert(s_buf && s_buf1 && "P4: Failed to allocate LVGL buffers in PSRAM");

    lv_disp_draw_buf_init(&s_draw_buf, s_buf, s_buf1, LCD_V_RES * 100);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res       = LCD_H_RES;   // 480
    disp_drv.ver_res       = LCD_V_RES;   // 800
    disp_drv.draw_buf      = &s_draw_buf;
    disp_drv.full_refresh  = false;
    disp_drv.sw_rotate     = 1;
    disp_drv.rotated       = LV_DISP_ROT_270; // landscape
    disp_drv.drv_update_cb = p4_rotation_cb;
    disp_drv.flush_cb      = [](lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
        (void)drv;
        s_lcd.lcd_draw_bitmap(area->x1, area->y1, area->x2 + 1, area->y2 + 1, &color_p->full);
    };

    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

    // DPI panel transfer-done → notify LVGL
    esp_lcd_dpi_panel_event_callbacks_t cbs = {0};
    cbs.on_color_trans_done = [](esp_lcd_panel_handle_t, esp_lcd_dpi_panel_event_data_t *, void *user_ctx) -> bool {
        lv_disp_drv_t *drv = (lv_disp_drv_t *)user_ctx;
        if (drv) lv_disp_flush_ready(drv);
        return false;
    };
    esp_lcd_dpi_panel_register_event_callbacks(s_lcd_panels.panel, &cbs, &disp_drv);

    // ── LVGL touch input ───────────────────────────────────────────────────
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = [](lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
        (void)indev_driver;
        static uint32_t last_press_time   = 0;
        static uint32_t last_release_time = 0;
        static uint16_t stable_x = 0, stable_y = 0;
        static bool     was_pressed = false;

        bool touched;
        uint16_t touchX, touchY;
        uint32_t now = millis();

        touched = s_touch.getTouch(&touchX, &touchY);

        // Reject invalid coordinates (GT911 edge case)
        if (touched && (touchX == 0 || touchY == 0 ||
                        touchX >= LCD_H_RES || touchY >= LCD_V_RES)) {
            touched = false;
        }

        if (!touched) {
            if (was_pressed && (now - last_press_time) > 100) {
                data->state = LV_INDEV_STATE_REL;
                was_pressed = false;
                last_release_time = now;
            } else {
                data->state   = was_pressed ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
                data->point.x = stable_x;
                data->point.y = stable_y;
            }
        } else {
            if (!was_pressed && (now - last_release_time) < 200) {
                // Too soon after release — debounce
                data->state = LV_INDEV_STATE_REL;
            } else {
                data->state   = LV_INDEV_STATE_PR;
                data->point.x = touchX;
                data->point.y = touchY;
                if (!was_pressed) {
                    last_press_time = now;
                    stable_x = touchX;
                    stable_y = touchY;
                    was_pressed = true;
                } else {
                    stable_x = touchX;
                    stable_y = touchY;
                }
            }
        }
    };
    lv_indev_drv_register(&indev_drv);

    ESP_LOGI("BOARD", "P4: LVGL initialised (%dx%d sw-rotate 270°)", LCD_H_RES, LCD_V_RES);
    return disp;
#else
    return lv_disp_get_default();
#endif
}

void Esp32P4Board::initTouch() {
    // Touch already fully configured during initDisplay() (GT911 + LVGL indev).
    // Nothing additional needed here.
}

UiConfig Esp32P4Board::uiConfig() const {
    UiConfig cfg;
    cfg.cardHeight      = 220;
    cfg.valueFontSize   = 48;
    cfg.statsFontSize   = 14;
    cfg.valueY          = 35;
    cfg.subLblY         = 100;
    cfg.subValsY        = 118;
    cfg.pumpStatsY      = 140;
    cfg.unitSpacing     = 6;
    cfg.pumpIconY       = -10;
    cfg.forceThreeCards = true;
    cfg.showHostnameInIp = true;
    cfg.zigbeeLabelText = "Zigbee (via C6)";
    return cfg;
}

} // namespace core
