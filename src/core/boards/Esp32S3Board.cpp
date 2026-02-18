#include "Esp32S3Board.h"
#include <Arduino.h>
#include <lvgl.h>
#include <esp_log.h>

#if defined(USE_JC3248W535)
#include "jc3248w535.h"
static jc3248w535_handles_t s_jc_handles;
#elif defined(BOARD_ESP32S3_35)
// Legacy non-JC BSP path
#include "esp_bsp.h"
#include "display.h"
#include "lv_port.h"
#endif

#if defined(BOARD_ESP32S3_35)
#include "io/Touch.h"  // for io::readTouchOnce / io::touchBegin
#endif

namespace core {

void Esp32S3Board::earlyInit() {
    // QSPI display handled by BSP / JC library
}

void Esp32S3Board::initPeripherals() {
    // SPI/I2C left to BSP; no manual init needed on S3
}

BoardPins Esp32S3Board::pins() const {
    return BoardPins{
        .lcdBl     = 23,
        .btn1      = -1, .btn2   = -1,  // no user button on this board
        .motorStby = 46,
        .m1in1     = 15, .m1in2  = 16, .m1pwm = 17,
        .m2in1     = 18, .m2in2  = 14, .m2pwm = 9,
        .i2sBclk   = 42, .i2sLrc = 2,  .i2sDin = 41,
    };
}

lv_disp_t* Esp32S3Board::initDisplay() {
#if defined(USE_JC3248W535)
    ESP_LOGI("BOARD", "S3: Starting display init (JC3248W535)");
    (void)jc3248w535_begin_simple(90, &s_jc_handles);
    (void)jc3248w535_backlight_set(100);
    // Brief startup banner (runs under the BSP LVGL task)
    if (jc3248w535_lock(5)) {
        lv_obj_clean(lv_scr_act());
        lv_obj_t *label = lv_label_create(lv_scr_act());
        lv_label_set_text(label, "Starting UI...");
        lv_obj_center(label);
        jc3248w535_unlock();
    }
    return lv_disp_get_default();
#else
    // Non-JC S3 BSP path: display is initialised by esp_bsp before this point
    return lv_disp_get_default();
#endif
}

void Esp32S3Board::initTouch() {
#if defined(BOARD_ESP32S3_35)
    // Register a custom LVGL pointer input device using the io::Touch polling API.
    // The BSP touch device (if present) is left as-is; this driver takes precedence
    // for JC-path boards that have the Axs5106L wired separately.
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = [](lv_indev_drv_t *d, lv_indev_data_t *data) {
        (void)d;

        static uint32_t last_touch_ms   = 0;
        static int16_t  last_x          = 0, last_y = 0;
        static bool     was_pressed     = false;

        const uint32_t TOUCH_DEBOUNCE_MS   = 150;
        const uint32_t RELEASE_TIMEOUT_MS  = 100;
        const uint16_t JITTER_TOLERANCE_PX = 8;

        io::TouchPoint p{};
        uint32_t now = millis();

        if (io::readTouchOnce(p) && p.pressed) {
            int16_t dx = abs(p.x - last_x);
            int16_t dy = abs(p.y - last_y);

            if (was_pressed &&
                (now - last_touch_ms < TOUCH_DEBOUNCE_MS) &&
                dx <= JITTER_TOLERANCE_PX &&
                dy <= JITTER_TOLERANCE_PX) {
                // Within debounce window and same spot — hold current state
                data->point.x = last_x;
                data->point.y = last_y;
                data->state   = LV_INDEV_STATE_PRESSED;
                return;
            }

            if (!was_pressed ||
                (now - last_touch_ms >= TOUCH_DEBOUNCE_MS) ||
                dx > JITTER_TOLERANCE_PX ||
                dy > JITTER_TOLERANCE_PX) {
                last_touch_ms = now;
                last_x        = p.x;
                last_y        = p.y;
                was_pressed   = true;
                data->point.x = p.x;
                data->point.y = p.y;
                data->state   = LV_INDEV_STATE_PRESSED;
            }
        } else {
            if (was_pressed && (now - last_touch_ms < RELEASE_TIMEOUT_MS)) {
                // Still within release timeout — avoid flicker
                data->point.x = last_x;
                data->point.y = last_y;
                data->state   = LV_INDEV_STATE_PRESSED;
            } else {
                was_pressed = false;
                data->state = LV_INDEV_STATE_RELEASED;
            }
        }
    };
    (void)lv_indev_drv_register(&indev_drv);

    // On the legacy (non-JC) BSP path, tune the BSP touch repeat thresholds
    // to prevent double key inserts on the on-screen keyboard.
#if !defined(USE_JC3248W535)
    {
        lv_indev_t *bsp_indev = bsp_display_get_input_dev();
        if (bsp_indev && bsp_indev->driver) {
            bsp_indev->driver->long_press_time        = 800;
            bsp_indev->driver->long_press_repeat_time = 0;
        }
    }
#endif
#endif // BOARD_ESP32S3_35
}

bool Esp32S3Board::lvglLock() {
#if defined(USE_JC3248W535)
    return jc3248w535_lock(1);
#elif defined(BOARD_ESP32S3_35)
    return bsp_display_lock(1);
#else
    return true;
#endif
}

void Esp32S3Board::lvglUnlock() {
#if defined(USE_JC3248W535)
    jc3248w535_unlock();
#elif defined(BOARD_ESP32S3_35)
    bsp_display_unlock();
#endif
}

} // namespace core
