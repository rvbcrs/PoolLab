#include "Esp32C6Board.h"
#include <Arduino.h>
#include <lvgl.h>
#include <esp_log.h>

// C6-specific drivers (only compiled when targeting C6)
#if defined(BOARD_ESP32C6_TOUCH_1_47)
#include <SPI.h>
#include <Arduino_GFX_Library.h>
#include "core/display/St7789C6.h"
#include "core/DisplayBridge.h"
#include "io/Touch.h"

// File-scope display objects (owned here, not in main.cpp)
static Arduino_DataBus     *s_bus           = nullptr;
static Arduino_GFX         *s_gfx           = nullptr;
static core::St7789C6      *s_displayDriver = nullptr;
static core::DisplayBridge *s_displayBridge = nullptr;

// C6 display hardware constants
static const int C6_DC_PIN   = 15;
static const int C6_CS_PIN   = 14;
static const int C6_SCK_PIN  = 1;
static const int C6_MOSI_PIN = 2;
static const int C6_RST_PIN  = 22;
static const int C6_W        = 172;
static const int C6_H        = 320;
static const int C6_COL_OFF  = 34;
static const int C6_ROW_OFF  = 0;
#endif // BOARD_ESP32C6_TOUCH_1_47

namespace core {

void Esp32C6Board::earlyInit() {
    // Nothing board-specific before peripherals
}

void Esp32C6Board::initPeripherals() {
#if defined(BOARD_ESP32C6_TOUCH_1_47)
    SPI.begin(C6_SCK_PIN, -1 /* MISO */, C6_MOSI_PIN, C6_CS_PIN);
#endif
}

BoardPins Esp32C6Board::pins() const {
    return BoardPins{
        .lcdBl     = 23,
        .btn1      = 9, .btn2   = 0,
        .motorStby = 3,
        .m1in1     = 7, .m1in2  = 8, .m1pwm = 5,
        .m2in1     = 4, .m2in2  = 6, .m2pwm = 9,
        .i2sBclk   = 42, .i2sLrc = 2, .i2sDin = 41,
    };
}

lv_disp_t* Esp32C6Board::initDisplay() {
#if defined(BOARD_ESP32C6_TOUCH_1_47)
    // Hardware reset pulse
    if (C6_RST_PIN >= 0) {
        pinMode(C6_RST_PIN, OUTPUT);
        digitalWrite(C6_RST_PIN, LOW);
        delay(10);
        digitalWrite(C6_RST_PIN, HIGH);
        delay(120);
    }

    // Create display objects
    s_bus = new Arduino_HWSPI(C6_DC_PIN, C6_CS_PIN, C6_SCK_PIN, C6_MOSI_PIN, -1);
    s_gfx = new Arduino_ST7789(s_bus, C6_RST_PIN, 0 /* rotation */, false /* IPS */,
                                C6_W, C6_H,
                                C6_COL_OFF, C6_ROW_OFF,
                                C6_COL_OFF, C6_ROW_OFF);
    s_displayDriver = new core::St7789C6(s_gfx, s_bus);

    s_displayDriver->begin();
    s_displayDriver->setRotation(1); // landscape

    // Backlight on
    int blPin = pins().lcdBl;
    if (blPin >= 0) { pinMode(blPin, OUTPUT); digitalWrite(blPin, HIGH); }

    s_gfx->fillScreen(BLACK);

    // Register LVGL display via DisplayBridge
    s_displayBridge = new core::DisplayBridge(s_gfx);
    s_displayBridge->initLvgl(20);
    return s_displayBridge->registerDisplay();
#else
    return lv_disp_get_default();
#endif
}

void Esp32C6Board::initTouch() {
#if defined(BOARD_ESP32C6_TOUCH_1_47)
    io::touchBegin(); // initialises the Axs5106L I2C bus + device handle

    // Register LVGL pointer input device using polling read
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.scroll_limit           = 4;
    indev_drv.scroll_throw            = 8;
    indev_drv.long_press_time         = 400;
    indev_drv.long_press_repeat_time  = 0;
    indev_drv.gesture_limit           = 20;
    indev_drv.gesture_min_velocity    = 2;
    indev_drv.type                    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = [](lv_indev_drv_t *d, lv_indev_data_t *data) -> void {
        (void)d;
        uint8_t buf[14] = {0};
        if (!io::i2cRead(io::AXS5106L_ADDR, io::AXS5106L_TOUCH_DATA_REG, buf, sizeof(buf))) {
            static bool fail_reported = false;
            if (!fail_reported) {
                ESP_LOGI("TOUCH", "LVGL read_cb: I2C read failed!");
                fail_reported = true;
            }
            data->state = LV_INDEV_STATE_RELEASED;
            return;
        }
        uint8_t n = buf[1];
        if (n == 0) { data->state = LV_INDEV_STATE_RELEASED; return; }

        uint16_t rx = (((uint16_t)(buf[2] & 0x0F)) << 8) | buf[3];
        uint16_t ry = (((uint16_t)(buf[4] & 0x0F)) << 8) | buf[5];
        int16_t x = (int16_t)ry; // swapped for landscape
        int16_t y = (int16_t)rx;

        lv_disp_t *disp_local = lv_disp_get_default();
        int hor = disp_local ? (int)lv_disp_get_hor_res(disp_local) : 320;
        int ver = disp_local ? (int)lv_disp_get_ver_res(disp_local) : 172;
        x = constrain(x, 0, hor - 1);
        y = constrain(y, 0, ver - 1);

        data->point.x = x;
        data->point.y = y;
        data->state   = LV_INDEV_STATE_PRESSED;

        static uint32_t last_press_ms = 0;
        const uint32_t DEBOUNCE_MS = 400;
        uint32_t now = millis();
        if (now - last_press_ms < DEBOUNCE_MS) {
            data->state = LV_INDEV_STATE_RELEASED;
            return;
        }
        last_press_ms = now;

        static uint32_t last_read_ms = 0;
        if (now - last_read_ms < 50) return; // throttle to 20 Hz
        last_read_ms = now;
    };
    (void)lv_indev_drv_register(&indev_drv);
#endif
}

bool Esp32C6Board::hasZigbee() const {
#if defined(HAS_ZIGBEE) && HAS_ZIGBEE
    return true;
#else
    return false;
#endif
}

} // namespace core
