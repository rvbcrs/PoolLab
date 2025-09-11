/*
  ESP32-C6 Waveshare Touch — Tuya UART Sniffer + On-screen Viewer
  - Uses Arduino_GFX to drive the onboard 1.47" JD9853 LCD via ST7789 driver
  - Shows latest Tuya frames on the display (and prints to USB Serial)
  Pins per working HelloWorld on ESP32-C6-Touch-LCD-1.47 (your test):
    DC=15, CS=14, SCK=1, MOSI=2, RST=22, BL=23
    LCD size: 172x320, col offset 34, row offset 0
  Sniffer wiring (non-intrusive):
    GND  -> common ground
    MCU -> CB3S RXD1 (pin 15) -> ESP32-C6 RX_A (GPIO 4 by default)
    CB3S TXD1 (pin 16) -> MCU -> ESP32-C6 RX_B (GPIO 5, optional)
    Leave ESP TX pins unconnected while sniffing.
*/

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <HardwareSerial.h>
#include <SPI.h>
#include <Wire.h>
#include <vector>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <lvgl.h>
#include <Adafruit_GFX.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
// Core modules
#include "core/Storage.h"
#include "core/DisplayBridge.h"
#include "domain/Metrics.h"
#include "domain/ControlPolicy.h"
// IO modules
#include "io/MqttClient.h"
#include "io/Touch.h"
#include "io/Tuya.h"
#include "ui/UI.h"
// Icons
extern "C" const lv_img_dsc_t water_pump_24dp_E3E3E3_FILL0_wght400_GRAD0_opsz24;
extern "C" const lv_img_dsc_t water_ph_32dp_E3E3E3_FILL0_wght400_GRAD0_opsz40;
extern "C" const lv_img_dsc_t water_orp_32dp_E3E3E3_FILL0_wght400_GRAD0_opsz40;

#define FIRMWARE_VERSION __DATE__ " " __TIME__

// Local convenience accessor for metrics (scoped to this file only)
static inline domain::Metrics& METRICS(){ return domain::Metrics::instance(); }
extern "C" const lv_font_t lv_font_source_code_pro_16;
extern "C" const lv_font_t lv_font_montserrat_14;
extern "C" const lv_font_t lv_font_source_code_pro_18;
extern "C" const lv_font_t lv_font_source_code_pro_36;
extern "C" const lv_font_t lv_font_source_code_pro_36_bold;

// ====== USER CONFIG ======
// Set to true for a minimal diagnostic mode (serial prints + color flashes)
static const bool DIAG_MODE = false;
static const uint32_t TUYA_BAUD = 115200; // 115200 default; change to 9600 if needed
static const int RX_A_PIN = 16;          // MCU -> WiFi
static const int RX_B_PIN = 17;          // WiFi -> MCU
static const bool USE_CHANNEL_B = true; // set false if only one direction
// Backlight pin (as in your working example)
static const int LCD_BL_PIN = 23;        // set -1 to disable
// Optional transmit pins to inject Tuya frames (leave -1 for sniff-only)
static const int TX_A_PIN = -1;          // drive MCU<-WiFi line (rarely needed)
static const int TX_B_PIN = -1;          // drive WiFi->MCU line (emulate WiFi)
// Send a small set of Tuya queries on boot if TX pin is configured
static const bool SEND_ON_BOOT = false;  // set true after wiring TX safely
static const uint8_t TUYA_VER = 0x03;    // common Tuya protocol version

// ---- DP mapping (adjust if needed) ----
// VALUE (T2) big-endian, scales below
static const uint8_t DP_TEMP = 8;     // value / 10.0 => °C
static const uint8_t DP_ORP  = 131;   // signed value / 10.0 => mV (negative values seen here)
static const uint8_t DP_PH   = 106;   // value / 100.0 => pH (observed around 12.2..13.6)
static const uint8_t DP_ORP_ALT1 = 122; // alternative ORP (positive range)
static const uint8_t DP_PH_ALT1  = 118; // alternative pH

// If true, show only the key metrics (pH, ORP, Temp) on screen
static const bool SIMPLE_VIEW = true;
static const bool USE_LVGL_UI = true; // switch to LVGL-based UI with swipeable pages
// Safe rollback: keep LVGL minimal to avoid crashes while debugging
static const bool LVGL_SAFE_BASELINE = false;

// ===== LVGL UI globals =====
static lv_obj_t *lv_tv = nullptr;
static lv_obj_t *lv_tile_main = nullptr;
static lv_obj_t *lv_tile_settings = nullptr;
static lv_obj_t *lv_dots = nullptr;
// static lv_obj_t *lv_lbl_dbg = nullptr; // debug label (removed)
// Card containers + titles
static lv_obj_t *lv_card_ph = nullptr;
static lv_obj_t *lv_card_orp = nullptr;
static lv_obj_t *lv_card_temp = nullptr;
static lv_obj_t *lv_lbl_ph_title = nullptr;
static lv_obj_t *lv_lbl_orp_title = nullptr;
static lv_obj_t *lv_lbl_temp_title = nullptr;
static volatile uint32_t lv_flush_count = 0;
static volatile uint32_t lv_touch_press_count = 0;
static volatile int16_t lv_touch_last_x = -1;
static volatile int16_t lv_touch_last_y = -1;
// Touch cache polled by a fast LVGL timer to avoid missing short taps

// Main page widgets
static lv_obj_t *lv_lbl_ph = nullptr;
static lv_obj_t *lv_lbl_orp = nullptr;
static lv_obj_t *lv_lbl_temp = nullptr;
static lv_obj_t *lv_lbl_orp_unit = nullptr;
static lv_obj_t *lv_lbl_ph_shadow = nullptr;
static lv_obj_t *lv_lbl_orp_shadow = nullptr;
static lv_obj_t *lv_img_ph_icon = nullptr;
static lv_obj_t *lv_img_orp_icon = nullptr;
static lv_obj_t *lv_img_pump_ph = nullptr;
static lv_obj_t *lv_img_pump_orp = nullptr;
static lv_obj_t *lv_img_ph_icon_shadow = nullptr;
static lv_obj_t *lv_img_orp_icon_shadow = nullptr;
static lv_obj_t *lv_img_pump_ph_shadow = nullptr;
static lv_obj_t *lv_img_pump_orp_shadow = nullptr;
static lv_obj_t *lv_lbl_ip = nullptr;
static lv_obj_t *lv_lbl_m1 = nullptr; // motor1 indicator
static lv_obj_t *lv_lbl_m2 = nullptr; // motor2 indicator

// Settings widgets
static lv_obj_t *lv_lbl_speed1 = nullptr;
static lv_obj_t *lv_lbl_speed2 = nullptr;

// Forward declarations for LVGL helper functions (definitions are below metrics/globals)
static void lv_update_speed_labels();
static void on_ph_minus_cb(lv_event_t *e);
static void on_ph_plus_cb(lv_event_t *e);
static void on_orp_minus_cb(lv_event_t *e);
static void on_orp_plus_cb(lv_event_t *e);
static void on_speed_save_cb(lv_event_t *e);
static void updateLvglValues();
static void showRangeDialog(bool isPh);
// Enable dummy/test mode to generate values without the meter connected
static const bool DUMMY_MODE = true;  // set true to simulate values
// Tap vs swipe detection for tiles
struct TileTapCtx { bool isPh; lv_point_t start; uint32_t start_ms; bool maybe_tap; };
static void tile_tap_cb(lv_event_t *e){
  TileTapCtx *ctx = (TileTapCtx*)lv_event_get_user_data(e);
  lv_event_code_t code = lv_event_get_code(e);
  if (!ctx) return;
  const int SLOP = 12; // px
  const uint32_t MAX_TAP_MS = 300;
  if (code == LV_EVENT_PRESSED) {
    lv_indev_t *indev = lv_indev_get_act();
    lv_point_t p; p.x = p.y = 0;
    if (indev) lv_indev_get_point(indev, &p);
    ctx->start = p; ctx->start_ms = lv_tick_get(); ctx->maybe_tap = true;
  } else if (code == LV_EVENT_PRESSING) {
    lv_indev_t *indev = lv_indev_get_act();
    lv_point_t p; p.x = p.y = 0;
    if (indev) lv_indev_get_point(indev, &p);
    int dx = LV_ABS(p.x - ctx->start.x);
    int dy = LV_ABS(p.y - ctx->start.y);
    if (dx > SLOP || dy > SLOP) ctx->maybe_tap = false; // it's a swipe/drag
  } else if (code == LV_EVENT_RELEASED) {
    if (ctx->maybe_tap && lv_tick_elaps(ctx->start_ms) <= MAX_TAP_MS) {
      showRangeDialog(ctx->isPh);
    }
  }
}

// ---- TB6612FNG motor driver (optional dosing pumps) ----
static const bool MOTOR_ENABLE = true; // set false to disable all motor control
static const bool MOTOR_TEST   = true; // jog both motors at boot to verify wiring
// Force Motor A continuously on (test). Set true for hard-on at 100% duty.
static const bool FORCE_MOTOR_A_ON = false;

// Pinout (change to suit your wiring). All pins must be 3.3V tolerant GPIOs.
// TB6612 → ESP32-C6 mapping:
//  STBY  → TB_STBY
//  AIN1  → M1_IN1,  AIN2 → M1_IN2,  PWMA → M1_PWM  (Motor 1)
//  BIN1  → M2_IN1,  BIN2 → M2_IN2,  PWMB → M2_PWM  (Motor 2)
// Choose pins that are free; these are not used by LCD/UART.
static const int TB_STBY = 3;  // use free GPIO3 (SPI MISO pad) for STBY
static const int M1_IN1  = 7;
static const int M1_IN2  = 8;
static const int M1_PWM  = 5;  // LEDC PWM
static const int M2_IN1  = 4;
static const int M2_IN2  = 6;
static const int M2_PWM  = 9;  // LEDC PWM

// Control policy thresholds and timing
static float PH_MIN = 6.80f, PH_MAX = 7.60f;   // outside → run Motor1
static int   ORP_MIN = 250, ORP_MAX = 850;     // mV outside → run Motor2
static const uint32_t MOTOR_RUN_MS = 5000;     // run time per correction burst
static uint8_t  M1_SPEED_PC = 60;     // PWM duty % (pH)
static uint8_t  M2_SPEED_PC = 60;     // PWM duty % (ORP)
static const uint32_t MOTOR_COOLDOWN_MS = 2000; // pause after burst

// PWM setup (use Arduino analogWrite APIs for C6)
static const int PWM_FREQ = 10000; // 10 kHz (safe with 8-bit on C6)
static const int PWM_BITS = 8;     // 0..255

// Internal motor state
static uint32_t m1StopAt = 0, m2StopAt = 0;
static uint32_t m1CoolUntil = 0, m2CoolUntil = 0;
static bool m1Running = false, m2Running = false;
// Hysteresis for continuous control (no burst/cooldown)
static const float PH_HYST = 0.05f;   // stop when pH < (PH_MAX - PH_HYST)
static const int   ORP_HYST = 20;     // stop when ORP > (ORP_MIN + ORP_HYST)

// (helpers moved below prefs)

// (helpers moved below metrics and prefs)
// ---- WiFi + MQTT (Home Assistant via Mosquitto) ----
static const char* WIFI_SSID     = "ABERSONPLEIN-IoT";
static const char* WIFI_PASSWORD = "ramonvanbruggen";
static const char* MQTT_HOST     = "192.168.0.248"; // or broker IP
static const uint16_t MQTT_PORT  = 1883;
static const char* MQTT_USER     = "mqqt";  // optional
static const char* MQTT_PASS     = "mqqt";  // optional
static const char* MQTT_CLIENTID = "pool-sniffer-c6";

// MQTT is handled by io::MqttClient now
static core::Storage storage("poolcfg");
static core::DisplayBridge *displayBridge = nullptr;
static domain::ControlPolicy *control = nullptr;
static io::MqttClient mqttClient;

// WiFi event logging
static void setupWiFiEvents() {
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info){
    switch (event) {
      case ARDUINO_EVENT_WIFI_STA_CONNECTED:
        Serial.println("[WiFi] Connected to AP");
        break;
      case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        Serial.print("[WiFi] Got IP: "); Serial.println(WiFi.localIP());
        break;
      case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        Serial.print("[WiFi] Disconnected, reason="); Serial.println((int)info.wifi_sta_disconnected.reason);
        break;
      default: break;
    }
  });
}
// Extra: in DIAG_MODE we try multiple candidates in case of board revision
static const int BL_CANDIDATES[] = {2, 1, 3, 20, 21, 19, 18, 17, 16, 15, 14, 13, 12, 11, 5, 4};
static const size_t BL_CANDIDATES_COUNT = sizeof(BL_CANDIDATES) / sizeof(BL_CANDIDATES[0]);
// =========================

// ---- Display setup (match working HelloWorld) ----
Arduino_DataBus *bus = new Arduino_HWSPI(15 /* DC */, 14 /* CS */, 1 /* SCK */, 2 /* MOSI */, -1 /* MISO */);
Arduino_GFX *gfx = new Arduino_ST7789(bus, 22 /* RST */, 0 /* rotation */, false /* IPS */,
                                      172 /* width */, 320 /* height */,
                                      34 /* col_offset1 */, 0 /* row_offset1 */,
                                      34 /* col_offset2 */, 0 /* row_offset2 */);

// Optional vendor init sequence used by the HelloWorld that worked for you
static void lcd_reg_init(void) {
  static const uint8_t init_operations[] = {
    BEGIN_WRITE,
    WRITE_COMMAND_8, 0x11,
    END_WRITE,
    DELAY, 120,

    BEGIN_WRITE,
    WRITE_C8_D16, 0xDF, 0x98, 0x53,
    WRITE_C8_D8, 0xB2, 0x23,

    WRITE_COMMAND_8, 0xB7,
    WRITE_BYTES, 4,
    0x00, 0x47, 0x00, 0x6F,

    WRITE_COMMAND_8, 0xBB,
    WRITE_BYTES, 6,
    0x1C, 0x1A, 0x55, 0x73, 0x63, 0xF0,

    WRITE_C8_D16, 0xC0, 0x44, 0xA4,
    WRITE_C8_D8, 0xC1, 0x16,

    WRITE_COMMAND_8, 0xC3,
    WRITE_BYTES, 8,
    0x7D, 0x07, 0x14, 0x06, 0xCF, 0x71, 0x72, 0x77,

    WRITE_COMMAND_8, 0xC4,
    WRITE_BYTES, 12,
    0x00, 0x00, 0xA0, 0x79, 0x0B, 0x0A, 0x16, 0x79, 0x0B, 0x0A, 0x16, 0x82,

    WRITE_COMMAND_8, 0xC8,
    WRITE_BYTES, 32,
    0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28, 0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00, 0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28, 0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,

    WRITE_COMMAND_8, 0xD0,
    WRITE_BYTES, 5,
    0x04, 0x06, 0x6B, 0x0F, 0x00,

    WRITE_C8_D16, 0xD7, 0x00, 0x30,
    WRITE_C8_D8, 0xE6, 0x14,
    WRITE_C8_D8, 0xDE, 0x01,

    WRITE_COMMAND_8, 0xB7,
    WRITE_BYTES, 5,
    0x03, 0x13, 0xEF, 0x35, 0x35,

    WRITE_COMMAND_8, 0xC1,
    WRITE_BYTES, 3,
    0x14, 0x15, 0xC0,

    WRITE_C8_D16, 0xC2, 0x06, 0x3A,
    WRITE_C8_D16, 0xC4, 0x72, 0x12,
    WRITE_C8_D8, 0xBE, 0x00,
    WRITE_C8_D8, 0xDE, 0x02,

    WRITE_COMMAND_8, 0xE5,
    WRITE_BYTES, 3,
    0x00, 0x02, 0x00,

    WRITE_COMMAND_8, 0xE5,
    WRITE_BYTES, 3,
    0x01, 0x02, 0x00,

    WRITE_C8_D8, 0xDE, 0x00,
    WRITE_C8_D8, 0x35, 0x00,
    WRITE_C8_D8, 0x3A, 0x05,

    WRITE_COMMAND_8, 0x2A,
    WRITE_BYTES, 4,
    0x00, 0x22, 0x00, 0xCD,

    WRITE_COMMAND_8, 0x2B,
    WRITE_BYTES, 4,
    0x00, 0x00, 0x01, 0x3F,

    WRITE_C8_D8, 0xDE, 0x02,

    WRITE_COMMAND_8, 0xE5,
    WRITE_BYTES, 3,
    0x00, 0x02, 0x00,

    WRITE_C8_D8, 0xDE, 0x00,
    WRITE_C8_D8, 0x36, 0x00,
    WRITE_COMMAND_8, 0x21,
    END_WRITE,

    DELAY, 10,

    BEGIN_WRITE,
    WRITE_COMMAND_8, 0x29,
    END_WRITE
  };
  bus->batchOperation(init_operations, sizeof(init_operations));
}

// ---- UARTs ----
#ifndef ARDUINO_USB_CDC_ON_BOOT
#define ARDUINO_USB_CDC_ON_BOOT 1
#endif
HardwareSerial TUYA_A(0); // UART0 RX-only
HardwareSerial TUYA_B(1); // UART1 RX-only

// ---- Tuya helpers ----
uint8_t tuyaChecksum(const uint8_t* p, size_t n) { uint32_t s=0; for (size_t i=0;i<n;i++) s+=p[i]; return (uint8_t)(s & 0xFF); }
void hexByte(Print &out, uint8_t b){ static const char* H="0123456789ABCDEF"; char t[3]={H[b>>4],H[b&0xF],0}; out.print(t); }

// ---- Tuya TX helpers (optional) ----
static void tuyaSendFrame(HardwareSerial &port, uint8_t cmd, const uint8_t* data, uint16_t len) {
  uint8_t hdr[4] = {0x55, 0xAA, TUYA_VER, cmd};
  port.write(hdr, 4);
  uint8_t l[2] = { (uint8_t)(len >> 8), (uint8_t)(len & 0xFF) };
  port.write(l, 2);
  if (len) port.write(data, len);
  uint8_t chkBuf[6 + (len ? len : 0)];
  memcpy(chkBuf, hdr, 4);
  memcpy(chkBuf+4, l, 2);
  if (len) memcpy(chkBuf+6, data, len);
  port.write(tuyaChecksum(chkBuf, 6+len));
}

static void tuyaSendDpQuery(HardwareSerial &port) { // cmd 0x10
  tuyaSendFrame(port, 0x10, nullptr, 0);
}

static void tuyaSendQueryProductInfo(HardwareSerial &port) { // cmd 0x01
  tuyaSendFrame(port, 0x01, nullptr, 0);
}

static void tuyaSendSetWifiStatus(HardwareSerial &port, uint8_t status) { // cmd 0x03
  uint8_t d[1] = { status }; // e.g., 0x00 smartconfig, 0x02 connected
  tuyaSendFrame(port, 0x03, d, 1);
}

// ---- Simple ring buffer for lines on screen ----
static const uint16_t MAX_LINES = 6;       // lines in landscape with larger font
static const uint16_t MAX_LINE_CHARS = 22; // clipped per line (size=2)

std::vector<String> lines;

// Live RX counters to verify activity even if frames don't parse
static uint32_t rxA_count = 0;
static uint32_t rxB_count = 0;
static uint8_t lastA[7];
static uint8_t lastB[7];
static uint8_t idxA = 0;
static uint8_t idxB = 0;
static String asciiA;
static String asciiB;
static uint8_t rawCountA = 0;
static uint8_t rawCountB = 0;

// Live metrics (persist and render in header)
// Metrics moved to domain::Metrics (singleton). Temporary aliases are provided in domain/Metrics.h

// ===== LVGL helpers (definitions) =====
static void lv_update_speed_labels(){
  if (lv_lbl_speed1) lv_label_set_text_fmt(lv_lbl_speed1, "%u%%", (unsigned)M1_SPEED_PC);
  if (lv_lbl_speed2) lv_label_set_text_fmt(lv_lbl_speed2, "%u%%", (unsigned)M2_SPEED_PC);
}
static void on_ph_minus_cb(lv_event_t *e){ (void)e; if (M1_SPEED_PC>=5) M1_SPEED_PC-=5; else M1_SPEED_PC=0; lv_update_speed_labels(); }
static void on_ph_plus_cb (lv_event_t *e){ (void)e; if (M1_SPEED_PC<=95) M1_SPEED_PC+=5; else M1_SPEED_PC=100; lv_update_speed_labels(); }
static void on_orp_minus_cb(lv_event_t *e){ (void)e; if (M2_SPEED_PC>=5) M2_SPEED_PC-=5; else M2_SPEED_PC=0; lv_update_speed_labels(); }
static void on_orp_plus_cb (lv_event_t *e){ (void)e; if (M2_SPEED_PC<=95) M2_SPEED_PC+=5; else M2_SPEED_PC=100; lv_update_speed_labels(); }
static void on_speed_save_cb(lv_event_t *e){ (void)e; storage.setM1Speed(M1_SPEED_PC); storage.setM2Speed(M2_SPEED_PC); }

// Alert margins and border thickness for near/exceed thresholds (used by LVGL updater)
static const float WARN_MARGIN_PH = 0.05f;   // pH within 0.05 of min/max
static const int   WARN_MARGIN_ORP = 20;     // mV within 20 of min/max

static void updateLvglValues(){
  if (!USE_LVGL_UI) return;
  if (lv_lbl_ph) {
    if (METRICS().havePh) { char b[24]; snprintf(b, sizeof(b), "%.2f", (double)METRICS().phVal); lv_label_set_text(lv_lbl_ph, b); }
    else lv_label_set_text(lv_lbl_ph, "--.--");
    // Color by thresholds (red out-of-range, orange near limits, else white)
    lv_color_t phColor = lv_color_white();
    if (METRICS().havePh) {
      bool below = METRICS().phVal < PH_MIN;
      bool above = METRICS().phVal > PH_MAX;
      bool warn = (!below && !above) && (METRICS().phVal <= PH_MIN + WARN_MARGIN_PH || METRICS().phVal >= PH_MAX - WARN_MARGIN_PH);
      phColor = below || above ? lv_palette_main(LV_PALETTE_RED) : (warn ? lv_palette_main(LV_PALETTE_ORANGE) : lv_color_white());
    }
    lv_obj_set_style_text_color(lv_lbl_ph, phColor, 0);
    // Mirror text into shadow layer
    if (lv_lbl_ph_shadow) lv_label_set_text(lv_lbl_ph_shadow, lv_label_get_text(lv_lbl_ph));
  }
  if (lv_lbl_orp) {
    if (METRICS().haveOrp) {
      char b[16]; snprintf(b, sizeof(b), "%d", (int)lrintf(METRICS().orpMv));
      lv_label_set_text(lv_lbl_orp, b);
      if (lv_lbl_orp_unit) lv_obj_clear_flag(lv_lbl_orp_unit, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_label_set_text(lv_lbl_orp, "----");
      if (lv_lbl_orp_unit) lv_obj_clear_flag(lv_lbl_orp_unit, LV_OBJ_FLAG_HIDDEN);
    }
    // Color by thresholds for ORP (red out-of-range, orange near limit, else white)
    lv_color_t orpColor = lv_color_white();
    if (METRICS().haveOrp) {
      int v = (int)lrintf(METRICS().orpMv);
      bool low = v < ORP_MIN; bool high = v > ORP_MAX;
      bool warn = (!low && !high) && (v <= ORP_MIN + WARN_MARGIN_ORP || v >= ORP_MAX - WARN_MARGIN_ORP);
      if (low || high)      orpColor = lv_palette_main(LV_PALETTE_RED);
      else if (warn)        orpColor = lv_palette_main(LV_PALETTE_ORANGE);
      else                  orpColor = lv_color_white();
    }
    lv_obj_set_style_text_color(lv_lbl_orp, orpColor, 0);
    if (lv_lbl_orp_unit) lv_obj_set_style_text_color(lv_lbl_orp_unit, lv_color_white(), 0);
    // Mirror text into shadow layer
    if (lv_lbl_orp_shadow) lv_label_set_text(lv_lbl_orp_shadow, lv_label_get_text(lv_lbl_orp));
  }
  if (lv_lbl_temp) {
    if (METRICS().haveTemp) { char b[24]; snprintf(b, sizeof(b), "%.1f C", (double)METRICS().tempC); lv_label_set_text(lv_lbl_temp, b); }
    else lv_label_set_text(lv_lbl_temp, "--.- C");
  }
  if (lv_lbl_ip) {
    String ip = (WiFi.status()==WL_CONNECTED)? WiFi.localIP().toString() : String("--");
    char b[48]; snprintf(b, sizeof(b), "IP: %s", ip.c_str()); lv_label_set_text(lv_lbl_ip, b);
  }
  if (lv_img_pump_ph) {
    if (m1Running) {
      lv_obj_clear_flag(lv_img_pump_ph, LV_OBJ_FLAG_HIDDEN);
      if (lv_img_pump_ph_shadow) lv_obj_clear_flag(lv_img_pump_ph_shadow, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(lv_img_pump_ph, LV_OBJ_FLAG_HIDDEN);
      if (lv_img_pump_ph_shadow) lv_obj_add_flag(lv_img_pump_ph_shadow, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (lv_img_pump_orp) {
    if (m2Running) {
      lv_obj_clear_flag(lv_img_pump_orp, LV_OBJ_FLAG_HIDDEN);
      if (lv_img_pump_orp_shadow) lv_obj_clear_flag(lv_img_pump_orp_shadow, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(lv_img_pump_orp, LV_OBJ_FLAG_HIDDEN);
      if (lv_img_pump_orp_shadow) lv_obj_add_flag(lv_img_pump_orp_shadow, LV_OBJ_FLAG_HIDDEN);
    }
  }

  // Card background color by status
  if (lv_card_ph) {
    lv_color_t base = lv_palette_main(LV_PALETTE_BLUE);
    if (METRICS().havePh) {
      bool below = METRICS().phVal < PH_MIN; bool above = METRICS().phVal > PH_MAX;
      bool warn = (!below && !above) && (METRICS().phVal <= PH_MIN + WARN_MARGIN_PH || METRICS().phVal >= PH_MAX - WARN_MARGIN_PH);
      base = below || above ? lv_palette_main(LV_PALETTE_RED) : (warn ? lv_palette_main(LV_PALETTE_ORANGE) : lv_palette_main(LV_PALETTE_BLUE));
    }
    lv_obj_set_style_bg_color(lv_card_ph, base, 0);
  }
  if (lv_card_orp) {
    lv_color_t base = lv_palette_main(LV_PALETTE_RED);
    if (METRICS().haveOrp) {
      int v = (int)lrintf(METRICS().orpMv);
      bool low = v < ORP_MIN; bool high = v > ORP_MAX;
      bool warn = (!low && !high) && (v <= ORP_MIN + WARN_MARGIN_ORP || v >= ORP_MAX - WARN_MARGIN_ORP);
      base = low || high ? lv_palette_main(LV_PALETTE_RED) : (warn ? lv_palette_main(LV_PALETTE_ORANGE) : lv_palette_main(LV_PALETTE_GREEN));
    }
    lv_obj_set_style_bg_color(lv_card_orp, base, 0);
  }
  if (lv_card_temp) {
    lv_color_t base = lv_palette_main(LV_PALETTE_GREEN);
    lv_obj_set_style_bg_color(lv_card_temp, base, 0);
  }
}

// Ensure LVGL centers the first tile after the first few ticks (post-layout)
static void lv_fix_initial_layout(lv_timer_t *t){
  (void)t;
  if (lv_tv && lv_tile_main){
    lv_obj_set_tile(lv_tv, lv_tile_main, LV_ANIM_OFF);
    lv_obj_scroll_to_x(lv_tv, 0, LV_ANIM_OFF);
    lv_obj_scroll_to_y(lv_tv, 0, LV_ANIM_OFF);
  }
}

// Modal dialog to change min/max for pH or ORP
static void showRangeDialog(bool isPh){
  const char *title = isPh ? "pH range" : "ORP range";
  lv_obj_t *modal = lv_obj_create(lv_layer_top());
  lv_obj_set_size(modal, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
  lv_obj_set_style_bg_opa(modal, LV_OPA_50, 0);
  lv_obj_set_style_bg_color(modal, lv_color_black(), 0);
  // No scrollbars on modal overlay
  lv_obj_clear_flag(modal, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(modal, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_bg_opa(modal, LV_OPA_TRANSP, LV_PART_SCROLLBAR);

  // Disable tileview scrolling while dialog is open
  if (lv_tv) lv_obj_clear_flag(lv_tv, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *dlg = lv_obj_create(modal);
  lv_obj_set_size(dlg, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
  lv_obj_center(dlg);
  lv_obj_set_style_radius(dlg, 10, 0);
  lv_obj_set_style_pad_all(dlg, 8, 0);
  // No scrollbars on dialog window
  lv_obj_clear_flag(dlg, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(dlg, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_bg_opa(dlg, LV_OPA_TRANSP, LV_PART_SCROLLBAR);

  lv_obj_t *lbl = lv_label_create(dlg); lv_label_set_text(lbl, title); lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, 0);

  // Min
  lv_obj_t *minLbl = lv_label_create(dlg); lv_label_set_text(minLbl, "Min:"); lv_obj_align(minLbl, LV_ALIGN_LEFT_MID, 0, -20);
  lv_obj_t *taMin = lv_textarea_create(dlg); lv_obj_set_width(taMin, 80); lv_obj_align_to(taMin, minLbl, LV_ALIGN_OUT_RIGHT_MID, 6, 0);
  // Hide any text area scrollbars
  lv_obj_set_scrollbar_mode(taMin, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_bg_opa(taMin, LV_OPA_TRANSP, LV_PART_SCROLLBAR);
  char bmin[16]; if (isPh) snprintf(bmin,sizeof(bmin),"%.2f", PH_MIN); else snprintf(bmin,sizeof(bmin),"%d", ORP_MIN); lv_textarea_set_text(taMin, bmin);

  // Max
  lv_obj_t *maxLbl = lv_label_create(dlg); lv_label_set_text(maxLbl, "Max:"); lv_obj_align(maxLbl, LV_ALIGN_LEFT_MID, 0, 20);
  lv_obj_t *taMax = lv_textarea_create(dlg); lv_obj_set_width(taMax, 80); lv_obj_align_to(taMax, maxLbl, LV_ALIGN_OUT_RIGHT_MID, 6, 0);
  lv_obj_set_scrollbar_mode(taMax, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_bg_opa(taMax, LV_OPA_TRANSP, LV_PART_SCROLLBAR);
  char bmax[16]; if (isPh) snprintf(bmax,sizeof(bmax),"%.2f", PH_MAX); else snprintf(bmax,sizeof(bmax),"%d", ORP_MAX); lv_textarea_set_text(taMax, bmax);

  // Buttons
  lv_obj_t *btnCancel = lv_btn_create(dlg); lv_obj_set_size(btnCancel, 80, 32); lv_obj_align(btnCancel, LV_ALIGN_BOTTOM_LEFT, 0, 0); lv_label_set_text(lv_label_create(btnCancel), "Cancel");
  lv_obj_t *btnSave   = lv_btn_create(dlg); lv_obj_set_size(btnSave, 80, 32); lv_obj_align(btnSave, LV_ALIGN_BOTTOM_RIGHT, 0, 0); lv_label_set_text(lv_label_create(btnSave), "Save");

  struct RangeCtx { bool isPh; lv_obj_t *taMin; lv_obj_t *taMax; lv_obj_t *dlg; lv_obj_t *modal; lv_obj_t *tv; lv_obj_t *lblMin; lv_obj_t *lblMax; };
  RangeCtx *ctx = (RangeCtx*)lv_mem_alloc(sizeof(RangeCtx));
  ctx->isPh = isPh; ctx->taMin = taMin; ctx->taMax = taMax; ctx->dlg = dlg; ctx->modal = modal; ctx->tv = lv_tv;

  // Replace textareas with sliders UI
  // Clean dialog content and rebuild compact slider layout
  lv_obj_clean(dlg);
  lv_obj_set_style_pad_all(dlg, 10, 0);

  // Title
  lv_obj_t *titleLbl = lv_label_create(dlg);
  lv_label_set_text(titleLbl, title);
  lv_obj_align(titleLbl, LV_ALIGN_TOP_MID, 0, 0);

  // Labels to show values
  lv_obj_t *lblMin = lv_label_create(dlg);
  lv_obj_t *lblMax = lv_label_create(dlg);
  // Initial placement; final alignment is tied to sliders below
  lv_obj_align(lblMin, LV_ALIGN_LEFT_MID, 0, -25);
  lv_obj_align(lblMax, LV_ALIGN_LEFT_MID, 0, 15);

  // Sliders
  lv_obj_t *slMin = lv_slider_create(dlg);
  lv_obj_set_width(slMin, lv_obj_get_width(dlg) - 120);
  // Move sliders ~10px up versus original (-20 -> -30, 20 -> 10)
  lv_obj_align(slMin, LV_ALIGN_RIGHT_MID, -10, -35);
  lv_obj_t *slMax = lv_slider_create(dlg);
  lv_obj_set_width(slMax, lv_obj_get_width(dlg) - 120);
  lv_obj_align(slMax, LV_ALIGN_RIGHT_MID, -10, 5);

  // Configure ranges and initial values
  if (isPh) {
    lv_label_set_text(lblMin, "Min:");
    lv_label_set_text(lblMax, "Max:");
    lv_slider_set_range(slMin, 0, 1400);
    lv_slider_set_range(slMax, 0, 1400);
    lv_slider_set_value(slMin, (int)(PH_MIN * 100.0f), LV_ANIM_OFF);
    lv_slider_set_value(slMax, (int)(PH_MAX * 100.0f), LV_ANIM_OFF);
  } else {
    lv_label_set_text(lblMin, "Min (mV):");
    lv_label_set_text(lblMax, "Max (mV):");
    lv_slider_set_range(slMin, 0, 3000);
    lv_slider_set_range(slMax, 0, 3000);
    lv_slider_set_value(slMin, ORP_MIN, LV_ANIM_OFF);
    lv_slider_set_value(slMax, ORP_MAX, LV_ANIM_OFF);
  }

  // Show current values next to labels
  auto update_value_labels = [&](RangeCtx *c){
    if (c->isPh) {
      char b1[16], b2[16];
      snprintf(b1, sizeof(b1), "%.2f", lv_slider_get_value(slMin) / 100.0f);
      snprintf(b2, sizeof(b2), "%.2f", lv_slider_get_value(slMax) / 100.0f);
      lv_label_set_text_fmt(lblMin, "Min: %s", b1);
      lv_label_set_text_fmt(lblMax, "Max: %s", b2);
    } else {
      lv_label_set_text_fmt(lblMin, "Min (mV): %d", (int)lv_slider_get_value(slMin));
      lv_label_set_text_fmt(lblMax, "Max (mV): %d", (int)lv_slider_get_value(slMax));
    }
  };

  // Extend ctx to carry new widgets
  ctx->taMin = slMin; ctx->taMax = slMax; ctx->lblMin = lblMin; ctx->lblMax = lblMax;

  update_value_labels(ctx);

  // Ensure labels are vertically centered to their sliders and sit just to the left
  lv_obj_align_to(lblMin, slMin, LV_ALIGN_OUT_LEFT_MID, -6, 0);
  lv_obj_align_to(lblMax, slMax, LV_ALIGN_OUT_LEFT_MID, -6, 0);

  // Enforce constraints and update labels on slider change
  lv_obj_add_event_cb(slMin, [](lv_event_t *e){
    RangeCtx *c = (RangeCtx*)lv_event_get_user_data(e);
    lv_obj_t *slMinL = c->taMin;
    lv_obj_t *slMaxL = c->taMax;
    int minv = lv_slider_get_value(slMinL);
    int maxv = lv_slider_get_value(slMaxL);
    int step = 1;
    if (minv > maxv - step) { minv = maxv - step; lv_slider_set_value(slMinL, minv, LV_ANIM_OFF); }
    if (c->isPh) {
      lv_label_set_text_fmt(c->lblMin, "Min: %.2f", minv / 100.0f);
      lv_label_set_text_fmt(c->lblMax, "Max: %.2f", lv_slider_get_value(slMaxL) / 100.0f);
    } else {
      lv_label_set_text_fmt(c->lblMin, "Min (mV): %d", minv);
      lv_label_set_text_fmt(c->lblMax, "Max (mV): %d", (int)lv_slider_get_value(slMaxL));
    }
  }, LV_EVENT_VALUE_CHANGED, ctx);

  lv_obj_add_event_cb(slMax, [](lv_event_t *e){
    RangeCtx *c = (RangeCtx*)lv_event_get_user_data(e);
    lv_obj_t *slMaxL = c->taMax;
    lv_obj_t *slMinL = c->taMin;
    int minv = lv_slider_get_value(slMinL);
    int maxv = lv_slider_get_value(slMaxL);
    int step = 1;
    if (maxv < minv + step) { maxv = minv + step; lv_slider_set_value(slMaxL, maxv, LV_ANIM_OFF); }
    if (c->isPh) {
      lv_label_set_text_fmt(c->lblMin, "Min: %.2f", lv_slider_get_value(slMinL) / 100.0f);
      lv_label_set_text_fmt(c->lblMax, "Max: %.2f", maxv / 100.0f);
    } else {
      lv_label_set_text_fmt(c->lblMin, "Min (mV): %d", (int)lv_slider_get_value(slMinL));
      lv_label_set_text_fmt(c->lblMax, "Max (mV): %d", maxv);
    }
  }, LV_EVENT_VALUE_CHANGED, ctx);

  // Large buttons
  lv_obj_t *btnCancel2 = lv_btn_create(dlg); lv_obj_set_size(btnCancel2, 120, 44); lv_obj_align(btnCancel2, LV_ALIGN_BOTTOM_LEFT, 0, 0); lv_label_set_text(lv_label_create(btnCancel2), "Cancel");
  lv_obj_t *btnSave2   = lv_btn_create(dlg); lv_obj_set_size(btnSave2, 120, 44); lv_obj_align(btnSave2, LV_ALIGN_BOTTOM_RIGHT, 0, 0); lv_label_set_text(lv_label_create(btnSave2), "Save");

  // Cancel closes modal and re-enables swiping
  lv_obj_add_event_cb(btnCancel2, [](lv_event_t *e){
    RangeCtx *c = (RangeCtx*)lv_event_get_user_data(e);
    if (c->tv) lv_obj_add_flag(c->tv, LV_OBJ_FLAG_SCROLLABLE);
    if (c->modal) lv_obj_del(c->modal);
    lv_mem_free(c);
  }, LV_EVENT_CLICKED, ctx);

  // Save applies values then closes modal and re-enables swiping
  lv_obj_add_event_cb(btnSave2, [](lv_event_t *e){
    RangeCtx *c = (RangeCtx*)lv_event_get_user_data(e);
    // Retrieve sliders from ctx
    lv_obj_t *slMinL = c->taMin;
    lv_obj_t *slMaxL = c->taMax;
    if (c->isPh) { PH_MIN = lv_slider_get_value(slMinL) / 100.0f; PH_MAX = lv_slider_get_value(slMaxL) / 100.0f; }
    else { ORP_MIN = lv_slider_get_value(slMinL); ORP_MAX = lv_slider_get_value(slMaxL); }
    storage.setPhMin(PH_MIN); storage.setPhMax(PH_MAX);
    storage.setOrpMin(ORP_MIN); storage.setOrpMax(ORP_MAX);
    updateLvglValues();
    if (c->tv) lv_obj_add_flag(c->tv, LV_OBJ_FLAG_SCROLLABLE);
    if (c->modal) lv_obj_del(c->modal);
    lv_mem_free(c);
  }, LV_EVENT_CLICKED, ctx);

  // Ensure restoring scrollable on modal delete (safety)
  lv_obj_add_event_cb(modal, [](lv_event_t *e){
    if (lv_event_get_code(e) == LV_EVENT_DELETE) {
      if (lv_tv) lv_obj_add_flag(lv_tv, LV_OBJ_FLAG_SCROLLABLE);
    }
  }, LV_EVENT_ALL, NULL);
}

// ---- Simple vector icons (drawn with primitives) ----
static void drawDropletIcon(Arduino_GFX *gfx, int x, int y, uint16_t color) {
  // tip at (x, y), bulb below
  gfx->fillTriangle(x, y, x-7, y+10, x+7, y+10, color);
  gfx->fillCircle(x, y+14, 10, color);
}

static void drawBoltIcon(Arduino_GFX *gfx, int x, int y, uint16_t color) {
  // simple stylized lightning bolt
  int x0=x, y0=y;
  gfx->fillTriangle(x0, y0, x0+10, y0, x0-6, y0+18, color);
  gfx->fillTriangle(x0+4, y0+12, x0+16, y0+12, x0-2, y0+30, color);
}

static void drawThermoIcon(Arduino_GFX *gfx, int x, int y, uint16_t color) {
  // thermometer: stem with bulb at bottom
  gfx->fillRoundRect(x, y, 10, 26, 4, color);
  gfx->fillCircle(x+5, y+26, 8, color);
}

// --- UI layout constants (for SIMPLE_VIEW) ---
static const int UI_MID_X = 160;
static const int UI_PAD   = 6;
static int ui_lx = UI_PAD + 14;          // left column x (more inset)
static int ui_ly = 16;                   // left column y (lower)
static int ui_rx = UI_MID_X + UI_PAD + 14; // right column x (more inset)
static int ui_ry = 16;                   // right column y (lower)
static int ui_by = 142;                  // bottom row y
// Value redraw boxes
static const int PH_BOX_X   = UI_PAD + 14;
static const int PH_BOX_Y   = 48;  // move values a bit lower, away from labels
static const int PH_BOX_W   = 140;
static const int PH_BOX_H   = 44;
static const int ORP_BOX_X  = UI_MID_X + UI_PAD + 14;
static const int ORP_BOX_Y  = 48;  // move values a bit lower, away from labels
static const int ORP_BOX_W  = 136; // keep inside right frame
static const int ORP_BOX_H  = 44;
static const int TEMP_BOX_X = UI_PAD + 14 + 22;
static const int TEMP_BOX_Y = 132;
// Bottom-right IP address area
static const int IP_BOX_X   = UI_MID_X + UI_PAD + 4; // shift left to allow more width
static const int IP_BOX_Y   = 145; // move 10 px further down
static const int IP_BOX_W   = 146; // wider but still inside frame
static const int IP_BOX_H   = 20;  // a bit taller
static const int TEMP_BOX_W = 100;
static const int TEMP_BOX_H = 20;

// Alert margins and border thickness for near/exceed thresholds
// static const int   ALERT_BORDER_T = 3; // thick border size in pixels (unused in LVGL cards)

// Cached last shown values to avoid flicker and pointless redraws
static int lastPhScaled  = INT32_MIN;  // pH*100
static int lastOrpInt    = INT32_MIN;  // mV
static int lastTempScaled= INT32_MIN;  // C*10
static bool staticDrawn  = false;

// Off-screen buffers to render text without visible clearing (reduce flicker)
static GFXcanvas16 *phCanvas = nullptr;
static GFXcanvas16 *orpCanvas = nullptr;
static GFXcanvas16 *tempCanvas = nullptr;
static GFXcanvas16 *ipCanvas = nullptr;
static String shownIp;
// When true, suppress dynamic redraw of value areas (shown in modal overlay)
static bool UI_OVERLAY_ACTIVE = false;
// Swipe/tap gesture state
static uint8_t currentPage = 0; // 0=main, 1=settings
static bool gestureActive = false;
static int16_t touchStartX = 0, touchStartY = 0;
static int16_t touchLastX = 0, touchLastY = 0;  // Track last position during gesture
static uint32_t touchStartMs = 0;
static const int SWIPE_MIN_PX = 30;  // Lower threshold for swipe detection
static const int TAP_SLOP_PX = 15;   // Max movement for tap
static const uint32_t TAP_DECIDE_MS = 400;  // Max duration for tap
static bool touchHandled = false;  // Prevent multiple actions per touch

// ---- Pagination dots ----
static void drawPagination(){
  int y = 135; // Move higher to avoid overlap with IP
  int cx = 160; // center
  uint16_t inactive = DARKGREY, active = CYAN;
  // Clear area first
  gfx->fillRect(cx-15, y-5, 30, 10, BLACK);
  // two dots spaced by 14 px
  gfx->fillCircle(cx-7, y, 3, currentPage==0 ? active : inactive);
  gfx->fillCircle(cx+7, y, 3, currentPage==1 ? active : inactive);
}

// Small motor activity icon near value boxes
static int M1_ICON_X = PH_BOX_X + PH_BOX_W - 14;
static int M1_ICON_Y = PH_BOX_Y + PH_BOX_H - 12;
static int M2_ICON_X = ORP_BOX_X + ORP_BOX_W - 14;
static int M2_ICON_Y = ORP_BOX_Y + ORP_BOX_H - 12;
static bool lastM1Icon=false, lastM2Icon=false;

static void drawMotorIcon(Arduino_GFX *gfx, int x, int y, bool on) {
  // Erase area first
  gfx->fillRect(x-6, y-6, 12, 12, BLACK);
  if (!on) return;
  // Simple gear-like icon: ring + three spokes
  gfx->drawCircle(x, y, 5, GREEN);
  gfx->drawCircle(x, y, 2, GREEN);
  gfx->drawLine(x, y, x+4, y, GREEN);
  gfx->drawLine(x, y, x-2, y+3, GREEN);
  gfx->drawLine(x, y, x-2, y-3, GREEN);
}

static void drawStaticUI() {
  if (!SIMPLE_VIEW) return;
  if (staticDrawn) return;
  staticDrawn = true;

  gfx->fillScreen(BLACK);

  // Outer frame
  gfx->drawRoundRect(2, 2, 316, 168, 10, CYAN);
  gfx->drawRoundRect(4, 4, 312, 164, 10, DARKGREEN);
  // Column separator
  gfx->drawLine(UI_MID_X, 8, UI_MID_X, 164, DARKGREY);

  // Left (pH)
  drawDropletIcon(gfx, ui_lx-4, ui_ly+2, CYAN);
  gfx->setTextColor(WHITE);
  gfx->setFont(nullptr);
  gfx->setTextSize(2);
  gfx->setCursor(ui_lx+18, ui_ly+14);
  gfx->print("pH");

  // Right (ORP)
  drawBoltIcon(gfx, ui_rx-6, ui_ry+2, YELLOW);
  gfx->setFont(nullptr);
  gfx->setTextSize(2); // gelijk aan pH label
  gfx->setCursor(ui_rx+14, ui_ry+14);
  gfx->print("ORP");

  // (mV label wordt naast het ORP-waardevak getekend wanneer de waarde wordt geüpdatet)

  // Verplaats motor iconen achter labels (boven de waardevelden)
  M1_ICON_X = ui_lx + 60; M1_ICON_Y = ui_ly + 14;
  M2_ICON_X = ui_rx + 68; M2_ICON_Y = ui_ry + 14;

  // Bottom temperature label/icon
  drawThermoIcon(gfx, ui_lx-4, ui_by-14, ORANGE);
  // Restore default font after custom fonts
  gfx->setFont(nullptr);

  // Force first-time numeric rendering of placeholders/values
  lastPhScaled = INT32_MAX;
  lastOrpInt = INT32_MAX;
  lastTempScaled = INT32_MAX;

  // Create canvases once
  if (!phCanvas)   phCanvas   = new GFXcanvas16(PH_BOX_W, PH_BOX_H);
  if (!orpCanvas)  orpCanvas  = new GFXcanvas16(ORP_BOX_W, ORP_BOX_H);
  if (!tempCanvas) tempCanvas = new GFXcanvas16(TEMP_BOX_W, TEMP_BOX_H);
  if (!ipCanvas)   ipCanvas   = new GFXcanvas16(IP_BOX_W, IP_BOX_H);

  // Draw pagination
  drawPagination();
}

static void updateValueAreas() {
  if (USE_LVGL_UI) { updateLvglValues(); return; }
  if (!SIMPLE_VIEW) return;
  if (UI_OVERLAY_ACTIVE) return; // avoid drawing over the modal overlay

  // pH value box (draw to canvas, then blit)
  int phScaled = METRICS().havePh ? (int)lrintf(METRICS().phVal * 100.0f) : INT32_MIN;
  if (phScaled != lastPhScaled) {
    lastPhScaled = phScaled;
    if (phCanvas) {
      phCanvas->fillScreen(BLACK);
      phCanvas->setFont(&FreeSansBold24pt7b);
      uint16_t c = WHITE;
      if (METRICS().havePh) {
        bool nearMin = METRICS().phVal <= PH_MIN + WARN_MARGIN_PH;
        bool nearMax = METRICS().phVal >= PH_MAX - WARN_MARGIN_PH;
        bool below   = METRICS().phVal < PH_MIN;
        bool above   = METRICS().phVal > PH_MAX;
        if (below || above) c = RED;
        else if (nearMin || nearMax) c = ORANGE;
      }
      phCanvas->setTextColor(c);
      phCanvas->setCursor(0, 34);
      if (METRICS().havePh) { char b[16]; snprintf(b,sizeof(b),"%.2f", METRICS().phVal); phCanvas->print(b); } else { phCanvas->print("--.--"); }
      gfx->draw16bitRGBBitmap(PH_BOX_X, PH_BOX_Y, phCanvas->getBuffer(), PH_BOX_W, PH_BOX_H);
      phCanvas->setFont(nullptr);
    }
  }

  // ORP value box (canvas)
  int orpInt = METRICS().haveOrp ? (int)lrintf(METRICS().orpMv) : INT32_MIN;
  if (orpInt != lastOrpInt) {
    lastOrpInt = orpInt;
    if (orpCanvas) {
      orpCanvas->fillScreen(BLACK);
      orpCanvas->setFont(&FreeSansBold24pt7b);
      uint16_t c = WHITE;
      if (METRICS().haveOrp) {
        bool nearMin = orpInt <= ORP_MIN + WARN_MARGIN_ORP;
        bool nearMax = orpInt >= ORP_MAX - WARN_MARGIN_ORP;
        bool below   = orpInt < ORP_MIN;
        bool above   = orpInt > ORP_MAX;
        if (below || above) c = RED;
        else if (nearMin || nearMax) c = ORANGE;
      }
      orpCanvas->setTextColor(c);
      orpCanvas->setCursor(0, 34);
      char vb[16];
      if (METRICS().haveOrp) { snprintf(vb,sizeof(vb),"%d", orpInt); orpCanvas->print(vb); } else { strcpy(vb, "----"); orpCanvas->print(vb); }
      gfx->draw16bitRGBBitmap(ORP_BOX_X, ORP_BOX_Y, orpCanvas->getBuffer(), ORP_BOX_W, ORP_BOX_H);
      orpCanvas->setFont(nullptr);
      // Draw unit label just after the rendered number width, clamped inside the ORP box
      int16_t bx1, by1; uint16_t bw, bh;
      orpCanvas->setFont(&FreeSansBold24pt7b);
      orpCanvas->getTextBounds(vb, 0, 34, &bx1, &by1, &bw, &bh);
      int mvx = ORP_BOX_X + (int)min((int)bw + 8, ORP_BOX_W - 16);
      int mvy = ORP_BOX_Y + 28;
      gfx->setFont(nullptr);
      gfx->setTextSize(1);
      gfx->setTextColor(WHITE);
      gfx->setCursor(mvx, mvy);
      gfx->print("mV");
    }
  }

  // Temp value box (canvas)
  int tempScaled = METRICS().haveTemp ? (int)lrintf(METRICS().tempC * 10.0f) : INT32_MIN;
  if (tempScaled != lastTempScaled) {
    lastTempScaled = tempScaled;
    if (tempCanvas) {
      tempCanvas->fillScreen(BLACK);
      tempCanvas->setFont(&FreeSans12pt7b);
      tempCanvas->setTextColor(WHITE);
      tempCanvas->setCursor(0, 16);
      if (METRICS().haveTemp) { char b[16]; snprintf(b,sizeof(b),"%.1f\xC2\xB0C", METRICS().tempC); tempCanvas->print(b); } else { tempCanvas->print("--.-\xC2\xB0C"); }
      gfx->draw16bitRGBBitmap(TEMP_BOX_X, TEMP_BOX_Y, tempCanvas->getBuffer(), TEMP_BOX_W, TEMP_BOX_H);
      tempCanvas->setFont(nullptr);
    }
  }

  // IP address box (canvas)
  String wantIp = (WiFi.status()==WL_CONNECTED) ? WiFi.localIP().toString() : String("--");
  if (wantIp != shownIp) {
    shownIp = wantIp;
    if (ipCanvas) {
      ipCanvas->fillScreen(BLACK);
      ipCanvas->setFont(nullptr);
      ipCanvas->setTextSize(1);
      ipCanvas->setTextColor(WHITE);
      ipCanvas->setCursor(0, 12);
      ipCanvas->print("IP: "); ipCanvas->print(shownIp);
      gfx->draw16bitRGBBitmap(IP_BOX_X, IP_BOX_Y, ipCanvas->getBuffer(), IP_BOX_W, IP_BOX_H);
    }
  }

  // Skip Arduino_GFX motor icon drawing when LVGL UI is active
  if (!USE_LVGL_UI && !UI_OVERLAY_ACTIVE) {
    if (lastM1Icon != m1Running) { lastM1Icon = m1Running; drawMotorIcon(gfx, M1_ICON_X, M1_ICON_Y, m1Running); }
    if (lastM2Icon != m2Running) { lastM2Icon = m2Running; drawMotorIcon(gfx, M2_ICON_X, M2_ICON_Y, m2Running); }
  }
}

// Touch moved to io/Touch

// ---- Simple on-device editor overlay ----
enum OverlayKind { OVL_NONE=0, OVL_PH, OVL_ORP };
static OverlayKind ovl = OVL_NONE;
static bool ovlDirty=false;

// Overlay layout
static const int OVL_X=12, OVL_Y=6, OVL_W=300, OVL_H=148;
static const int BTN_W=48, BTN_H=32;
static const int ROW1_Y = OVL_Y + 26;  // hoger zodat meer ruimte overblijft
static const int ROW2_Y = ROW1_Y + 48; // compacte verticale spacing
static const int VAL_W=110;
static const int COL_LEFT = OVL_X + 16;
static const int COL_VAL  = OVL_X + 70;   // tekstvak verder links
static const int COL_PLUS = OVL_X + 220;  // netjes naast elkaar
static const int COL_MINUS= OVL_X + 170;

static void drawButton(int x,int y,int w,int h,const char* label,uint16_t fg,uint16_t bg,int textSize=2){
  gfx->fillRoundRect(x,y,w,h,6,bg);
  gfx->drawRoundRect(x,y,w,h,6,fg);
  gfx->setFont(nullptr); gfx->setTextSize(textSize); gfx->setTextColor(fg);
  int tx = x + 8; int ty = y + (h/2) - (textSize*4); // approx vertical centering
  gfx->setCursor(tx,ty);
  gfx->print(label);
}

static void drawOverlay(){
  UI_OVERLAY_ACTIVE = true;
  // Modal box
  gfx->fillRoundRect(OVL_X, OVL_Y, OVL_W, OVL_H, 10, DARKGREY);
  gfx->drawRoundRect(OVL_X, OVL_Y, OVL_W, OVL_H, 10, WHITE);
  // (Optioneel) titel weglaten om ruimte te winnen
  // gfx->setFont(nullptr); gfx->setTextSize(2); gfx->setTextColor(WHITE);
  // gfx->setCursor(OVL_X+10, OVL_Y+18);
  // gfx->print(ovl==OVL_PH?"Edit pH":"Edit ORP");

  // Labels
  gfx->setTextSize(2);
  gfx->setCursor(COL_LEFT, ROW1_Y-10); gfx->print("Min");
  gfx->setCursor(COL_LEFT, ROW2_Y-10); gfx->print("Max");

  // Value boxes
  char b[16];
  if (ovl==OVL_PH) { snprintf(b,sizeof(b),"%.2f", PH_MIN); }
  else { snprintf(b,sizeof(b),"%d", ORP_MIN); }
  drawButton(COL_VAL, ROW1_Y-8, VAL_W, BTN_H, b, WHITE, BLACK, 2);
  if (ovl==OVL_PH) { snprintf(b,sizeof(b),"%.2f", PH_MAX); }
  else { snprintf(b,sizeof(b),"%d", ORP_MAX); }
  drawButton(COL_VAL, ROW2_Y-8, VAL_W, BTN_H, b, WHITE, BLACK, 2);

  // +/- buttons
  drawButton(COL_MINUS, ROW1_Y-8, BTN_W, BTN_H, "-", WHITE, DARKGREY, 3);
  drawButton(COL_PLUS,  ROW1_Y-8, BTN_W, BTN_H, "+", WHITE, DARKGREY, 3);
  drawButton(COL_MINUS, ROW2_Y-8, BTN_W, BTN_H, "-", WHITE, DARKGREY, 3);
  drawButton(COL_PLUS,  ROW2_Y-8, BTN_W, BTN_H, "+", WHITE, DARKGREY, 3);

  // Onderste strook achter Save/Cancel om overlap met velden te vermijden
  gfx->fillRect(OVL_X+8, OVL_Y+OVL_H-44, OVL_W-16, 40, DARKGREY);

  // Save/Cancel
  drawButton(OVL_X+34,  OVL_Y+OVL_H-38, 100, BTN_H, "Save", WHITE, GREEN, 2);
  drawButton(OVL_X+OVL_W-144, OVL_Y+OVL_H-38, 100, BTN_H, "Cancel", WHITE, RED, 2);
  ovlDirty=false;
}

static bool inRect(int x,int y,int rx,int ry,int rw,int rh){ return x>=rx && x<rx+rw && y>=ry && y<ry+rh; }

static void applyAndPersistThresholds(){
  // Publish retained and store to NVS
  // Publish retained config via MQTT client
  String _s1 = String(PH_MIN,2); mqttClient.ensureConnected();
  (void)_s1; // ensure not optimized out if client defers publish
  storage.setPhMin(PH_MIN);
  storage.setPhMax(PH_MAX);
  storage.setOrpMin(ORP_MIN);
  storage.setOrpMax(ORP_MAX);
}

static void drawSettingsPage() {
  // Draw settings frame
  gfx->drawRoundRect(2, 2, 316, 168, 10, CYAN);
  gfx->drawRoundRect(4, 4, 312, 164, 10, DARKGREEN);
  
  gfx->setFont(nullptr);
  gfx->setTextSize(2);
  gfx->setTextColor(WHITE);
  gfx->setCursor(10, 20);
  gfx->print("Motor Settings");
  
  // Motor speed controls
  gfx->setTextSize(1);
  gfx->setTextColor(WHITE);
  
  // pH Motor
  gfx->setCursor(20, 50);
  gfx->print("pH Motor Speed:");
  drawButton(150, 45, 40, 20, "-", WHITE, DARKGREY, 1);
  drawButton(195, 45, 50, 20, String(M1_SPEED_PC).c_str(), WHITE, BLACK, 1);
  drawButton(250, 45, 40, 20, "+", WHITE, DARKGREY, 1);
  gfx->setCursor(295, 50);
  gfx->print("%");
  
  // ORP Motor  
  gfx->setCursor(20, 80);
  gfx->print("ORP Motor Speed:");
  drawButton(150, 75, 40, 20, "-", WHITE, DARKGREY, 1);
  drawButton(195, 75, 50, 20, String(M2_SPEED_PC).c_str(), WHITE, BLACK, 1);
  drawButton(250, 75, 40, 20, "+", WHITE, DARKGREY, 1);
  gfx->setCursor(295, 80);
  gfx->print("%");
  
  // Save button
  drawButton(110, 120, 100, 30, "Save", WHITE, GREEN, 2);
  
  // Draw pagination
  drawPagination();
}

static void handleTouchUI(){
  io::TouchPoint tp; 
  bool hasTouch = io::readTouchOnce(tp);
  
  // Simple button-based page navigation (temporary solution)
  static bool pageButtonDrawn = false;
  if (!pageButtonDrawn && currentPage == 0) {
    // Draw "Settings" button at bottom
    drawButton(210, 145, 100, 25, "Settings", WHITE, DARKGREY, 1);
    pageButtonDrawn = true;
  } else if (!pageButtonDrawn && currentPage == 1) {
    // Draw "Back" button at bottom
    drawButton(10, 145, 60, 25, "Back", WHITE, DARKGREY, 1);
    pageButtonDrawn = true;
  }
  
  // Track last touch for simple tap detection
  static int16_t lastTouchX = 0, lastTouchY = 0;
  static uint32_t lastTouchMs = 0;
  static bool touching = false;
  
  if (hasTouch && tp.pressed) {
    lastTouchX = tp.x;
    lastTouchY = tp.y;
    lastTouchMs = millis();
    touching = true;
    return; // Wait for release
  }
  
  // Handle touch release (tap)
  if (!hasTouch && touching) {
    touching = false;
    
    // Page navigation buttons
    if (currentPage == 0 && inRect(lastTouchX, lastTouchY, 210, 145, 100, 25)) {
      // Go to settings
      currentPage = 1;
      pageButtonDrawn = false;
      gfx->fillScreen(BLACK);
      drawSettingsPage();
      return;
    } else if (currentPage == 1 && inRect(lastTouchX, lastTouchY, 10, 145, 60, 25)) {
      // Back to main
      currentPage = 0;
      pageButtonDrawn = false;
      gfx->fillScreen(BLACK);
      staticDrawn = false;
      drawStaticUI();
      updateValueAreas();
      return;
    }
    
    // Handle overlay interactions on release
    if (ovl != OVL_NONE) {
      float phStep = 0.05f; int orpStep = 10;
      if (inRect(lastTouchX,lastTouchY, COL_MINUS, ROW1_Y-8, BTN_W, BTN_H)) { 
        if (ovl==OVL_PH) { PH_MIN-=phStep; } else { ORP_MIN-=orpStep; } 
        ovlDirty=true; 
      }
      else if (inRect(lastTouchX,lastTouchY, COL_PLUS, ROW1_Y-8, BTN_W, BTN_H)) { 
        if (ovl==OVL_PH) { PH_MIN+=phStep; } else { ORP_MIN+=orpStep; } 
        ovlDirty=true; 
      }
      else if (inRect(lastTouchX,lastTouchY, COL_MINUS, ROW2_Y-8, BTN_W, BTN_H)) { 
        if (ovl==OVL_PH) { PH_MAX-=phStep; } else { ORP_MAX-=orpStep; } 
        ovlDirty=true; 
      }
      else if (inRect(lastTouchX,lastTouchY, COL_PLUS, ROW2_Y-8, BTN_W, BTN_H)) { 
        if (ovl==OVL_PH) { PH_MAX+=phStep; } else { ORP_MAX+=orpStep; } 
        ovlDirty=true; 
      }
      else if (inRect(lastTouchX,lastTouchY, OVL_X+34, OVL_Y+OVL_H-38, 100, BTN_H)) {
      applyAndPersistThresholds();
      ovl=OVL_NONE; UI_OVERLAY_ACTIVE=false;
      gfx->fillScreen(BLACK);
      staticDrawn=false; drawStaticUI();
      shownIp = String();
      lastM1Icon=!lastM1Icon; lastM2Icon=!lastM2Icon;
      updateValueAreas();
      return; 
    }
      else if (inRect(lastTouchX,lastTouchY, OVL_X+OVL_W-144, OVL_Y+OVL_H-38, 100, BTN_H)) {
      ovl=OVL_NONE; UI_OVERLAY_ACTIVE=false;
      gfx->fillScreen(BLACK);
      staticDrawn=false; drawStaticUI();
      shownIp = String();
      lastM1Icon=!lastM1Icon; lastM2Icon=!lastM2Icon;
      updateValueAreas();
      return; 
    }
    if (ovlDirty) {
      char b[16];
      if (ovl==OVL_PH) { snprintf(b,sizeof(b),"%.2f", PH_MIN); }
      else { snprintf(b,sizeof(b),"%d", ORP_MIN); }
      drawButton(COL_VAL, ROW1_Y-8, VAL_W, BTN_H, b, WHITE, BLACK, 2);
      if (ovl==OVL_PH) { snprintf(b,sizeof(b),"%.2f", PH_MAX); }
      else { snprintf(b,sizeof(b),"%d", ORP_MAX); }
      drawButton(COL_VAL, ROW2_Y-8, VAL_W, BTN_H, b, WHITE, BLACK, 2);
      ovlDirty=false;
    }
    return;
  }
  
  // Check pH/ORP box taps on main page
  if (currentPage == 0 && ovl == OVL_NONE) {
    if (inRect(lastTouchX, lastTouchY, PH_BOX_X, PH_BOX_Y, PH_BOX_W, PH_BOX_H)) {
      ovl = OVL_PH;
      showRangeDialog(true);
    } else if (inRect(lastTouchX, lastTouchY, ORP_BOX_X, ORP_BOX_Y, ORP_BOX_W, ORP_BOX_H)) {
      ovl = OVL_ORP;
      showRangeDialog(false);
    }
  }
  
  // Check settings page button taps
  if (currentPage == 1) {
    // pH motor -/+
    if (inRect(lastTouchX, lastTouchY, 150, 45, 40, 20)) {
      M1_SPEED_PC = max(0, M1_SPEED_PC - 5);
      drawSettingsPage();
    } else if (inRect(lastTouchX, lastTouchY, 250, 45, 40, 20)) {
      M1_SPEED_PC = min(100, M1_SPEED_PC + 5);
      drawSettingsPage();
    }
    // ORP motor -/+
    else if (inRect(lastTouchX, lastTouchY, 150, 75, 40, 20)) {
      M2_SPEED_PC = max(0, M2_SPEED_PC - 5);
      drawSettingsPage();
    } else if (inRect(lastTouchX, lastTouchY, 250, 75, 40, 20)) {
      M2_SPEED_PC = min(100, M2_SPEED_PC + 5);
      drawSettingsPage();
    }
    // Save button
    else if (inRect(lastTouchX, lastTouchY, 110, 120, 100, 30)) {
      storage.setM1Speed(M1_SPEED_PC);
      storage.setM2Speed(M2_SPEED_PC);
      pageButtonDrawn = false;
      currentPage = 0;
      gfx->fillScreen(BLACK);
      staticDrawn = false;
      drawStaticUI();
      updateValueAreas();
    }
  }
  } // End of touch release handling
}

static void connectWiFiIfNeeded() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("pool-sniffer-c6");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

static void ensureMqtt() {
  mqttClient.setStorage(&storage);
  mqttClient.setThresholdRefs(&PH_MIN, &PH_MAX, &ORP_MIN, &ORP_MAX);
  mqttClient.begin(MQTT_HOST, MQTT_PORT, MQTT_USER, MQTT_PASS, MQTT_CLIENTID);
}

static void publishDiscoveryOnce() { mqttClient.publishDiscoveryOnce(); }

static void publishStatesIfReady() { mqttClient.publishStatesIfReady(domain::Metrics::instance()); }

// ---- Dummy telemetry generator ----
static void updateDummyTelemetry() {
  static bool inited=false;
  static uint32_t last=0; uint32_t now=millis();
  if (!inited) {
    // Seed with hardware RNG if available
    randomSeed((uint32_t)esp_random());
    METRICS().phVal = (PH_MIN + PH_MAX) * 0.5f; METRICS().orpMv = (float)ORP_MIN + 50.0f; METRICS().tempC = 25.0f;
    METRICS().havePh = METRICS().haveOrp = METRICS().haveTemp = true; METRICS().preferPhPrimary = true;
    inited = true;
    last = 0; // force first tick
  }
  if (now - last < 1500) return; // langzamer en beter zichtbaar
  last = now;

  // --- pH state machine: rise → plateau_high → fall → plateau_low → rise ---
  static uint8_t phState = 0; // 0 rise, 1 plateau_high, 2 fall, 3 plateau_low
  static uint32_t phPlateauUntil = 0;
  const float phStep = 0.10f;        // grotere lineaire stap (0.1 pH)
  const float phHighTarget = PH_MAX + 0.15f; // iets boven MAX
  const float phLowTarget  = PH_MIN + 0.10f; // net boven MIN
  switch (phState) {
    case 0: // rise
      METRICS().phVal += phStep;
      if (METRICS().phVal >= phHighTarget) { METRICS().phVal = phHighTarget; phState = 1; phPlateauUntil = now + 6000; }
      break;
    case 1: // plateau high (motor zou lopen)
      if ((int32_t)(now - phPlateauUntil) >= 0) { phState = 2; }
      break;
    case 2: // fall
      METRICS().phVal -= phStep;
      if (METRICS().phVal <= phLowTarget) { METRICS().phVal = phLowTarget; phState = 3; phPlateauUntil = now + 4000; }
      break;
    default: // plateau low
      if ((int32_t)(now - phPlateauUntil) >= 0) { phState = 0; }
      break;
  }
  METRICS().phVal = constrain(METRICS().phVal, 3.0f, 14.0f);

  // --- ORP state machine: fall → plateau_low → rise → plateau_high → fall ---
  static uint8_t orpState = 0; // 0 fall, 1 plateau_low, 2 rise, 3 plateau_high
  static uint32_t orpPlateauUntil = 0;
  const float orpStep = 4.0f;            // mV per tick
  const float orpLowTarget  = (float)ORP_MIN - 40.0f;  // onder MIN
  const float orpHighTarget = (float)ORP_MIN + 140.0f; // ruim boven MIN
  switch (orpState) {
    case 0: // fall
      METRICS().orpMv -= orpStep;
      if (METRICS().orpMv <= orpLowTarget) { METRICS().orpMv = orpLowTarget; orpState = 1; orpPlateauUntil = now + 6000; }
      break;
    case 1: // plateau low (motor zou lopen)
      if ((int32_t)(now - orpPlateauUntil) >= 0) { orpState = 2; }
      break;
    case 2: // rise
      METRICS().orpMv += orpStep;
      if (METRICS().orpMv >= orpHighTarget) { METRICS().orpMv = orpHighTarget; orpState = 3; orpPlateauUntil = now + 4000; }
      break;
    default: // plateau high
      if ((int32_t)(now - orpPlateauUntil) >= 0) { orpState = 0; }
      break;
  }
  METRICS().orpMv = constrain(METRICS().orpMv, -2000.0f, 2000.0f);
  METRICS().tempC  += (float)random(-3, 4) / 10.0f;      // ±0.3 °C
  METRICS().tempC   = constrain(METRICS().tempC, 5.0f, 40.0f);

  updateValueAreas();
}

void pushLine(const String &s) {
  String t = s;
  if (t.length() > MAX_LINE_CHARS) t = t.substring(0, MAX_LINE_CHARS);
  if (lines.size() >= MAX_LINES) lines.erase(lines.begin());
  lines.push_back(t);
}

void drawScreen() {
  gfx->fillScreen(BLACK);
  gfx->setTextColor(WHITE);
  gfx->setTextWrap(false);

  if (SIMPLE_VIEW) {
    // Draw based on current page
    if (currentPage == 0) {
    drawStaticUI();
    updateValueAreas();
    } else if (currentPage == 1) {
      drawSettingsPage();
    }
  } else {
    // Full diagnostic header + lines
    gfx->setTextSize(2);
    gfx->setCursor(0, 0);
    gfx->println("Tuya Sniffer");
    gfx->setCursor(0, 20);
    gfx->print("A->WiFi  B->MCU  ");
    gfx->print(TUYA_BAUD);
    // Show live byte counters
    gfx->setCursor(0, 40);
    gfx->print("A:"); gfx->print(rxA_count);
    gfx->print("  B:"); gfx->print(rxB_count);
    // Key metrics
    gfx->setCursor(0, 60);
    gfx->print("Temp: "); if (METRICS().haveTemp) { char b[16]; snprintf(b,sizeof(b),"%.1f",METRICS().tempC); gfx->print(b); gfx->print(" \xC2\xB0C"); } else { gfx->print("--"); }
    gfx->setCursor(0, 80);
    gfx->print("pH: "); if (METRICS().havePh) { char b[16]; snprintf(b,sizeof(b),"%.2f",METRICS().phVal); gfx->print(b); } else { gfx->print("--"); }
    gfx->setCursor(0, 100);
    gfx->print("ORP: "); if (METRICS().haveOrp) { char b[16]; snprintf(b,sizeof(b),"%.1f",METRICS().orpMv); gfx->print(b); gfx->print(" mV"); } else { gfx->print("--"); }

    int y = 124; // below header + counters + metrics
    for (auto &s : lines) {
      gfx->setCursor(0, y);
      gfx->println(s);
      y += 20;
    }
  }
}

// (legacy parser verwijderd; io/Tuya wordt gebruikt)

// Parser moved to io/Tuya

void setup() {
  // USB serial
  Serial.begin(115200);
  // Wait a bit for the serial monitor to connect
  delay(2000); 

  Serial.setTimeout(50);
  unsigned long t0 = millis();
  while (!Serial && (millis() - t0) < 2000) { delay(10); }
  Serial.setDebugOutput(true);
  Serial.println("\nESP32-C6 Tuya Sniffer + Display");
  Serial.print("Firmware Version: ");
  Serial.println(FIRMWARE_VERSION);

  // Ensure HW SPI uses pins from working example (SCK=1, MOSI=2, CS=14)
  SPI.begin(1 /* SCK */, -1 /* MISO */, 2 /* MOSI */, 14 /* SS */);

  // Backlight on (default)
  if (LCD_BL_PIN >= 0) { pinMode(LCD_BL_PIN, OUTPUT); digitalWrite(LCD_BL_PIN, HIGH); }
  
  // Remove broad BL scan to avoid toggling reserved pins

  // Hardware reset pulse on LCD reset pin (GPIO22)
  pinMode(22, OUTPUT);
  digitalWrite(22, LOW);
  delay(20);
  digitalWrite(22, HIGH);
  delay(120);

  // LCD init
  if (!gfx->begin()) {
    Serial.println("LCD begin() failed");
  } else {
    // Apply vendor init sequence and set rotation (landscape)
    lcd_reg_init();
    gfx->setRotation(1);
  }
  // Clear to black once; avoid further direct GFX drawing when LVGL is used
  gfx->fillScreen(BLACK);
  delay(20);

  // Begin touch - MUST be after display init but BEFORE LVGL (matching working code)
  // io::touchBegin(); // MOVED to after LVGL init

  if (USE_LVGL_UI) {
    // Initialize LVGL via display bridge
    displayBridge = new core::DisplayBridge(gfx);
    displayBridge->initLvgl(20);
    lv_disp_t *disp = displayBridge->registerDisplay();
    ui::init(disp);
    // Connect UI slider handlers to storage-backed speeds
    ui::Handlers h; h.onSpeedChange = [](int idx, int value){
      value = constrain(value, 0, 100);
      if (idx==1) { M1_SPEED_PC = (uint8_t)value; storage.setM1Speed(M1_SPEED_PC); }
      else if (idx==2) { M2_SPEED_PC = (uint8_t)value; storage.setM2Speed(M2_SPEED_PC); }
    };
    ui::configureHandlers(h);
    ui::setInitialSpeeds(M1_SPEED_PC, M2_SPEED_PC);

    // Apply a dark theme so text contrasts on black backgrounds
    lv_theme_t *th = lv_theme_default_init(
      disp,
      lv_palette_main(LV_PALETTE_BLUE),
      lv_palette_main(LV_PALETTE_RED),
      true, // dark mode
      &lv_font_montserrat_14
    );
    lv_disp_set_theme(disp, th);

    // Input device (touch) bridge (enabled with safe polling read_cb)
    if (true) {
      static lv_indev_drv_t indev_drv;
      lv_indev_drv_init(&indev_drv);
      indev_drv.type = LV_INDEV_TYPE_POINTER;
      indev_drv.read_cb = [](lv_indev_drv_t *d, lv_indev_data_t *data)->void{
        uint8_t buf[14] = {0};
        if (!io::i2cRead(io::AXS5106L_ADDR, io::AXS5106L_TOUCH_DATA_REG, buf, sizeof(buf))) {
          static bool fail_reported = false;
          if (!fail_reported) {
            Serial.println("[Touch] LVGL read_cb: I2C read failed!");
            fail_reported = true;
          }
          data->state = LV_INDEV_STATE_RELEASED; 
          return;
        }

        uint8_t n = buf[1]; // Number of touch points
        if (n == 0) { 
          data->state = LV_INDEV_STATE_RELEASED; 
          return; 
        }

        // At least one point detected, let's process and report it
        uint16_t rx = (((uint16_t)(buf[2] & 0x0F)) << 8) | buf[3];
        uint16_t ry = (((uint16_t)(buf[4] & 0x0F)) << 8) | buf[5];
        int16_t x = (int16_t)ry; // Swapped for landscape
        int16_t y = (int16_t)rx;
        
        // Use the gfx object which is captured by the lambda
        extern Arduino_GFX *gfx; 
        x = constrain(x, 0, (int)gfx->width()-1);
        y = constrain(y, 0, (int)gfx->height()-1);
        
        data->point.x = x; 
        data->point.y = y; 
        data->state = LV_INDEV_STATE_PRESSED;
        
        static uint32_t last_print = 0;
        if (millis() - last_print > 100) { // Rate limit printing
            last_print = millis();
            Serial.printf("[Touch] LVGL: PRESSED at x=%d, y=%d, points=%d\n", x, y, n);
        }
      };
      (void)lv_indev_drv_register(&indev_drv);
    }

    // Build LVGL UI
    auto build_lvgl_ui = [=](){
      lv_obj_t *scr = lv_scr_act();
      // overall background: light grey
      lv_obj_set_style_bg_color(scr, lv_palette_lighten(LV_PALETTE_GREY, 3), 0);
      lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
      lv_obj_set_style_text_color(scr, lv_color_black(), 0);
      // Ensure no scrollbars on the root screen
      lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
      lv_obj_set_style_bg_opa(scr, LV_OPA_TRANSP, LV_PART_SCROLLBAR);
      if (LVGL_SAFE_BASELINE) {
        // Minimal stable UI: show values as simple labels
        lv_obj_t *lbl1 = lv_label_create(scr);
        lv_label_set_text(lbl1, "pH --.--");
        lv_obj_set_style_text_font(lbl1, &lv_font_montserrat_28, 0);
        lv_obj_align(lbl1, LV_ALIGN_LEFT_MID, 10, -10);

        lv_obj_t *lbl2 = lv_label_create(scr);
        lv_label_set_text(lbl2, "ORP ---- mV");
        lv_obj_set_style_text_font(lbl2, &lv_font_montserrat_28, 0);
        lv_obj_align(lbl2, LV_ALIGN_RIGHT_MID, -10, -10);

        lv_lbl_ph = lbl1; lv_lbl_orp = lbl2; lv_lbl_orp_unit = NULL;
        lv_lbl_ip = lv_label_create(scr);
        lv_obj_set_style_text_color(lv_lbl_ip, lv_palette_darken(LV_PALETTE_GREY, 4), 0);
        lv_label_set_text(lv_lbl_ip, "IP: --");
        lv_obj_align(lv_lbl_ip, LV_ALIGN_BOTTOM_RIGHT, -2, -1);
        ui::updateValues();
        return;
      }
      // overall background: light grey
      lv_obj_set_style_bg_color(scr, lv_palette_lighten(LV_PALETTE_GREY, 3), 0);
      lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
      lv_obj_set_style_text_color(scr, lv_color_white(), 0);
      // Eliminate any implicit padding on screen
      lv_obj_set_style_pad_all(scr, 0, 0);

      // Continue into legacy tileview UI for now (ensures swipe + main tiles)

      // (moved lower) create debug label after frame to ensure foreground

      // Fullscreen root container
      lv_obj_t *frame = lv_obj_create(scr);
      lv_obj_set_size(frame, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
      lv_obj_set_pos(frame, 0, 0);
      lv_obj_set_style_border_width(frame, 0, 0);
      lv_obj_set_style_radius(frame, 0, 0);
      lv_obj_set_style_bg_opa(frame, LV_OPA_TRANSP, 0);
      lv_obj_set_style_pad_all(frame, 0, 0);
      lv_obj_set_scrollbar_mode(frame, LV_SCROLLBAR_MODE_OFF);
      lv_obj_set_style_bg_opa(frame, LV_OPA_TRANSP, LV_PART_SCROLLBAR);

      // Tileview with 2 pages (main, settings)
      lv_tv = lv_tileview_create(frame);
      lv_obj_set_size(lv_tv, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
      lv_obj_set_pos(lv_tv, 0, 0);
      lv_obj_set_scroll_dir(lv_tv, LV_DIR_HOR);
      lv_obj_set_scroll_snap_x(lv_tv, LV_SCROLL_SNAP_CENTER);
      lv_obj_set_style_bg_opa(lv_tv, LV_OPA_TRANSP, 0);
      lv_obj_set_style_pad_all(lv_tv, 0, 0);
      // Keep scrolling enabled for swipe; just hide scrollbars
      lv_obj_set_scrollbar_mode(lv_tv, LV_SCROLLBAR_MODE_OFF);
      lv_obj_set_style_bg_opa(lv_tv, LV_OPA_TRANSP, LV_PART_SCROLLBAR);
      lv_obj_set_style_text_color(lv_tv, lv_color_white(), 0);

      lv_tile_main = lv_tileview_add_tile(lv_tv, 0, 0, LV_DIR_HOR);
      lv_tile_settings = lv_tileview_add_tile(lv_tv, 1, 0, LV_DIR_HOR);
      lv_obj_set_style_bg_opa(lv_tile_main, LV_OPA_TRANSP, 0);
      lv_obj_set_style_pad_all(lv_tile_main, 0, 0);
      lv_obj_clear_flag(lv_tile_main, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_scrollbar_mode(lv_tile_main, LV_SCROLLBAR_MODE_OFF);
      lv_obj_set_style_text_color(lv_tile_main, lv_color_white(), 0);
      lv_obj_set_style_bg_opa(lv_tile_settings, LV_OPA_TRANSP, 0);
      lv_obj_set_style_pad_all(lv_tile_settings, 0, 0);
      lv_obj_clear_flag(lv_tile_settings, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_scrollbar_mode(lv_tile_settings, LV_SCROLLBAR_MODE_OFF);
      lv_obj_set_style_text_color(lv_tile_settings, lv_color_white(), 0);

      // Main tile content: transparent background, white text
      lv_obj_set_style_bg_opa(lv_tile_main, LV_OPA_TRANSP, 0);
      lv_obj_set_style_text_color(lv_tile_main, lv_color_white(), 0);

      // Screen resolution (avoid zero sizes before layout)
      int scr_w = lv_disp_get_hor_res(NULL);
      int scr_h = lv_disp_get_ver_res(NULL);

      // No header bar; compute zero header height
      int hdr_h = 0;

      // Tiles direct on grey background (no white panel)
      int gap = 10; int pad = 10;
      int content_w = scr_w - (pad*2);
      int content_h = scr_h - (pad*2) - 10; // leave room for IP label
      lv_obj_t *content = lv_obj_create(lv_tile_main); lv_obj_remove_style_all(content);
      lv_obj_set_size(content, content_w, content_h);
      lv_obj_set_style_pad_all(content, pad, 0);
      // Center content; a slight upward shift keeps room for IP label
      lv_obj_align(content, LV_ALIGN_CENTER, 0, -5);
      lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_OFF);
      lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, LV_PART_SCROLLBAR);
      static lv_style_t st_card; static bool st_inited=false; if(!st_inited){ st_inited=true; lv_style_init(&st_card); lv_style_set_radius(&st_card, 12); lv_style_set_bg_opa(&st_card, LV_OPA_COVER); lv_style_set_bg_grad_dir(&st_card, LV_GRAD_DIR_VER); lv_style_set_shadow_width(&st_card, 10); lv_style_set_shadow_opa(&st_card, LV_OPA_30); lv_style_set_shadow_ofs_y(&st_card, 4); lv_style_set_pad_all(&st_card, 10); }
      auto make_tile = [&](bool left, lv_color_t c1, lv_color_t c2, const char *title){ lv_obj_t *tile = lv_btn_create(content); lv_obj_remove_style_all(tile); lv_obj_add_style(tile, &st_card, 0); lv_obj_set_style_bg_color(tile, c1, 0); lv_obj_set_style_bg_grad_color(tile, c2, 0); int tw = (content_w - gap)/2; int th = content_h - 4; if (th < 60) th = 60; lv_obj_set_size(tile, tw, th); lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE); lv_obj_set_scrollbar_mode(tile, LV_SCROLLBAR_MODE_OFF); /* no gesture bubble here; let tileview consume gestures */ if(left) lv_obj_align(tile, LV_ALIGN_LEFT_MID, -4, 0); else lv_obj_align(tile, LV_ALIGN_RIGHT_MID, 4, 0); 
        return tile; };
      lv_obj_t *ph_tile   = make_tile(true,  lv_palette_main(LV_PALETTE_INDIGO), lv_palette_darken(LV_PALETTE_INDIGO, 2), "pH");
      lv_obj_t *orp_tile  = make_tile(false, lv_palette_main(LV_PALETTE_GREEN),  lv_palette_darken(LV_PALETTE_GREEN, 2),  "ORP");

      // value labels inside tiles (centered)
      if (ph_tile) {
        // Create a subtle shadow behind main label (1px offset, semi-transparent)
        lv_lbl_ph_shadow = lv_label_create(ph_tile);
        lv_obj_set_style_text_font(lv_lbl_ph_shadow, &lv_font_source_code_pro_36_bold, 0);
        lv_obj_set_style_text_color(lv_lbl_ph_shadow, lv_color_black(), 0);
        lv_obj_set_style_text_opa(lv_lbl_ph_shadow, (lv_opa_t)89, 0); // ~35%
        lv_label_set_text(lv_lbl_ph_shadow, "--.--");
        lv_obj_align(lv_lbl_ph_shadow, LV_ALIGN_CENTER, 1, 1);
        // Main label on top
        lv_lbl_ph = lv_label_create(ph_tile);
        lv_obj_set_style_text_color(lv_lbl_ph, lv_color_white(), 0);
        lv_obj_set_style_text_font(lv_lbl_ph, &lv_font_source_code_pro_36_bold, 0);
        lv_obj_align(lv_lbl_ph, LV_ALIGN_CENTER, 0, 0);
        lv_label_set_text(lv_lbl_ph, "--.--");
        // pH icon top-left: manual silhouette shadow (duplicate image behind)
        lv_img_ph_icon_shadow = lv_img_create(ph_tile);
        lv_img_set_src(lv_img_ph_icon_shadow, &water_ph_32dp_E3E3E3_FILL0_wght400_GRAD0_opsz40);
        lv_obj_set_style_img_recolor_opa(lv_img_ph_icon_shadow, LV_OPA_COVER, 0);
        lv_obj_set_style_img_recolor(lv_img_ph_icon_shadow, lv_color_black(), 0);
        lv_obj_set_style_img_opa(lv_img_ph_icon_shadow, (lv_opa_t)89, 0);
        lv_obj_align(lv_img_ph_icon_shadow, LV_ALIGN_TOP_LEFT, 1, 1);
        lv_img_ph_icon = lv_img_create(ph_tile);
        lv_img_set_src(lv_img_ph_icon, &water_ph_32dp_E3E3E3_FILL0_wght400_GRAD0_opsz40);
        lv_obj_set_style_img_recolor_opa(lv_img_ph_icon, LV_OPA_COVER, 0);
        lv_obj_set_style_img_recolor(lv_img_ph_icon, lv_color_white(), 0);
        lv_obj_align(lv_img_ph_icon, LV_ALIGN_TOP_LEFT, 0, 0);
      }
      if (orp_tile){
        lv_lbl_orp_shadow = lv_label_create(orp_tile);
        lv_obj_set_style_text_font(lv_lbl_orp_shadow, &lv_font_source_code_pro_36_bold, 0);
        lv_obj_set_style_text_color(lv_lbl_orp_shadow, lv_color_black(), 0);
        lv_obj_set_style_text_opa(lv_lbl_orp_shadow, (lv_opa_t)89, 0); // ~35%
        lv_label_set_text(lv_lbl_orp_shadow, "----");
        lv_obj_align(lv_lbl_orp_shadow, LV_ALIGN_CENTER, -9, 1);
        lv_lbl_orp = lv_label_create(orp_tile);
        lv_obj_set_style_text_color(lv_lbl_orp, lv_color_white(), 0);
        lv_obj_set_style_text_font(lv_lbl_orp, &lv_font_source_code_pro_36_bold, 0);
        lv_obj_align(lv_lbl_orp, LV_ALIGN_CENTER, -10, 0);
        lv_label_set_text(lv_lbl_orp, "----");
        // small unit label "mV"
        lv_lbl_orp_unit = lv_label_create(orp_tile);
        lv_obj_set_style_text_color(lv_lbl_orp_unit, lv_color_white(), 0);
        lv_obj_set_style_text_font(lv_lbl_orp_unit, &lv_font_montserrat_14, 0);
        lv_label_set_text(lv_lbl_orp_unit, " mV");
        lv_obj_align_to(lv_lbl_orp_unit, lv_lbl_orp, LV_ALIGN_OUT_RIGHT_MID, 4, 2);
        // ORP icon top-left: manual silhouette shadow
        lv_img_orp_icon_shadow = lv_img_create(orp_tile);
        lv_img_set_src(lv_img_orp_icon_shadow, &water_orp_32dp_E3E3E3_FILL0_wght400_GRAD0_opsz40);
        lv_obj_set_style_img_recolor_opa(lv_img_orp_icon_shadow, LV_OPA_COVER, 0);
        lv_obj_set_style_img_recolor(lv_img_orp_icon_shadow, lv_color_black(), 0);
        lv_obj_set_style_img_opa(lv_img_orp_icon_shadow, (lv_opa_t)89, 0);
        lv_obj_align(lv_img_orp_icon_shadow, LV_ALIGN_TOP_LEFT, 1, 1);
        lv_img_orp_icon = lv_img_create(orp_tile);
        lv_img_set_src(lv_img_orp_icon, &water_orp_32dp_E3E3E3_FILL0_wght400_GRAD0_opsz40);
        lv_obj_set_style_img_recolor_opa(lv_img_orp_icon, LV_OPA_COVER, 0);
        lv_obj_set_style_img_recolor(lv_img_orp_icon, lv_color_white(), 0);
        lv_obj_align(lv_img_orp_icon, LV_ALIGN_TOP_LEFT, 0, 0);
      }
      // attach tap -> range dialog
      // Enable short-tap to open edit dialog (avoid conflict with swipe)
      if (ph_tile)  { TileTapCtx *c=(TileTapCtx*)lv_mem_alloc(sizeof(TileTapCtx)); memset(c,0,sizeof(TileTapCtx)); c->isPh=true; lv_obj_add_event_cb(ph_tile, tile_tap_cb, LV_EVENT_ALL, c); }
      if (orp_tile) { TileTapCtx *c=(TileTapCtx*)lv_mem_alloc(sizeof(TileTapCtx)); memset(c,0,sizeof(TileTapCtx)); c->isPh=false; lv_obj_add_event_cb(orp_tile, tile_tap_cb, LV_EVENT_ALL, c); }

      // Footer IP at bottom-right (always show)
      lv_lbl_ip = lv_label_create(lv_tile_main); lv_obj_set_style_text_color(lv_lbl_ip, lv_palette_darken(LV_PALETTE_GREY, 4), 0); lv_obj_set_style_text_font(lv_lbl_ip, &lv_font_montserrat_14, 0); lv_label_set_long_mode(lv_lbl_ip, LV_LABEL_LONG_CLIP); lv_obj_set_width(lv_lbl_ip, LV_SIZE_CONTENT); lv_obj_set_style_text_align(lv_lbl_ip, LV_TEXT_ALIGN_RIGHT, 0); lv_obj_align(lv_lbl_ip, LV_ALIGN_BOTTOM_RIGHT, -14, -1); lv_label_set_text(lv_lbl_ip, "IP: --");

      // Pump icon for pH (M1) at bottom-left of pH tile with silhouette shadow
      lv_img_pump_ph_shadow = lv_img_create(ph_tile);
      lv_img_set_src(lv_img_pump_ph_shadow, &water_pump_24dp_E3E3E3_FILL0_wght400_GRAD0_opsz24);
      lv_obj_set_style_img_recolor_opa(lv_img_pump_ph_shadow, LV_OPA_COVER, 0);
      lv_obj_set_style_img_recolor(lv_img_pump_ph_shadow, lv_color_black(), 0);
      lv_obj_set_style_img_opa(lv_img_pump_ph_shadow, (lv_opa_t)89, 0);
      lv_obj_align(lv_img_pump_ph_shadow, LV_ALIGN_BOTTOM_LEFT, 1, 1);
      lv_img_pump_ph = lv_img_create(ph_tile);
      lv_img_set_src(lv_img_pump_ph, &water_pump_24dp_E3E3E3_FILL0_wght400_GRAD0_opsz24);
      lv_obj_set_style_img_recolor_opa(lv_img_pump_ph, LV_OPA_COVER, 0);
      lv_obj_set_style_img_recolor(lv_img_pump_ph, lv_color_white(), 0);
      lv_obj_align(lv_img_pump_ph, LV_ALIGN_BOTTOM_LEFT, 0, 0);
      lv_obj_add_flag(lv_img_pump_ph, LV_OBJ_FLAG_HIDDEN);

      // Pump icon for ORP (M2) at bottom-left of ORP tile with silhouette shadow
      lv_img_pump_orp_shadow = lv_img_create(orp_tile);
      lv_img_set_src(lv_img_pump_orp_shadow, &water_pump_24dp_E3E3E3_FILL0_wght400_GRAD0_opsz24);
      lv_obj_set_style_img_recolor_opa(lv_img_pump_orp_shadow, LV_OPA_COVER, 0);
      lv_obj_set_style_img_recolor(lv_img_pump_orp_shadow, lv_color_black(), 0);
      lv_obj_set_style_img_opa(lv_img_pump_orp_shadow, (lv_opa_t)89, 0);
      lv_obj_align(lv_img_pump_orp_shadow, LV_ALIGN_BOTTOM_LEFT, 1, 1);
      lv_img_pump_orp = lv_img_create(orp_tile);
      lv_img_set_src(lv_img_pump_orp, &water_pump_24dp_E3E3E3_FILL0_wght400_GRAD0_opsz24);
      lv_obj_set_style_img_recolor_opa(lv_img_pump_orp, LV_OPA_COVER, 0);
      lv_obj_set_style_img_recolor(lv_img_pump_orp, lv_color_white(), 0);
      lv_obj_align(lv_img_pump_orp, LV_ALIGN_BOTTOM_LEFT, 0, 0);
      lv_obj_add_flag(lv_img_pump_orp, LV_OBJ_FLAG_HIDDEN);

      ui::updateValues();

      // Settings tile content: motor speed controls (clean styling)
      lv_obj_t *row1 = lv_obj_create(lv_tile_settings);
      lv_obj_remove_style_all(row1);
      lv_obj_set_size(row1, lv_obj_get_width(lv_tile_settings)-20, 40);
      lv_obj_align(row1, LV_ALIGN_TOP_MID, 0, 16);
      lv_obj_set_style_bg_color(row1, lv_palette_darken(LV_PALETTE_GREY,2), 0);
      lv_obj_set_style_bg_opa(row1, LV_OPA_30, 0);
      lv_obj_set_style_pad_all(row1, 6, 0);
      lv_obj_clear_flag(row1, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_scrollbar_mode(row1, LV_SCROLLBAR_MODE_OFF);

      lv_obj_t *lbl1 = lv_label_create(row1); lv_label_set_text(lbl1, "pH Motor Speed"); lv_obj_align(lbl1, LV_ALIGN_LEFT_MID, 0, 0);
      lv_obj_t *btn1m = lv_btn_create(row1); lv_obj_set_size(btn1m, 36, 32); lv_obj_align(btn1m, LV_ALIGN_RIGHT_MID, -120, 0); lv_obj_add_event_cb(btn1m, on_ph_minus_cb, LV_EVENT_CLICKED, NULL); lv_label_set_text(lv_label_create(btn1m), "-");
      lv_lbl_speed1 = lv_label_create(row1); lv_label_set_text(lv_lbl_speed1, "--%"); lv_obj_align(lv_lbl_speed1, LV_ALIGN_RIGHT_MID, -70, 0);
      lv_obj_t *btn1p = lv_btn_create(row1); lv_obj_set_size(btn1p, 36, 32); lv_obj_align(btn1p, LV_ALIGN_RIGHT_MID, -20, 0); lv_obj_add_event_cb(btn1p, on_ph_plus_cb, LV_EVENT_CLICKED, NULL); lv_label_set_text(lv_label_create(btn1p), "+");

      lv_obj_t *row2 = lv_obj_create(lv_tile_settings);
      lv_obj_remove_style_all(row2);
      lv_obj_set_size(row2, lv_obj_get_width(lv_tile_settings)-20, 40);
      lv_obj_align(row2, LV_ALIGN_TOP_MID, 0, 70);
      lv_obj_set_style_bg_color(row2, lv_palette_darken(LV_PALETTE_GREY,2), 0);
      lv_obj_set_style_bg_opa(row2, LV_OPA_30, 0);
      lv_obj_set_style_pad_all(row2, 6, 0);
      lv_obj_clear_flag(row2, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_scrollbar_mode(row2, LV_SCROLLBAR_MODE_OFF);
      lv_obj_t *lbl2 = lv_label_create(row2); lv_label_set_text(lbl2, "ORP Motor Speed"); lv_obj_align(lbl2, LV_ALIGN_LEFT_MID, 0, 0);
      lv_obj_t *btn2m = lv_btn_create(row2); lv_obj_set_size(btn2m, 36, 32); lv_obj_align(btn2m, LV_ALIGN_RIGHT_MID, -120, 0); lv_obj_add_event_cb(btn2m, on_orp_minus_cb, LV_EVENT_CLICKED, NULL); lv_label_set_text(lv_label_create(btn2m), "-");
      lv_lbl_speed2 = lv_label_create(row2); lv_label_set_text(lv_lbl_speed2, "--%"); lv_obj_align(lv_lbl_speed2, LV_ALIGN_RIGHT_MID, -70, 0);
      lv_obj_t *btn2p = lv_btn_create(row2); lv_obj_set_size(btn2p, 36, 32); lv_obj_align(btn2p, LV_ALIGN_RIGHT_MID, -20, 0); lv_obj_add_event_cb(btn2p, on_orp_plus_cb, LV_EVENT_CLICKED, NULL); lv_label_set_text(lv_label_create(btn2p), "+");

      // Save button
      lv_obj_t *btnSave = lv_btn_create(lv_tile_settings); lv_obj_set_size(btnSave, 100, 34); lv_obj_align(btnSave, LV_ALIGN_BOTTOM_MID, 0, -16); lv_obj_add_event_cb(btnSave, on_speed_save_cb, LV_EVENT_CLICKED, NULL); lv_label_set_text(lv_label_create(btnSave), "Save");
      lv_update_speed_labels();

      // Pagination dots removed to simplify and avoid event-related issues
      // Ensure initial layout is computed and the first tile is active/centered
      lv_obj_update_layout(scr);
      lv_obj_update_layout(lv_tv);
      lv_obj_update_layout(lv_tile_main);
      // Re-apply content align after sizes are final
      lv_obj_align(content, LV_ALIGN_TOP_MID, 0, 6);
      // Activate first tile without animation (prevents initial offset)
      lv_obj_set_tile(lv_tv, lv_tile_main, LV_ANIM_OFF);
      // Force scroll offsets to (0,0) to avoid any residual drift
      lv_obj_scroll_to_x(lv_tv, 0, LV_ANIM_OFF);
      lv_obj_scroll_to_y(lv_tv, 0, LV_ANIM_OFF);
    };
    build_lvgl_ui();
    // Add a one-shot timer to enforce centered layout once sizes settle
    lv_timer_t *once = lv_timer_create(lv_fix_initial_layout, 60, NULL);
    lv_timer_set_repeat_count(once, 1);
  }

  // Begin touch AFTER LVGL init, which matches the original working code structure.
  io::touchBegin();

  if (!DIAG_MODE) {
    // UARTs (RX only) unless TX pins are provided
    TUYA_A.begin(TUYA_BAUD, SERIAL_8N1, RX_A_PIN, TX_A_PIN);
    if (USE_CHANNEL_B) TUYA_B.begin(TUYA_BAUD, SERIAL_8N1, RX_B_PIN, TX_B_PIN);
  }

  // Configure Tuya DP ids for new parser module
  io::tuyaConfigure(DP_TEMP, DP_ORP, DP_PH, DP_ORP_ALT1, DP_PH_ALT1);

  pushLine("Ready. Waiting for frames...");
  if (!USE_LVGL_UI) {
  drawStaticUI();
  updateValueAreas();
  }

  // WiFi + MQTT
  setupWiFiEvents();
  connectWiFiIfNeeded();

  // Load persisted thresholds
  storage.begin(false);
  PH_MIN = storage.getPhMin(PH_MIN);
  PH_MAX = storage.getPhMax(PH_MAX);
  ORP_MIN = storage.getOrpMin(ORP_MIN);
  ORP_MAX = storage.getOrpMax(ORP_MAX);


  // Load custom speeds if present
  M1_SPEED_PC = (uint8_t)storage.getM1Speed(M1_SPEED_PC);
  M2_SPEED_PC = (uint8_t)storage.getM2Speed(M2_SPEED_PC);

  // TB6612 pins
  if (MOTOR_ENABLE) {
    control = new domain::ControlPolicy(TB_STBY, M1_IN1, M1_IN2, M1_PWM, M2_IN1, M2_IN2, M2_PWM);

    // Use the correct ESP32-C6 ledc API
    ledcAttach(M1_PWM, PWM_FREQ, PWM_BITS);
    ledcAttach(M2_PWM, PWM_FREQ, PWM_BITS);
    ledcWrite(M1_PWM, 0);
    ledcWrite(M2_PWM, 0);

    if (MOTOR_TEST && !FORCE_MOTOR_A_ON) {
      uint8_t duty = (uint8_t)(M1_SPEED_PC * 255 / 100);
      // M1 forward
      digitalWrite(M1_IN1, HIGH); digitalWrite(M1_IN2, LOW);
      ledcWrite(M1_PWM, duty); delay(1000); ledcWrite(M1_PWM, 0); delay(300);
      // M1 reverse
      digitalWrite(M1_IN1, LOW); digitalWrite(M1_IN2, HIGH);
      ledcWrite(M1_PWM, duty); delay(1000); ledcWrite(M1_PWM, 0); delay(500);
      // M2 forward
      digitalWrite(M2_IN1, HIGH); digitalWrite(M2_IN2, LOW);
      ledcWrite(M2_PWM, duty); delay(1000); ledcWrite(M2_PWM, 0); delay(300);
      // M2 reverse
      digitalWrite(M2_IN1, LOW); digitalWrite(M2_IN2, HIGH);
      ledcWrite(M2_PWM, duty); delay(1000); ledcWrite(M2_PWM, 0);
    }

    // Hard force Motor A on continuously for test (AIN1=LOW, AIN2=HIGH, 100% duty)
    if (FORCE_MOTOR_A_ON) {
      digitalWrite(M1_IN1, LOW);
      digitalWrite(M1_IN2, HIGH);
      ledcWrite(M1_PWM, 255);
      // Prevent the control loop from turning it off
      m1StopAt = UINT32_MAX;
      m1Running = true;
    }
  }

  // Optionally send Tuya queries after boot (requires TX pin wired!)
  if (SEND_ON_BOOT) {
    delay(300);
    pushLine("TX: query product info"); drawScreen();
    tuyaSendQueryProductInfo(TUYA_A);
    delay(200);
    pushLine("TX: set wifi status 0x00"); drawScreen();
    tuyaSendSetWifiStatus(TUYA_A, 0x00);
    delay(200);
    pushLine("TX: DP query"); drawScreen();
    tuyaSendDpQuery(TUYA_A);
  }
}

void loop() {
  if (USE_LVGL_UI) {
    lv_timer_handler();
    // (removed periodic debug text update)
    delay(1);  // more responsive UI
  } else if (DIAG_MODE) {
    static uint32_t last = 0;
    uint32_t now = millis();
    if (now - last > 1000) {
      last = now;
      Serial.print("[DIAG] millis="); Serial.println(now);
      // visual heartbeat on screen border
      static bool toggle = false; toggle = !toggle;
      uint16_t c = toggle ? YELLOW : CYAN;
      gfx->drawRect(0, 0, 171, 319, c);
    }
    return;
  }

  {
    int processed = 0;
    while (TUYA_A.available()) {
    uint8_t b = TUYA_A.read();
    rxA_count++;
    lastA[idxA] = b; idxA = (uint8_t)((idxA + 1) % 7);
    io::tuyaFeedA(b);
    // Reduce debug/printing when UI is active to avoid lag
    if (!USE_LVGL_UI && DIAG_MODE) {
      // ASCII line capture for quick human-readable sniffing
      if (b == '\n' || b == '\r') {
        if (asciiA.length() > 0) { pushLine(String("A> ") + asciiA); asciiA = ""; }
      } else {
        if (b >= 0x20 && b <= 0x7E) asciiA += (char)b; else asciiA += '.';
        if (asciiA.length() >= MAX_LINE_CHARS) { pushLine(String("A> ") + asciiA); asciiA = ""; }
      }
      // RAW hex dump to USB Serial (16 bytes per line)
      if ((rawCountA % 16) == 0) { Serial.print("\nA "); }
      hexByte(Serial, b); Serial.print(' ');
      rawCountA++;
    }
    if (USE_LVGL_UI && ++processed > 256) break; // yield to UI
  }
  }
  if (USE_CHANNEL_B && !USE_LVGL_UI) {
    while (TUYA_B.available()) {
      uint8_t b = TUYA_B.read();
      rxB_count++;
      lastB[idxB] = b; idxB = (uint8_t)((idxB + 1) % 7);
      io::tuyaFeedB(b);
      if (b == '\n' || b == '\r') {
        if (asciiB.length() > 0) { pushLine(String("B> ") + asciiB); asciiB = ""; }
      } else {
        if (b >= 0x20 && b <= 0x7E) asciiB += (char)b; else asciiB += '.';
        if (asciiB.length() >= MAX_LINE_CHARS) { pushLine(String("B> ") + asciiB); asciiB = ""; }
      }
      // RAW hex dump to USB Serial (16 bytes per line)
      if ((rawCountB % 16) == 0) { Serial.print("\nB "); }
      hexByte(Serial, b); Serial.print(' ');
      rawCountB++;
    }
  }

  // Avoid drawing directly with Arduino_GFX while LVGL UI is active

  // Dummy telemetry (optional)
  if (DUMMY_MODE) {
    updateDummyTelemetry();
  }

  // WiFi/MQTT service loop
  static uint32_t lastConnectAttempt = 0;
  uint32_t now = millis();
  
  if (WiFi.status() != WL_CONNECTED && now - lastConnectAttempt > 5000) { 
    lastConnectAttempt = now; 
    connectWiFiIfNeeded(); 
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    // Try to connect to MQTT, but don't block
    if (!mqttClient.isConnected() && now - lastConnectAttempt > 5000) {
      lastConnectAttempt = now;
      mqttClient.ensureConnected(); // This will now attempt once and not block
    }
    
    // Only proceed if connected
    if (mqttClient.isConnected()) {
      mqttClient.publishDiscoveryOnce();
      mqttClient.publishStatesIfReady(domain::Metrics::instance());
      mqttClient.loop();
    }
  }

  if (!USE_LVGL_UI) {
    // Touch handling
    handleTouchUI();
  }

  // Motor control policy (skip if forced-on test is active)
  if (MOTOR_ENABLE) {
    domain::ControlConfig cfg{PH_MAX, PH_HYST, ORP_MIN, ORP_HYST, M1_SPEED_PC, M2_SPEED_PC};
    control->update(cfg,
                    domain::Metrics::instance().havePh,
                    domain::Metrics::instance().phVal,
                    domain::Metrics::instance().haveOrp,
                    domain::Metrics::instance().orpMv,
                    FORCE_MOTOR_A_ON,
                    m1Running, m2Running);
    if (USE_LVGL_UI) updateLvglValues();
    else {
      if (lastM1Icon != m1Running) { lastM1Icon = m1Running; drawMotorIcon(gfx, M1_ICON_X, M1_ICON_Y, m1Running); }
      if (lastM2Icon != m2Running) { lastM2Icon = m2Running; drawMotorIcon(gfx, M2_ICON_X, M2_ICON_Y, m2Running); }
    }
  }
}
