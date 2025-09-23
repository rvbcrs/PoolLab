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
#include <stdio.h>
#include <stdarg.h>
#if !defined(USE_JC3248W535)
#include <Arduino_GFX_Library.h>
#endif
#include <HardwareSerial.h>
#include <SPI.h>
#if !defined(USE_JC3248W535)
#include <Wire.h>
#endif
#include <vector>
#if !defined(USE_JC3248W535)
// Legacy Adafruit fonts removed; LVGL handles fonts across boards
#endif
#include <lvgl.h>
#if !defined(USE_JC3248W535)
// Legacy Adafruit_GFX include removed; not used by LVGL path
#endif
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <esp_log.h>
#include "domain/DummySensor.h"
#include "domain/Telemetry.h"
#if !defined(USE_JC3248W535)
// Legacy Adafruit fonts removed
#endif
#if defined(BOARD_ESP32S3_35) && !defined(USE_JC3248W535)
#include "esp_bsp.h"
#include "display.h"
#include "lv_port.h"
#endif
#if defined(USE_JC3248W535)
#include "jc3248w535.h"
// #include <demos/lv_demos.h>
//#include <Wire.h>
#endif

#if defined(BOARD_ESP32S3_35) && defined(USE_JC3248W535)
#define LVGL_LOCK() jc3248w535_lock(1)
#define LVGL_UNLOCK() jc3248w535_unlock()
#elif defined(BOARD_ESP32S3_35)
#define LVGL_LOCK() bsp_display_lock(1)
#define LVGL_UNLOCK() bsp_display_unlock()
#else
#define LVGL_LOCK() true
#define LVGL_UNLOCK() do{}while(0)
#endif
#ifndef ZB_ENABLED
#if ((defined(FORCE_ZIGBEE) && FORCE_ZIGBEE) || (defined(HAS_ZIGBEE) && HAS_ZIGBEE)) && __has_include(<Zigbee.h>)
#define ZB_ENABLED 1
#else
#define ZB_ENABLED 0
#endif
#endif
#if ZB_ENABLED
#include <Zigbee.h>
#include <ZigbeeEP.h>
#include <ep/ZigbeeTempSensor.h>
#include <ep/ZigbeeAnalog.h>
#include <ep/ZigbeeFlowSensor.h>
#include <ep/ZigbeePressureSensor.h>
// Use Analog Input for pH and ORP (correct semantics); ZHA needs a quirk to label nicely
#include <esp_zigbee_secur.h>
#include <esp_zigbee_core.h>
static inline bool zb_is_joined(){
  return esp_zb_bdb_dev_joined();
}
static ZigbeeTempSensor zbTempSensor(10);
// Prefer standard HA clusters where possible for best ZHA compatibility
static ZigbeeFlowSensor  zbPh(11);
static ZigbeePressureSensor zbOrp(12);
static bool zbStarted = false;
static uint32_t zbCommissionUntilMs = 0;
static bool zbScanRequested = false;
static bool zbMaskAdjusted = false;
static uint32_t zbLastScanMs = 0;
static uint32_t zbCommissionStartMs = 0;
static bool zbMaskExpanded = false;
static bool zbEverJoined = false; // becomes true once Zigbee.connected() observed
// Writable thresholds via Zigbee (Analog Output)
static ZigbeeAnalog      zbPhMin(13);
static ZigbeeAnalog      zbPhMax(14);
static ZigbeeAnalog      zbOrpMin(15);
static ZigbeeAnalog      zbOrpMax(16);
// Defer AO writes from ZCL context to main loop to avoid reentrancy during interview
static volatile bool zbPhMinPending = false, zbPhMaxPending = false, zbOrpMinPending = false, zbOrpMaxPending = false;
static volatile float zbPhMinValue = 0, zbPhMaxValue = 0, zbOrpMinValue = 0, zbOrpMaxValue = 0;
static Preferences zbPrefs;
#endif
static bool wifiOff = false;
static volatile bool g_settingsRequested = false;
static volatile bool g_startPortalRequested = false;
static volatile bool g_ui_ip_dirty = false;
static String g_ui_ip_text;
static volatile bool g_showMainRequested = false;

extern "C" void requestShowMain(){ g_showMainRequested = true; }
#if ZB_ENABLED
static const char *ZB_PREF_NS = "poollab";
static const char *ZB_PREF_PAIR = "zb_pair"; // bool flag to start pairing on next boot
static const char *ZB_PREF_BOUND = "zb_bound"; // bool flag: device has joined a network before
#endif
// Global boot timestamp for UI grace periods
static uint32_t APP_BOOT_MS = 0;

// Core modules
#include "core/Storage.h"
#if !defined(USE_JC3248W535)
#include "core/DisplayBridge.h"
#endif
#include "domain/Metrics.h"
#include "domain/ControlPolicy.h"
// IO modules
#include "io/MqttClient.h"
#include "io/Touch.h"
#include "io/Tuya.h"
#include "io/ZigbeeClient.h"
#include "io/CaptivePortal.h"
#include "ui/UI.h"
// Provide C-linkage declarations for image assets used by UI when included here
extern "C" {
  extern const lv_img_dsc_t water_ph_32dp_E3E3E3_FILL0_wght400_GRAD0_opsz40;
  extern const lv_img_dsc_t water_orp_32dp_E3E3E3_FILL0_wght400_GRAD0_opsz40;
}
#if defined(BOARD_ESP32C6_TOUCH_1_47) && !defined(USE_JC3248W535)
#endif
#include "io/WebUI.h"
#include "io/WiFiManager.h"
#include "boards/BoardSelect.h"
#include "io/Buttons.h"
#include "io/MotorController.h"
// Icons
extern "C" const lv_img_dsc_t water_pump_24dp_E3E3E3_FILL0_wght400_GRAD0_opsz24;
extern "C" const lv_img_dsc_t water_ph_32dp_E3E3E3_FILL0_wght400_GRAD0_opsz40;
extern "C" const lv_img_dsc_t water_orp_32dp_E3E3E3_FILL0_wght400_GRAD0_opsz40;
extern "C" const lv_img_dsc_t link_32dp_E3E3E3_FILL0_wght400_GRAD0_opsz40;
extern "C" const lv_img_dsc_t link_off_32dp_E3E3E3_FILL0_wght400_GRAD0_opsz40;
extern "C" const lv_img_dsc_t link_16dp_999999_FILL0_wght400_GRAD0_opsz20;
extern "C" const lv_img_dsc_t link_off_16dp_999999_FILL0_wght400_GRAD0_opsz20;

#define FIRMWARE_VERSION __DATE__ " " __TIME__

// Local convenience accessor for metrics (scoped to this file only)
static inline domain::Metrics& METRICS(){ return domain::Metrics::instance(); }
extern "C" const lv_font_t lv_font_source_code_pro_16;
extern "C" const lv_font_t lv_font_montserrat_14;
extern "C" const lv_font_t lv_font_source_code_pro_18;
extern "C" const lv_font_t lv_font_source_code_pro_36;
extern "C" const lv_font_t lv_font_source_code_pro_36_bold;
#include "core/Log.h"
#if defined(USE_JC3248W535)
static jc3248w535_handles_t jc_handles; // zero-initialized
// Keep JC path free of legacy helpers; do not define pushLine/connectWiFi... stubs here
#endif

// ====== USER CONFIG ======
// Set to true for a minimal diagnostic mode (serial prints + color flashes)
static const bool DIAG_MODE = false;
static const uint32_t TUYA_BAUD = 115200; // 115200 default; change to 9600 if needed
static const int RX_A_PIN = 16;          // MCU -> WiFi
static const int RX_B_PIN = 17;          // WiFi -> MCU
static const bool USE_CHANNEL_B = true; // set false if only one direction
// Backlight pin (as in your working example)

#if defined(BOARD_ESP32C6_TOUCH_1_47)
static const int LCD_BL_PIN = 23;        // C6 original working backlight pin
#else
static const int LCD_BL_PIN = 23;
#endif

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
#if defined(BOARD_ESP32C6_TOUCH_1_47)
static const bool USE_LVGL_UI = true;  // C6 uses LVGL UI as on master
#else
static const bool USE_LVGL_UI = true;  // S3: LVGL cards UI
#endif
// Safe rollback: keep LVGL minimal to avoid crashes while debugging
// On S3 with JC driver, start with safe baseline UI to avoid complex widgets until stable
#if defined(BOARD_ESP32S3_35) && defined(USE_JC3248W535)
static const bool LVGL_SAFE_BASELINE = false; // enable full UI on S3
#else
static const bool LVGL_SAFE_BASELINE = false;
#endif

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
static lv_obj_t *lv_img_link = nullptr;
static lv_obj_t *lv_link_wrap = nullptr;
static lv_obj_t *lv_lbl_link_dbg = nullptr;
static lv_obj_t *lv_lbl_m1 = nullptr; // motor1 indicator
static lv_obj_t *lv_lbl_m2 = nullptr; // motor2 indicator
static lv_obj_t *lv_zb_modal = nullptr; // zigbee commissioning modal

// Settings widgets
static lv_obj_t *lv_lbl_speed1 = nullptr;
static lv_obj_t *lv_lbl_speed2 = nullptr;
static lv_obj_t *lv_sl_speed1 = nullptr;
static lv_obj_t *lv_sl_speed2 = nullptr;

// Forward declarations for LVGL helper functions (definitions are below metrics/globals)
static void lv_update_speed_labels();
static void on_ph_minus_cb(lv_event_t *e);
static void on_ph_plus_cb(lv_event_t *e);
static void on_orp_minus_cb(lv_event_t *e);
static void on_orp_plus_cb(lv_event_t *e);
static void on_all_off_cb(lv_event_t *e);
static void on_speed_save_cb(lv_event_t *e);
static void updateLvglValues();
static void showRangeEditorProxy(bool isPh);
// Enable dummy/test mode to generate values without the meter connected
static const bool DUMMY_MODE = true;  // set true to simulate values
static domain::DummySensor g_dummySensor(6.80f, 7.60f, 250);
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
      showRangeEditorProxy(ctx->isPh);
    }
  }
}

// ---- TB6612FNG motor driver (optional dosing pumps) ----
static const bool MOTOR_ENABLE = false; // disable motor control to free BOOT pin GPIO9
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
static bool emergencyStop = false; // when true, force both motors off until reboot or future clear
// Hysteresis for continuous control (no burst/cooldown)
static const float PH_HYST = 0.05f;   // stop when pH < (PH_MAX - PH_HYST)
static const int   ORP_HYST = 20;     // stop when ORP > (ORP_MIN + ORP_HYST)

// (helpers moved below prefs)

// (helpers moved below metrics and prefs)
// ---- WiFi + MQTT (Home Assistant via Mosquitto) ----
static String WIFI_SSID     = "";
static String WIFI_PASSWORD = "";
static String MQTT_HOST     = "192.168.0.248"; // default; overridden by storage
static uint16_t MQTT_PORT  = 1883;
static String MQTT_USER     = "";  // optional
static String MQTT_PASS     = "";  // optional
static const char* MQTT_CLIENTID = "pool-sniffer-c6";

// MQTT is handled by io::MqttClient now
static core::Storage storage("poolcfg");
#if !defined(USE_JC3248W535)
static core::DisplayBridge *displayBridge = nullptr;
#endif
static io::MotorController g_motor;
static io::MqttClient mqttClient;
static io::WiFiManager wifiMgr;
static io::ZigbeeClient zigbee;
static io::CaptivePortal portal;
static io::WebUI webui;
static core::Storage::Mode runMode = core::Storage::MODE_WIFI_MQTT;
static core::Storage::Mode savedMode = core::Storage::MODE_WIFI_MQTT;
static bool modeForced = false;
// Minimal UI heartbeat (JC simple mode)
#if defined(USE_JC3248W535)
static bool g_minimal_ui_active = false;
#endif
// --- Button for Zigbee commissioning (monitor both common BOOT pins)
#if defined(BOARD_ESP32C6_TOUCH_1_47)
static const int BTN_PIN1 = 9;   // C6: BOOT (GPIO9)
static const int BTN_PIN2 = 0;   // backup
#elif defined(BOARD_ESP32S3_35)
static const int BTN_PIN1 = -1;  // S3: no button logic
static const int BTN_PIN2 = -1;  // unused
#else
static const int BTN_PIN1 = 0;
static const int BTN_PIN2 = -1;
#endif
// If BOOT is not wired on this board, fall back to GPIO0 only
static io::Buttons g_buttons;

// (WiFi events handled in io::WiFiManager)
// Extra: in DIAG_MODE we try multiple candidates in case of board revision
static const int BL_CANDIDATES[] = {2, 1, 3, 20, 21, 19, 18, 17, 16, 15, 14, 13, 12, 11, 5, 4};
static const size_t BL_CANDIDATES_COUNT = sizeof(BL_CANDIDATES) / sizeof(BL_CANDIDATES[0]);
// =========================

// ---- Display setup (match working HelloWorld) ----



#if defined(BOARD_ESP32C6_TOUCH_1_47)
#include "core/Board.h"
#include "core/boards/Esp32C6Board.h"
#include "core/display/DisplayDriver.h"
#include "core/display/St7789C6.h"
#include "core/touch/TouchDriver.h"
#include "core/touch/Axs5106L.h"
static core::Esp32C6Board g_boardC6;
Arduino_DataBus *bus = new Arduino_HWSPI(15 /* DC */, 14 /* CS */, 1 /* SCK */, 2 /* MOSI */, -1 /* MISO */);
Arduino_GFX *gfx = new Arduino_ST7789(bus, DISPLAY_CFG.rstPin, DISPLAY_CFG.rotation, false /* IPS */,
                                      DISPLAY_CFG.width, DISPLAY_CFG.height,
                                      DISPLAY_CFG.colOffset1, DISPLAY_CFG.rowOffset1,
                                      DISPLAY_CFG.colOffset2, DISPLAY_CFG.rowOffset2);
static core::St7789C6 displayDriver(gfx, bus);
static core::Axs5106L touchDriver(18,19,20,21,0x63);
// Legacy UI removed; C6 uses LVGL UI like S3

#if 0
static void lcd_reg_init(void) {}
#endif
#endif



// ---- UARTs ----
#ifndef ARDUINO_USB_CDC_ON_BOOT
#define ARDUINO_USB_CDC_ON_BOOT 1
#endif
HardwareSerial TUYA_A(0); // UART0 RX-only
HardwareSerial TUYA_B(1); // UART1 RX-only

// ---- Tuya helpers ----
// moved to io/Tuya

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
static void on_ph_minus_cb(lv_event_t *e){ (void)e; if (M1_SPEED_PC>=5) M1_SPEED_PC-=5; else M1_SPEED_PC=0; storage.setM1Speed(M1_SPEED_PC); lv_update_speed_labels(); }
static void on_ph_plus_cb (lv_event_t *e){ (void)e; if (M1_SPEED_PC<=95) M1_SPEED_PC+=5; else M1_SPEED_PC=100; storage.setM1Speed(M1_SPEED_PC); lv_update_speed_labels(); }
static void on_orp_minus_cb(lv_event_t *e){ (void)e; if (M2_SPEED_PC>=5) M2_SPEED_PC-=5; else M2_SPEED_PC=0; storage.setM2Speed(M2_SPEED_PC); lv_update_speed_labels(); }
static void on_orp_plus_cb (lv_event_t *e){ (void)e; if (M2_SPEED_PC<=95) M2_SPEED_PC+=5; else M2_SPEED_PC=100; storage.setM2Speed(M2_SPEED_PC); lv_update_speed_labels(); }
static void on_all_off_cb(lv_event_t *e){
  (void)e;
  // Immediate stop of both motors (emergency)
  emergencyStop = true;
  m1Running = false; m2Running = false;
  if (MOTOR_ENABLE) {
    // Disable driver and outputs
    digitalWrite(TB_STBY, LOW);
    digitalWrite(M1_IN1, LOW);
    digitalWrite(M1_IN2, LOW);
    digitalWrite(M2_IN1, LOW);
    digitalWrite(M2_IN2, LOW);
    ledcWrite(M1_PWM, 0);
    ledcWrite(M2_PWM, 0);
  }
  // Hide pump icons if present
  if (USE_LVGL_UI) {
    if (lv_img_pump_ph) { lv_obj_add_flag(lv_img_pump_ph, LV_OBJ_FLAG_HIDDEN); }
    if (lv_img_pump_ph_shadow) { lv_obj_add_flag(lv_img_pump_ph_shadow, LV_OBJ_FLAG_HIDDEN); }
    if (lv_img_pump_orp) { lv_obj_add_flag(lv_img_pump_orp, LV_OBJ_FLAG_HIDDEN); }
    if (lv_img_pump_orp_shadow) { lv_obj_add_flag(lv_img_pump_orp_shadow, LV_OBJ_FLAG_HIDDEN); }
  }
}
static void on_speed_save_cb(lv_event_t *e){ (void)e; storage.setM1Speed(M1_SPEED_PC); storage.setM2Speed(M2_SPEED_PC); }

// Alert margins and border thickness for near/exceed thresholds (used by LVGL updater)
static const float WARN_MARGIN_PH = 0.05f;   // pH within 0.05 of min/max
static const int   WARN_MARGIN_ORP = 20;     // mV within 20 of min/max

static void ui_update_values_async(void *){ ui::updateValues(); }
static void updateLvglValues(){
  if (!USE_LVGL_UI) return;
  #if defined(BOARD_ESP32S3_35) && defined(USE_JC3248W535)
  // On JC path, rely on the LVGL timer in UI.cpp to update values to avoid flooding
  return;
  #endif
  #if defined(BOARD_ESP32S3_35)
  if (!LVGL_LOCK()) return;
  #endif
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
    // Color by thresholds for ORP
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
    if (lv_lbl_orp_shadow) lv_label_set_text(lv_lbl_orp_shadow, lv_label_get_text(lv_lbl_orp));
  }
  if (lv_lbl_temp) {
    if (METRICS().haveTemp) { char b[24]; snprintf(b, sizeof(b), "%.1f C", (double)METRICS().tempC); lv_label_set_text(lv_lbl_temp, b); }
    else lv_label_set_text(lv_lbl_temp, "--.- C");
  }
  // Bottom-right: show IP in WiFi/MQTT mode; show link icon when Zigbee connected in Zigbee mode
  if (runMode == core::Storage::MODE_WIFI_MQTT) {
    if (lv_img_link) { lv_obj_add_flag(lv_img_link, LV_OBJ_FLAG_HIDDEN); }
    if (lv_lbl_ip) {
      String ip = (WiFi.status()==WL_CONNECTED)? WiFi.localIP().toString() : String("--");
      char b[48]; snprintf(b, sizeof(b), "IP: %s", ip.c_str()); lv_label_set_text(lv_lbl_ip, b);
      lv_obj_clear_flag(lv_lbl_ip, LV_OBJ_FLAG_HIDDEN);
    }
  } else {
    // Zigbee mode: hide IP, show link/link_off
    if (lv_lbl_ip) { lv_obj_add_flag(lv_lbl_ip, LV_OBJ_FLAG_HIDDEN); }
    if (!lv_img_link) {
      // Lazily create if not yet created (robust against init order)
      lv_img_link = lv_img_create(lv_scr_act());
      lv_img_set_src(lv_img_link, &link_off_16dp_999999_FILL0_wght400_GRAD0_opsz20);
      lv_obj_align(lv_img_link, LV_ALIGN_BOTTOM_RIGHT, -10, -6);
      // native size (no zoom)
      // lv_img_set_zoom(lv_img_link, 256);
      lv_obj_set_style_img_recolor_opa(lv_img_link, LV_OPA_COVER, 0);
      lv_obj_set_style_img_recolor(lv_img_link, lv_color_black(), 0);
      lv_obj_set_style_img_opa(lv_img_link, LV_OPA_COVER, 0);
      lv_obj_clear_flag(lv_img_link, LV_OBJ_FLAG_HIDDEN);
      lv_obj_move_foreground(lv_img_link);
    }
    if (lv_img_link) {
      #if ZB_ENABLED
      bool connected_now = Zigbee.connected();
      bool joined_now = zb_is_joined();
      if (connected_now && joined_now) {
        lv_img_set_src(lv_img_link, &link_16dp_999999_FILL0_wght400_GRAD0_opsz20);
      } else {
        lv_img_set_src(lv_img_link, &link_off_16dp_999999_FILL0_wght400_GRAD0_opsz20);
      }
      #else
      lv_img_set_src(lv_img_link, &link_off_16dp_999999_FILL0_wght400_GRAD0_opsz20);
      #endif
      lv_obj_clear_flag(lv_img_link, LV_OBJ_FLAG_HIDDEN);
      lv_obj_move_foreground(lv_img_link);
    }
  }
  #if defined(BOARD_ESP32S3_35)
  LVGL_UNLOCK();
  #endif
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
static void showRangeEditorProxy(bool isPh){
  ui::showRangeEditor(isPh);
}

static void showRangeDialog(bool isPh){
  // Legacy dialog replaced by UI module implementation
  (void)isPh;
  ui::showRangeEditor(isPh);
}

static void showZigbeeCommissioningModal(uint32_t seconds){
  if (!USE_LVGL_UI) { ESP_LOGI("ZB", "Commissioning started"); return; }
  if (lv_zb_modal) { lv_obj_del(lv_zb_modal); lv_zb_modal = nullptr; }
  lv_obj_t *modal = lv_obj_create(lv_layer_top());
  lv_obj_set_size(modal, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
  lv_obj_set_style_bg_opa(modal, LV_OPA_50, 0);
  lv_obj_set_style_bg_color(modal, lv_color_black(), 0);
  lv_obj_clear_flag(modal, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(modal, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_bg_opa(modal, LV_OPA_TRANSP, LV_PART_SCROLLBAR);
  if (lv_tv) lv_obj_clear_flag(lv_tv, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *dlg = lv_obj_create(modal);
  lv_obj_set_size(dlg, lv_disp_get_hor_res(NULL)-40, lv_disp_get_ver_res(NULL)-40);
  lv_obj_center(dlg);
  lv_obj_set_style_radius(dlg, 10, 0);
  lv_obj_set_style_pad_all(dlg, 12, 0);
  lv_obj_clear_flag(dlg, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(dlg, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t *title = lv_label_create(dlg);
  lv_label_set_text(title, "Zigbee commissioning");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

  lv_obj_t *spinner = lv_spinner_create(dlg, 1000, 60);
  lv_obj_set_size(spinner, 28, 28);
  lv_obj_align(spinner, LV_ALIGN_CENTER, 0, -6);
  lv_obj_t *msg = lv_label_create(dlg);
  static uint32_t zb_modal_deadline = 0;
  zb_modal_deadline = millis() + seconds * 1000UL;
  char b[48]; snprintf(b, sizeof(b), "Pairing... %us", (unsigned)seconds);
  lv_label_set_text(msg, b);
  lv_obj_align(msg, LV_ALIGN_CENTER, 0, 22);
  // Timer to update countdown every second
  lv_timer_t *ct = lv_timer_create([](lv_timer_t *tm){
    if (!lv_zb_modal) { lv_timer_del(tm); return; }
    uint32_t now = millis();
    uint32_t remain = (now >= zb_modal_deadline) ? 0 : (zb_modal_deadline - now + 999) / 1000;
    lv_obj_t *label = (lv_obj_t *)tm->user_data;
    if (label) {
      char bb[48]; snprintf(bb, sizeof(bb), "Pairing... %us", (unsigned)remain);
      lv_label_set_text(label, bb);
    }
    if (remain == 0) { lv_timer_del(tm); }
  }, 1000, msg);

  lv_obj_add_event_cb(modal, [](lv_event_t *e){
    if (lv_event_get_code(e) == LV_EVENT_DELETE) {
      if (lv_tv) lv_obj_add_flag(lv_tv, LV_OBJ_FLAG_SCROLLABLE);
      lv_zb_modal = nullptr;
    }
  }, LV_EVENT_ALL, NULL);
  lv_zb_modal = modal;
}

// Simple modal to instruct user to hold BOOT for 3s to start pairing
static void showZigbeeHoldToPairModal(){
  if (!USE_LVGL_UI) { ESP_LOGI("ZB", "Hold BOOT 3s to start pairing"); return; }
  if (lv_zb_modal) { lv_obj_del(lv_zb_modal); lv_zb_modal = nullptr; }
  lv_obj_t *modal = lv_obj_create(lv_layer_top());
  lv_obj_set_size(modal, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
  lv_obj_set_style_bg_opa(modal, LV_OPA_50, 0);
  lv_obj_set_style_bg_color(modal, lv_color_black(), 0);
  lv_obj_clear_flag(modal, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(modal, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_bg_opa(modal, LV_OPA_TRANSP, LV_PART_SCROLLBAR);
  if (lv_tv) lv_obj_clear_flag(lv_tv, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *dlg = lv_obj_create(modal);
  lv_obj_set_size(dlg, lv_disp_get_hor_res(NULL)-40, lv_disp_get_ver_res(NULL)-40);
  lv_obj_center(dlg);
  lv_obj_set_style_radius(dlg, 10, 0);
  lv_obj_set_style_pad_all(dlg, 12, 0);
  lv_obj_clear_flag(dlg, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(dlg, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t *title = lv_label_create(dlg);
  lv_label_set_text(title, "Hold BOOT 3s to start Zigbee pairing");
  lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

  lv_obj_add_event_cb(modal, [](lv_event_t *e){
    if (lv_event_get_code(e) == LV_EVENT_DELETE) {
      if (lv_tv) lv_obj_add_flag(lv_tv, LV_OBJ_FLAG_SCROLLABLE);
      lv_zb_modal = nullptr;
    }
  }, LV_EVENT_ALL, NULL);
  lv_zb_modal = modal;
  // Auto-close hint after 1500 ms
  lv_timer_t *t = lv_timer_create([](lv_timer_t *tm){
    (void)tm;
    if (lv_zb_modal) { lv_obj_del(lv_zb_modal); lv_zb_modal = nullptr; }
  }, 1500, NULL);
  lv_timer_set_repeat_count(t, 1);
}

#if ZB_ENABLED
static void zb_start_and_commission(uint8_t seconds){
  if (!zbStarted) {
    // Register endpoints and start stack
    ESP_LOGI("ZB", "Commissioning: preparing endpoints");
    zbTempSensor.setManufacturerAndModel("PoolLab", "Pool Temperature");
    zbTempSensor.setMinMaxValue(0, 60);
    // Configure Flow/Pressure endpoints for pH and ORP (ZHA-friendly)
    zbPh.setManufacturerAndModel("PoolLab", "Pool pH");
    zbPh.setMinMaxValue(0.0f, 14.0f);
    zbOrp.setManufacturerAndModel("PoolLab", "Pool ORP");
    zbOrp.setMinMaxValue(-2000, 2000);
    // Writable thresholds (Analog Output)
    zbPhMin.addAnalogOutput();
    zbPhMax.addAnalogOutput();
    zbOrpMin.addAnalogOutput();
    zbOrpMax.addAnalogOutput();
    // Defer writes to main loop to avoid blocking ZCL thread during interview
    zbPhMin.onAnalogOutputChange([](float v){ zbPhMinValue = v; zbPhMinPending = true; });
    zbPhMax.onAnalogOutputChange([](float v){ zbPhMaxValue = v; zbPhMaxPending = true; });
    zbOrpMin.onAnalogOutputChange([](float v){ zbOrpMinValue = v; zbOrpMinPending = true; });
    zbOrpMax.onAnalogOutputChange([](float v){ zbOrpMaxValue = v; zbOrpMaxPending = true; });
    Zigbee.setRxOnWhenIdle(true);
    // Start on coordinator channel 11; we'll expand to all channels after ~20s if needed
    Zigbee.setPrimaryChannelMask(1u << 11);
    Zigbee.setScanDuration(4); // max
    Zigbee.setTimeout(120000);
    Zigbee.addEndpoint(&zbTempSensor);
    Zigbee.addEndpoint(&zbPh);
    Zigbee.addEndpoint(&zbOrp);
    Zigbee.addEndpoint(&zbPhMin);
    Zigbee.addEndpoint(&zbPhMax);
    Zigbee.addEndpoint(&zbOrpMin);
    Zigbee.addEndpoint(&zbOrpMax);
    // Allow multiple binding records; useful for ZHA reconfigure
    ZigbeeEP::allowMultipleBinding(true);
    // Router mode with maximum TX power
    ESP_LOGI("ZB", "Commissioning: starting Zigbee (factory-new, ROUTER, erase NVS)");
    bool ok = Zigbee.begin(ZIGBEE_ROUTER, true);
    // Boost 802.15.4 TX power for better range/link margin
    esp_zb_set_tx_power(20);
    ESP_LOGI("ZB", "begin() -> %s", ok ? "OK" : "FAIL");
    (void)ok;
    // Configure reporting and push initial values
    zbTempSensor.setReporting(1, 0, 1);
    // Ensure pH/ORP report at least every 30s and on small changes
    zbPh.setReporting(0, 30, 0.01f);
    zbOrp.setReporting(0, 30, 5);
    float initT = METRICS().haveTemp ? METRICS().tempC : 25.0f;
    float initPh = METRICS().havePh ? METRICS().phVal : 7.00f;
    int16_t initOrp = METRICS().haveOrp ? (int16_t)lrintf(METRICS().orpMv) : (int16_t)300;
    zbTempSensor.setTemperature(initT);
    zbTempSensor.reportTemperature();
    // Push initial pH/ORP immediately so ZHA entities become available fast
    zbPh.setFlow(initPh);
    zbPh.report();
    zbOrp.setPressure(initOrp);
    zbOrp.report();
    // Post initial values after a longer delay to avoid race during interview
    lv_timer_t *zb_ai_init = lv_timer_create([](lv_timer_t *tm){
      (void)tm;
      float ph = METRICS().havePh ? METRICS().phVal : 7.00f;
      int16_t orp = METRICS().haveOrp ? (int16_t)lrintf(METRICS().orpMv) : (int16_t)300;
      zbPh.setFlow(ph);
      zbPh.report();
      zbOrp.setPressure(orp);
      zbOrp.report();
    }, 2000, NULL);
    lv_timer_set_repeat_count(zb_ai_init, 1);
    // Writable outputs use default values; user can set them from ZHA
    zbStarted = true;
  }
  // Track commissioning window
  zbCommissionUntilMs = millis() + (uint32_t)seconds * 1000UL;
  zbCommissionStartMs = millis();
  zbMaskExpanded = false;
}
#endif

#if ZB_ENABLED
static void zb_start_joined(){
  if (zbStarted) return;
  ESP_LOGI("ZB", "Start Zigbee (joined mode, no commissioning)");
  // Prepare endpoints identical to commissioning, but don't erase NVS
  zbTempSensor.setManufacturerAndModel("PoolLab", "Pool Temperature");
  zbTempSensor.setMinMaxValue(0, 60);
  zbPh.setManufacturerAndModel("PoolLab", "Pool pH");
  zbPh.setMinMaxValue(0.0f, 14.0f);
  zbOrp.setManufacturerAndModel("PoolLab", "Pool ORP");
  zbOrp.setMinMaxValue(-2000, 2000);
  // Writable thresholds
  zbPhMin.addAnalogOutput();
  zbPhMax.addAnalogOutput();
  zbOrpMin.addAnalogOutput();
  zbOrpMax.addAnalogOutput();
  zbPhMin.onAnalogOutputChange([](float v){ zbPhMinValue = v; zbPhMinPending = true; });
  zbPhMax.onAnalogOutputChange([](float v){ zbPhMaxValue = v; zbPhMaxPending = true; });
  zbOrpMin.onAnalogOutputChange([](float v){ zbOrpMinValue = v; zbOrpMinPending = true; });
  zbOrpMax.onAnalogOutputChange([](float v){ zbOrpMaxValue = v; zbOrpMaxPending = true; });
  Zigbee.setRxOnWhenIdle(true);
  // Broad channel mask; device will use stored network params
  Zigbee.setPrimaryChannelMask(0x07FFF800);
  Zigbee.setScanDuration(3);
  Zigbee.setTimeout(120000);
  Zigbee.addEndpoint(&zbTempSensor);
  Zigbee.addEndpoint(&zbPh);
  Zigbee.addEndpoint(&zbOrp);
  Zigbee.addEndpoint(&zbPhMin);
  Zigbee.addEndpoint(&zbPhMax);
  Zigbee.addEndpoint(&zbOrpMin);
  Zigbee.addEndpoint(&zbOrpMax);
  ZigbeeEP::allowMultipleBinding(true);
  bool ok = Zigbee.begin(ZIGBEE_ROUTER, false /* erase_nvs */);
  esp_zb_set_tx_power(20);
  ESP_LOGI("ZB", "begin(joined) -> %s", ok ? "OK" : "FAIL");
  zbTempSensor.setReporting(1, 0, 1);
  zbPh.setReporting(0, 30, 0.01f);
  zbOrp.setReporting(0, 30, 5);
  zbStarted = true;
}
#endif

// ---- Simple vector icons (drawn with primitives) ----
// Legacy Arduino_GFX icon helpers removed

// Legacy C6 GFX stubs removed; use LVGL-only path for both boards
static inline void drawStaticUI() {}
static inline void drawPagination() {}
static void updateValueAreas() { updateLvglValues(); }

// WiFi helpers are fully handled by WiFiManager now

static void ensureMqtt() {
  mqttClient.setStorage(&storage);
  mqttClient.setThresholdRefs(&PH_MIN, &PH_MAX, &ORP_MIN, &ORP_MAX);
  // Load MQTT config from storage (fallback to defaults)
  {
    String h = storage.getMqttHost(MQTT_HOST);
    uint16_t p = storage.getMqttPort(MQTT_PORT);
    String u = storage.getMqttUser(MQTT_USER);
    String pw= storage.getMqttPass(MQTT_PASS);
    MQTT_HOST = h; MQTT_PORT = p; MQTT_USER = u; MQTT_PASS = pw;
  }
  // Skip MQTT setup entirely if no host configured
  if (MQTT_HOST.length() == 0) {
      return;
    }
  mqttClient.begin(MQTT_HOST.c_str(), MQTT_PORT, MQTT_USER.length()?MQTT_USER.c_str():nullptr, MQTT_PASS.length()?MQTT_PASS.c_str():nullptr, MQTT_CLIENTID);
}

static void publishDiscoveryOnce() { mqttClient.publishDiscoveryOnce(); }

static void publishStatesIfReady() { mqttClient.publishStatesIfReady(domain::Metrics::instance()); }
extern "C" void requestModeChange(int mode){
  // mode: 1 = Zigbee, 0 = WiFi
  runMode = (mode==1) ? core::Storage::MODE_ZIGBEE : core::Storage::MODE_WIFI_MQTT;
  storage.setMode(runMode);
  if (runMode == core::Storage::MODE_ZIGBEE) {
    if (WiFi.isConnected()) WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    wifiOff = true;
    #if ZB_ENABLED
    if (!zbStarted) {
      if (zbEverJoined) zb_start_joined(); else ui::showHoldToPair();
    }
#endif // C6 legacy GFX-only
  } else {
    wifiOff = false;
    WIFI_SSID = storage.getWifiSsid(WIFI_SSID);
    WIFI_PASSWORD = storage.getWifiPass(WIFI_PASSWORD);
    #if ZB_ENABLED
    if (zbStarted) { ESP.restart(); }
    #endif
    if (WIFI_SSID.length()==0) { if (!portal.isActive()) { portal.setStorage(&storage); portal.beginAP("PoolLab-Setup"); } }
    else { if (portal.isActive()) portal.stop(); WiFi.mode(WIFI_STA); wifiMgr.ensureSta(); ensureMqtt(); }
  }
}

// ---- Dummy telemetry generator ----
static void updateDummyTelemetry() {
  static bool inited = false;
  static uint32_t last = 0; 
  uint32_t now = millis();
  if (!inited) {
    randomSeed((uint32_t)esp_random());
    METRICS().phVal = (PH_MIN + PH_MAX) * 0.5f;
    METRICS().orpMv = (float)ORP_MIN + 50.0f;
    METRICS().tempC = 25.0f;
    METRICS().havePh = METRICS().haveOrp = METRICS().haveTemp = true;
    METRICS().preferPhPrimary = true;
    inited = true;
    last = 0;
  }
  if (now - last < 1500) return; 
  last = now;

  static uint8_t phState = 0; 
  static uint32_t phPlateauUntil = 0;
  const float phStep = 0.10f;
  const float phHighTarget = PH_MAX + 0.15f;
  const float phLowTarget  = PH_MIN + 0.10f;
  switch (phState) {
    case 0: METRICS().phVal += phStep; if (METRICS().phVal >= phHighTarget) { METRICS().phVal = phHighTarget; phState = 1; phPlateauUntil = now + 6000; } break;
    case 1: if ((int32_t)(now - phPlateauUntil) >= 0) { phState = 2; } break;
    case 2: METRICS().phVal -= phStep; if (METRICS().phVal <= phLowTarget) { METRICS().phVal = phLowTarget; phState = 3; phPlateauUntil = now + 4000; } break;
    default: if ((int32_t)(now - phPlateauUntil) >= 0) { phState = 0; } break;
  }
  METRICS().phVal = constrain(METRICS().phVal, 3.0f, 14.0f);

  static uint8_t orpState = 0; 
  static uint32_t orpPlateauUntil = 0;
  const float orpStep = 4.0f;
  const float orpLowTarget  = (float)ORP_MIN - 40.0f;
  const float orpHighTarget = (float)ORP_MIN + 140.0f;
  switch (orpState) {
    case 0: METRICS().orpMv -= orpStep; if (METRICS().orpMv <= orpLowTarget)  { METRICS().orpMv = orpLowTarget;  orpState = 1; orpPlateauUntil = now + 6000; } break;
    case 1: if ((int32_t)(now - orpPlateauUntil) >= 0) { orpState = 2; } break;
    case 2: METRICS().orpMv += orpStep; if (METRICS().orpMv >= orpHighTarget) { METRICS().orpMv = orpHighTarget; orpState = 3; orpPlateauUntil = now + 4000; } break;
    default: if ((int32_t)(now - orpPlateauUntil) >= 0) { orpState = 0; } break;
  }
  METRICS().orpMv = constrain(METRICS().orpMv, -2000.0f, 2000.0f);
  METRICS().tempC += (float)random(-3, 4) / 10.0f;
  METRICS().tempC = constrain(METRICS().tempC, 5.0f, 40.0f);

  updateValueAreas();
}

void pushLine(const String &s) {
  String t = s;
  if (t.length() > MAX_LINE_CHARS) t = t.substring(0, MAX_LINE_CHARS);
  if (lines.size() >= MAX_LINES) lines.erase(lines.begin());
  lines.push_back(t);
}

#if defined(BOARD_ESP32C6_TOUCH_1_47) && !defined(USE_JC3248W535)
static inline void drawScreen() {}
#else
static inline void drawScreen() {}
#endif



// (legacy parser verwijderd; io/Tuya wordt gebruikt)

// Parser moved to io/Tuya

void setup() {
  // USB serial (do not block UI waiting for monitor)
  Serial.begin(115200);
  delay(100);
  Serial.setTimeout(50);
  core::Log::init(true);
  ESP_LOGI("BOOT", "Boot start");
  APP_BOOT_MS = millis();
  // Avoid enabling debug output to USB CDC to prevent any hidden blocking
  // Serial.setDebugOutput(true);

  #if defined(USE_JC3248W535)
  ESP_LOGI("MAIN", "Starting display init (JC3248W535)");
  // Ensure Arduino Wire (driver_ng) is not used on S3; no Wire calls on JC path
  
    (void)jc3248w535_begin_simple(90, &jc_handles);
    (void)jc3248w535_backlight_set(100);
    // Minimal banner before full UI to confirm panel
    if (jc3248w535_lock(5)) {
      lv_obj_clean(lv_scr_act());
      lv_obj_t *label = lv_label_create(lv_scr_act());
      lv_label_set_text(label, "Starting UI...");
      lv_obj_center(label);
      jc3248w535_unlock();
    }
    g_minimal_ui_active = true;
    ESP_LOGI("MAIN", "Proceeding to full UI build");
  #endif

  // Create a pinned UI task on core 1 for LVGL processing (S3 only, but NOT when using JC BSP which provides its own LVGL task)
  #if defined(BOARD_ESP32S3_35) && !defined(USE_JC3248W535)
  static TaskHandle_t uiTaskHandle = NULL;
  // Delay starting the task slightly to ensure LVGL/BSP created the default display
  vTaskDelay(pdMS_TO_TICKS(10));
  xTaskCreatePinnedToCore(
    [](void *param){
      (void)param;
      for(;;){
        if (lv_disp_get_default() != NULL) {
          if (LVGL_LOCK()) { lv_timer_handler(); LVGL_UNLOCK(); }
        }
        vTaskDelay(pdMS_TO_TICKS(16));
      }
    },
    "ui_task",
    4096,
    NULL,
    1,
    &uiTaskHandle,
    1 // core 1
  );
  #endif

  // Avoid Arduino SPI init on S3 JC path (conflicts with QSPI LCD bus)
  #if !(defined(BOARD_ESP32S3_35) && defined(USE_JC3248W535))
    // Use board abstraction to init peripherals
  #if defined(BOARD_ESP32C6_TOUCH_1_47)
    g_boardC6.earlyInit();
    g_boardC6.initPeripherals();
  #else
  SPI.begin();
    #endif
  #else
    ESP_LOGI("MAIN", "Skipping SPI.begin() on S3 JC path");
  #endif

  // Backlight on (default)
  // #if !defined(BOARD_ESP32S3_35) && !defined(USE_JC3248W535)
  // if (LCD_BL_PIN >= 0) { pinMode(LCD_BL_PIN, OUTPUT); digitalWrite(LCD_BL_PIN, HIGH); }
  // #endif
  
  // Remove broad BL scan to avoid toggling reserved pins

  #if defined(BOARD_ESP32C6_TOUCH_1_47) && !defined(USE_JC3248W535)
  // Hardware reset pulse on LCD reset pin for the active board
  if (DISPLAY_CFG.rstPin >= 0) {
    pinMode(DISPLAY_CFG.rstPin, OUTPUT);
    digitalWrite(DISPLAY_CFG.rstPin, LOW);
    delay(10);
    digitalWrite(DISPLAY_CFG.rstPin, HIGH);
    delay(120);
  }
  #endif

  // LCD init
  // For ESP32-S3 3.5" board, the display is initialized via BSP when LVGL UI starts below.
  
  // Initialize Arduino_GFX on C6 legacy path and light backlight
  #if defined(BOARD_ESP32C6_TOUCH_1_47) && !defined(USE_JC3248W535)
    if (gfx) {
      displayDriver.begin();
      // Match master: rotation(1) for landscape
      displayDriver.setRotation(1);
      // Turn on backlight
      if (LCD_BL_PIN >= 0) { pinMode(LCD_BL_PIN, OUTPUT); digitalWrite(LCD_BL_PIN, HIGH); }
      // Clear screen to black
  gfx->fillScreen(BLACK);
      delay(10);
      // Only draw legacy banner/UI when not using LVGL UI
      if (!USE_LVGL_UI) {
        gfx->setTextColor(WHITE);
        gfx->setTextWrap(false);
        const char *banner = "PoolLab Ready";
        int16_t text_w = (int16_t)(6 * (int)strlen(banner) * 2);
        int16_t text_h = (int16_t)(8 * 2);
        int16_t cx = (int16_t)((int)gfx->width() - text_w) / 2;
        int16_t cy = (int16_t)((int)gfx->height() - text_h) / 2;
        gfx->setTextSize(2);
        gfx->setCursor(cx < 0 ? 0 : cx, cy < 0 ? 0 : cy);
        gfx->print(banner);
        delay(150);
        gfx->fillScreen(BLACK);
        // legacy static draw flag removed
        drawStaticUI();
        updateValueAreas();
      }
    }
  #endif
 

  // Begin touch - MUST be after display init but BEFORE LVGL (matching working code)
  // io::touchBegin(); // MOVED to after LVGL init

  if (USE_LVGL_UI) {
    // Initialize LVGL display
    lv_disp_t *disp = nullptr;
    
    #if !defined(USE_JC3248W535)
    // Initialize LVGL via Arduino_GFX bridge
    displayBridge = new core::DisplayBridge(gfx);
    displayBridge->initLvgl(20);
    disp = displayBridge->registerDisplay();
    #else
    // With JC path active, 'disp' comes from jc3248w535; keep as lv_disp_get_default()
    disp = lv_disp_get_default();
    #endif
    
    // Ensure LVGL is locked on BSP path during UI creation (S3 BSP)
    #if defined(BOARD_ESP32S3_35)
    LVGL_LOCK();
    #endif
    ESP_LOGI("UI", "Before ui::init; res=%dx%d", (int)lv_disp_get_hor_res(NULL), (int)lv_disp_get_ver_res(NULL));
    ui::init(disp);
    ESP_LOGI("UI", "After ui::init");
    // Ensure we remove the temporary banner and any prior objects before building UI
    lv_obj_clean(lv_scr_act());
    // Apply a dark theme so text contrasts on black backgrounds (do this under lock)
    {
      lv_theme_t *th = lv_theme_default_init(
        disp,
        lv_palette_main(LV_PALETTE_BLUE),
        lv_palette_main(LV_PALETTE_RED),
        true, // dark mode
        &lv_font_montserrat_14
      );
      lv_disp_set_theme(disp, th);
    }
    // Build a safe baseline only for S3 JC path; C6 will build full tile UI below
    #if defined(BOARD_ESP32S3_35) && defined(USE_JC3248W535)
    ui::build(false);
    // Seed demo metrics so values are visible immediately on S3 JC path
    {
      auto &M = domain::Metrics::instance();
      M.havePh = true; M.haveOrp = true; M.haveTemp = true;
      M.phVal = (PH_MIN + PH_MAX) * 0.5f;
      M.orpMv = (float)ORP_MIN + 80.0f;
      M.tempC = 25.0f;
    }
    // Update values once for initial paint (under lock)
    updateLvglValues();
    // Set initial IP label (if connected later, handlers will refresh)
    {
      String ip = (WiFi.status()==WL_CONNECTED)? WiFi.localIP().toString() : String("--");
      ui::setIp(ip.c_str());
    }
    // Unlock immediately after UI work to avoid blocking BSP LVGL task
    LVGL_UNLOCK();
    // Tune touch repeat thresholds to avoid double key insert on on-screen keyboard
    {
      lv_indev_t *indev = bsp_display_get_input_dev();
      if (indev && indev->driver) {
        indev->driver->long_press_time = 800;           // ms before repeat starts
        indev->driver->long_press_repeat_time = 0;      // disable key repeat to avoid doubles
      }
    }
    #endif
    // Load persisted configuration early so UI defaults and startup mode are correct
    storage.begin(false);
    PH_MIN = storage.getPhMin(PH_MIN);
    PH_MAX = storage.getPhMax(PH_MAX);
    ORP_MIN = storage.getOrpMin(ORP_MIN);
    ORP_MAX = storage.getOrpMax(ORP_MAX);
    M1_SPEED_PC = (uint8_t)storage.getM1Speed(M1_SPEED_PC);
    M2_SPEED_PC = (uint8_t)storage.getM2Speed(M2_SPEED_PC);
  // Force WiFi on S3 (match C6 WiFi-first behavior for UI)
  #if defined(BOARD_ESP32S3_35)
  runMode = core::Storage::MODE_WIFI_MQTT;
  #else
    runMode = storage.getMode(core::Storage::MODE_ZIGBEE);
  #endif
    savedMode = runMode;
    // Connect UI slider handlers to storage-backed speeds
    ui::Handlers h; h.onSpeedChange = [](int idx, int value){
      value = constrain(value, 0, 100);
      if (idx==1) { M1_SPEED_PC = (uint8_t)value; storage.setM1Speed(M1_SPEED_PC); }
      else if (idx==2) { M2_SPEED_PC = (uint8_t)value; storage.setM2Speed(M2_SPEED_PC); }
    };
    h.onModeToggle = [](bool zigbee){
      runMode = zigbee ? core::Storage::MODE_ZIGBEE : core::Storage::MODE_WIFI_MQTT;
      #if !defined(BOARD_ESP32S3_35)
      storage.setMode(runMode);
      #else
      runMode = core::Storage::MODE_WIFI_MQTT; // ignore Zigbee on S3
      #endif
      if (runMode == core::Storage::MODE_ZIGBEE) {
        // Pause MQTT & WiFi until commissioning window ends or user switches back
        if (WiFi.isConnected()) WiFi.disconnect(true, true);
        WiFi.mode(WIFI_OFF);
        wifiOff = true;
        // Stop Zigbee if not started via joined/commissioning, then (re)start joined if we were bound
        #if ZB_ENABLED
        if (!zbStarted && zbEverJoined) {
          zb_start_joined();
        }
        #endif
      } else {
        // Switch to WiFi/MQTT immediately
        wifiOff = false;
        WiFi.mode(WIFI_STA);
      #if !defined(USE_JC3248W535)
      wifiMgr.ensureSta();
        ensureMqtt();
      #endif
      }
    };
    h.onSettings = [](){ g_settingsRequested = true; };
    h.onWifiReset = [](){ storage.setWifiSsid(""); storage.setWifiPass(""); g_startPortalRequested = true; };
    h.onWifiSave = [](const char *s, const char *p){
      ESP_LOGI("UI", "Saving WiFi: SSID='%s', Pass='%s'", s ? s : "", p ? p : "");
      storage.setWifiSsid(s?s:""); 
      storage.setWifiPass(p?p:"");
    };
    h.onMqttSave = [](const char *host, uint16_t port, const char *user, const char *pass){
      ESP_LOGI("UI", "Saving MQTT: Host='%s', Port=%u, User='%s', Pass='%s'", host ? host : "", port, user ? user : "", pass ? pass : "");
      storage.setMqttHost(host?host:"");
      storage.setMqttPort(port);
      storage.setMqttUser(user?user:"");
      storage.setMqttPass(pass?pass:"");
      MQTT_HOST = storage.getMqttHost(MQTT_HOST);
      MQTT_PORT = storage.getMqttPort(MQTT_PORT);
      MQTT_USER = storage.getMqttUser(MQTT_USER);
      MQTT_PASS = storage.getMqttPass(MQTT_PASS);
    };
    h.onSaveSettings = [](){ 
      // Optional: additional saves or restart logic
      requestShowMain();
    };
    h.onCancelSettings = [](){ 
      requestShowMain();
    };
    ui::configureHandlers(h);
    ui::setThresholds(PH_MIN, PH_MAX, ORP_MIN, ORP_MAX);
    ui::setInitialSpeeds(M1_SPEED_PC, M2_SPEED_PC);

    // Theme already set earlier under lock for S3; C6 keeps default path

    // Input device (touch) bridge (enabled with safe polling read_cb)
    #if !defined(BOARD_ESP32S3_35)
    if (true) {
      static lv_indev_drv_t indev_drv;
      lv_indev_drv_init(&indev_drv);
      // Tune gesture/scroll parameters at runtime to avoid macro redefinition warnings
      indev_drv.scroll_limit = 4;            // match UI expectation for drag start
      indev_drv.scroll_throw = 8;            // moderate deceleration
      indev_drv.long_press_time = 400;       // ms
      indev_drv.long_press_repeat_time = 0;  // ms (disable repeat to avoid double key inserts)
      indev_drv.gesture_limit = 20;          // pixels
      indev_drv.gesture_min_velocity = 2;    // pixels per tick
      indev_drv.type = LV_INDEV_TYPE_POINTER;
      indev_drv.read_cb = [](lv_indev_drv_t *d, lv_indev_data_t *data)->void{
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
        
        // Clamp to LVGL display resolution (avoid Arduino_GFX rotation mismatch)
        lv_disp_t *disp_local = lv_disp_get_default();
        int hor = disp_local ? (int)lv_disp_get_hor_res(disp_local) : 320;
        int ver = disp_local ? (int)lv_disp_get_ver_res(disp_local) : 172;
        x = constrain(x, 0, hor - 1);
        y = constrain(y, 0, ver - 1);
        
        data->point.x = x; 
        data->point.y = y; 
        data->state = LV_INDEV_STATE_PRESSED;
        
        static uint32_t last_press_ms = 0;
        const uint32_t DEBOUNCE_MS = 400;
        uint32_t now = millis();
        if (now - last_press_ms < DEBOUNCE_MS) {
          data->state = LV_INDEV_STATE_RELEASED;
          return;
        }
        last_press_ms = now;
        
        static uint32_t last_read_ms = 0;
        if (now - last_read_ms < 50) return; // Throttle reads to 20Hz max to avoid overload
        last_read_ms = now;
        
        static uint32_t last_print = 0;
        if (millis() - last_print > 100) { // Rate limit printing
            last_print = millis();
            // Avoid heavy logging of touch events; it stalls UI if no CDC consumer
            //ESP_LOGI("TOUCH", "LVGL: PRESSED at x=%d, y=%d, points=%d", x, y, n);
        }
      };
      (void)lv_indev_drv_register(&indev_drv);
    }
    #endif

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
      lv_obj_set_scroll_dir(lv_tile_settings, LV_DIR_VER);
      lv_obj_set_scrollbar_mode(lv_tile_settings, LV_SCROLLBAR_MODE_AUTO);
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

      // Footer IP at bottom-right
      lv_lbl_ip = lv_label_create(lv_tile_main); lv_obj_set_style_text_color(lv_lbl_ip, lv_palette_darken(LV_PALETTE_GREY, 4), 0); lv_obj_set_style_text_font(lv_lbl_ip, &lv_font_montserrat_14, 0); lv_label_set_long_mode(lv_lbl_ip, LV_LABEL_LONG_CLIP); lv_obj_set_width(lv_lbl_ip, LV_SIZE_CONTENT); lv_obj_set_style_text_align(lv_lbl_ip, LV_TEXT_ALIGN_RIGHT, 0); lv_obj_align(lv_lbl_ip, LV_ALIGN_BOTTOM_RIGHT, -14, -1); lv_label_set_text(lv_lbl_ip, "IP: --");

      // Bottom-left temperature label
      lv_lbl_temp = lv_label_create(lv_tile_main);
      lv_obj_set_style_text_font(lv_lbl_temp, &lv_font_montserrat_14, 0);
      lv_obj_set_style_text_color(lv_lbl_temp, lv_color_black(), 0);
      lv_label_set_text(lv_lbl_temp, "--.- C");
      lv_obj_align(lv_lbl_temp, LV_ALIGN_BOTTOM_LEFT, 18, -1);

      // Prepare link icon on root screen; start hidden, will be controlled by mode
      if (!lv_img_link) {
        lv_img_link = lv_img_create(lv_scr_act());
        lv_img_set_src(lv_img_link, &link_off_16dp_999999_FILL0_wght400_GRAD0_opsz20);
        lv_obj_align(lv_img_link, LV_ALIGN_BOTTOM_RIGHT, -14, -1);
        lv_obj_set_style_img_recolor_opa(lv_img_link, LV_OPA_COVER, 0);
        lv_obj_set_style_img_recolor(lv_img_link, lv_palette_darken(LV_PALETTE_GREY, 3), 0);
        lv_obj_set_style_img_opa(lv_img_link, LV_OPA_COVER, 0);
        lv_obj_add_flag(lv_img_link, LV_OBJ_FLAG_HIDDEN);
      }

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
      lv_obj_add_flag(lv_img_pump_ph_shadow, LV_OBJ_FLAG_HIDDEN);

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
      lv_obj_add_flag(lv_img_pump_orp_shadow, LV_OBJ_FLAG_HIDDEN);

      ui::updateValues();

      // Settings tile content: simple vertical layout (container to isolate coords)
      lv_obj_t *settings = lv_obj_create(lv_tile_settings);
      lv_obj_remove_style_all(settings);
      lv_obj_set_width(settings, lv_obj_get_width(lv_tile_settings)-8);
      lv_obj_set_height(settings, LV_SIZE_CONTENT);
      lv_obj_align(settings, LV_ALIGN_TOP_MID, 0, 6);
      lv_obj_set_style_bg_opa(settings, LV_OPA_TRANSP, 0);
      lv_obj_set_flex_flow(settings, LV_FLEX_FLOW_COLUMN);
      lv_obj_set_style_pad_row(settings, 14, 0);
      lv_obj_set_style_pad_column(settings, 10, 0);

      // Section: General
      lv_obj_t *sec_general = lv_obj_create(settings);
      lv_obj_remove_style_all(sec_general);
      lv_obj_set_width(sec_general, LV_PCT(100));
      lv_obj_set_height(sec_general, LV_SIZE_CONTENT);
      lv_obj_set_style_bg_color(sec_general, lv_palette_lighten(LV_PALETTE_GREY,2), 0);
      lv_obj_set_style_bg_opa(sec_general, LV_OPA_20, 0);
      lv_obj_set_style_pad_all(sec_general, 6, 0);
      lv_obj_set_style_pad_row(sec_general, 10, 0);
      lv_obj_set_flex_flow(sec_general, LV_FLEX_FLOW_COLUMN);
      // Title
      lv_obj_t *title_general = lv_label_create(sec_general); lv_obj_set_style_text_color(title_general, lv_color_black(), 0); lv_label_set_text(title_general, "General");
      // Row: mode
      lv_obj_t *row_mode = lv_obj_create(sec_general); lv_obj_remove_style_all(row_mode); lv_obj_set_width(row_mode, LV_PCT(100)); lv_obj_set_height(row_mode, LV_SIZE_CONTENT); lv_obj_set_flex_flow(row_mode, LV_FLEX_FLOW_ROW); lv_obj_set_style_pad_column(row_mode, 12, 0);
      #if HAS_ZIGBEE
      lv_obj_t *lblMode = lv_label_create(row_mode); lv_obj_set_style_text_color(lblMode, lv_color_black(), 0); lv_label_set_text(lblMode, "Zigbee mode"); lv_obj_set_flex_grow(lblMode, 1);
      lv_obj_t *swMode = lv_switch_create(row_mode); lv_obj_set_size(swMode, 50, 24); if (runMode == core::Storage::MODE_ZIGBEE) lv_obj_add_state(swMode, LV_STATE_CHECKED); else lv_obj_clear_state(swMode, LV_STATE_CHECKED);
      lv_obj_add_event_cb(swMode, [](lv_event_t *e){
        if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
        bool zig = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
        runMode = zig ? core::Storage::MODE_ZIGBEE : core::Storage::MODE_WIFI_MQTT;
        storage.setMode(runMode);
        if (runMode == core::Storage::MODE_ZIGBEE) {
          // Switch radios: stop WiFi and start Zigbee immediately if already joined
          if (WiFi.isConnected()) WiFi.disconnect(true, true);
          WiFi.mode(WIFI_OFF);
          wifiOff = true;
          #if ZB_ENABLED
          if (!zbStarted) {
            if (zbEverJoined) zb_start_joined();
            else {
              // If never joined, prompt the user to press BOOT for commissioning
              ui::showHoldToPair();
            }
          }
          #endif
        } else {
          // Switch to WiFi/MQTT immediately
          wifiOff = false;
          // Load latest creds from NVS
          WIFI_SSID = storage.getWifiSsid(WIFI_SSID);
          WIFI_PASSWORD = storage.getWifiPass(WIFI_PASSWORD);
          // If Zigbee stack is running, perform a quick reboot to release radio cleanly
          #if ZB_ENABLED
          if (zbStarted) {
            ESP_LOGI("WiFi", "Switching from Zigbee->WiFi: scheduling reboot for clean radio handover");
            delay(100);
            ESP.restart();
          }
          #endif
          if (WIFI_SSID.length() == 0) {
            // No creds → start captive portal
            if (!portal.isActive()) { portal.setStorage(&storage); portal.beginAP("PoolLab-Setup"); }
          } else {
            // Ensure portal is stopped and bring up STA now
            if (portal.isActive()) portal.stop();
            WiFi.mode(WIFI_STA);
            wifiMgr.ensureSta();
            ensureMqtt();
          }
        }
      }, LV_EVENT_ALL, NULL);
      #endif
      // Row: Pair button (right)
      lv_obj_t *row_pair = lv_obj_create(sec_general); lv_obj_remove_style_all(row_pair); lv_obj_set_width(row_pair, LV_PCT(100)); lv_obj_set_height(row_pair, LV_SIZE_CONTENT); lv_obj_set_flex_flow(row_pair, LV_FLEX_FLOW_ROW); lv_obj_set_style_pad_column(row_pair, 12, 0);
      lv_obj_t *spacer = lv_obj_create(row_pair); lv_obj_remove_style_all(spacer); lv_obj_set_width(spacer, LV_PCT(100)); lv_obj_set_height(spacer, 1); lv_obj_set_flex_grow(spacer, 1);
      #if HAS_ZIGBEE
      lv_obj_t *btnPair = lv_btn_create(row_pair); lv_obj_set_size(btnPair, 120, 30);
      // Style + label based on bound state
      if (zbEverJoined) { lv_obj_set_style_bg_color(btnPair, lv_palette_main(LV_PALETTE_RED), 0); lv_label_set_text(lv_label_create(btnPair), "UNPAIR"); }
      else { lv_label_set_text(lv_label_create(btnPair), "PAIR"); }
      lv_obj_add_event_cb(btnPair, [](lv_event_t *e){
        if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
        #if ZB_ENABLED
        if (zbEverJoined) {
          // Perform a Zigbee factory reset (will reboot)
          ESP_LOGI("ZB", "Unpair requested -> factory reset Zigbee");
          Zigbee.factoryReset(true);
        } else {
          ui::showHoldToPair();
          ESP_LOGI("ZB", "Manual commissioning (60s)");
          zigbee.startCommissioning(60);
        }
        #else
        (void)e;
        #endif
      }, LV_EVENT_ALL, NULL);
      #endif

      // Section: Pumps
      lv_obj_t *sec_pumps = lv_obj_create(settings);
      lv_obj_remove_style_all(sec_pumps);
      lv_obj_set_width(sec_pumps, LV_PCT(100));
      lv_obj_set_height(sec_pumps, LV_SIZE_CONTENT);
      lv_obj_set_style_bg_color(sec_pumps, lv_palette_lighten(LV_PALETTE_GREY,2), 0);
      lv_obj_set_style_bg_opa(sec_pumps, LV_OPA_20, 0);
      lv_obj_set_style_pad_all(sec_pumps, 8, 0);
      lv_obj_set_style_pad_row(sec_pumps, 12, 0);
      lv_obj_set_flex_flow(sec_pumps, LV_FLEX_FLOW_COLUMN);
      lv_obj_t *title_pumps = lv_label_create(sec_pumps); lv_obj_set_style_text_color(title_pumps, lv_color_black(), 0); lv_label_set_text(title_pumps, "Pumps");
      // Row: pH
      lv_obj_t *row_ph = lv_obj_create(sec_pumps); lv_obj_remove_style_all(row_ph); lv_obj_set_width(row_ph, LV_PCT(100)); lv_obj_set_height(row_ph, 36); lv_obj_set_flex_flow(row_ph, LV_FLEX_FLOW_ROW); lv_obj_set_style_pad_column(row_ph, 12, 0); lv_obj_set_style_pad_ver(row_ph, 6, 0);
      lv_obj_t *lbl1 = lv_label_create(row_ph); lv_obj_set_style_text_color(lbl1, lv_color_black(), 0); lv_label_set_text(lbl1, "pH Speed");
      lv_sl_speed1 = lv_slider_create(row_ph); lv_obj_set_height(lv_sl_speed1, 12); lv_obj_set_flex_grow(lv_sl_speed1, 1); lv_obj_set_style_width(lv_sl_speed1, 14, LV_PART_KNOB); lv_obj_set_style_height(lv_sl_speed1, 14, LV_PART_KNOB); lv_slider_set_range(lv_sl_speed1, 0, 100); lv_slider_set_value(lv_sl_speed1, M1_SPEED_PC, LV_ANIM_OFF);
      lv_lbl_speed1 = lv_label_create(row_ph); lv_obj_set_style_text_color(lv_lbl_speed1, lv_color_black(), 0); lv_label_set_text_fmt(lv_lbl_speed1, "%u%%", (unsigned)M1_SPEED_PC);
      lv_obj_t *btn1m = lv_btn_create(row_ph); lv_obj_set_size(btn1m, 28, 24); { lv_obj_t *t = lv_label_create(btn1m); lv_label_set_text(t, "-"); lv_obj_center(t);} lv_obj_add_event_cb(btn1m, on_ph_minus_cb, LV_EVENT_CLICKED, NULL);
      lv_obj_t *btn1p = lv_btn_create(row_ph); lv_obj_set_size(btn1p, 28, 24); { lv_obj_t *t = lv_label_create(btn1p); lv_label_set_text(t, "+"); lv_obj_center(t);} lv_obj_add_event_cb(btn1p, on_ph_plus_cb, LV_EVENT_CLICKED, NULL);
      lv_obj_add_event_cb(lv_sl_speed1, [](lv_event_t *e){ if (lv_event_get_code(e)!=LV_EVENT_VALUE_CHANGED) return; int v = lv_slider_get_value((lv_obj_t*)lv_event_get_target(e)); v = constrain(v,0,100); M1_SPEED_PC = (uint8_t)v; storage.setM1Speed(M1_SPEED_PC); if (lv_lbl_speed1) lv_label_set_text_fmt(lv_lbl_speed1, "%u%%", (unsigned)M1_SPEED_PC); }, LV_EVENT_ALL, NULL);
      // Row: ORP
      lv_obj_t *row_orp = lv_obj_create(sec_pumps); lv_obj_remove_style_all(row_orp); lv_obj_set_width(row_orp, LV_PCT(100)); lv_obj_set_height(row_orp, 36); lv_obj_set_flex_flow(row_orp, LV_FLEX_FLOW_ROW); lv_obj_set_style_pad_column(row_orp, 12, 0); lv_obj_set_style_pad_ver(row_orp, 6, 0);
      lv_obj_t *lbl2 = lv_label_create(row_orp); lv_obj_set_style_text_color(lbl2, lv_color_black(), 0); lv_label_set_text(lbl2, "ORP Speed");
      lv_sl_speed2 = lv_slider_create(row_orp); lv_obj_set_height(lv_sl_speed2, 12); lv_obj_set_flex_grow(lv_sl_speed2, 1); lv_obj_set_style_width(lv_sl_speed2, 14, LV_PART_KNOB); lv_obj_set_style_height(lv_sl_speed2, 14, LV_PART_KNOB); lv_slider_set_range(lv_sl_speed2, 0, 100); lv_slider_set_value(lv_sl_speed2, M2_SPEED_PC, LV_ANIM_OFF);
      lv_lbl_speed2 = lv_label_create(row_orp); lv_obj_set_style_text_color(lv_lbl_speed2, lv_color_black(), 0); lv_label_set_text_fmt(lv_lbl_speed2, "%u%%", (unsigned)M2_SPEED_PC);
      lv_obj_t *btn2m = lv_btn_create(row_orp); lv_obj_set_size(btn2m, 28, 24); { lv_obj_t *t = lv_label_create(btn2m); lv_label_set_text(t, "-"); lv_obj_center(t);} lv_obj_add_event_cb(btn2m, on_orp_minus_cb, LV_EVENT_CLICKED, NULL);
      lv_obj_t *btn2p = lv_btn_create(row_orp); lv_obj_set_size(btn2p, 28, 24); { lv_obj_t *t = lv_label_create(btn2p); lv_label_set_text(t, "+"); lv_obj_center(t);} lv_obj_add_event_cb(btn2p, on_orp_plus_cb, LV_EVENT_CLICKED, NULL);
      lv_obj_add_event_cb(lv_sl_speed2, [](lv_event_t *e){ if (lv_event_get_code(e)!=LV_EVENT_VALUE_CHANGED) return; int v = lv_slider_get_value((lv_obj_t*)lv_event_get_target(e)); v = constrain(v,0,100); M2_SPEED_PC = (uint8_t)v; storage.setM2Speed(M2_SPEED_PC); if (lv_lbl_speed2) lv_label_set_text_fmt(lv_lbl_speed2, "%u%%", (unsigned)M2_SPEED_PC); }, LV_EVENT_ALL, NULL);

      // Row: ALL OFF emergency button
      lv_obj_t *row_off = lv_obj_create(sec_pumps); lv_obj_remove_style_all(row_off); lv_obj_set_width(row_off, LV_PCT(100)); lv_obj_set_height(row_off, LV_SIZE_CONTENT); lv_obj_set_flex_flow(row_off, LV_FLEX_FLOW_ROW);
      lv_obj_t *sp = lv_obj_create(row_off); lv_obj_remove_style_all(sp); lv_obj_set_width(sp, LV_PCT(100)); lv_obj_set_height(sp, 1); lv_obj_set_flex_grow(sp, 1);
      lv_obj_t *btnAllOff = lv_btn_create(row_off); lv_obj_set_size(btnAllOff, 110, 28); lv_obj_set_style_bg_color(btnAllOff, lv_palette_main(LV_PALETTE_RED), 0); lv_obj_set_style_bg_opa(btnAllOff, LV_OPA_COVER, 0); lv_label_set_text(lv_label_create(btnAllOff), "ALL OFF"); lv_obj_add_event_cb(btnAllOff, on_all_off_cb, LV_EVENT_CLICKED, NULL);

      // Save button (place further down so content can scroll)
      //lv_obj_t *btnSave = lv_btn_create(lv_tile_settings); lv_obj_set_size(btnSave, 100, 34); lv_obj_align(btnSave, LV_ALIGN_TOP_MID, -56, 170); lv_obj_add_event_cb(btnSave, on_speed_save_cb, LV_EVENT_CLICKED, NULL); lv_label_set_text(lv_label_create(btnSave), "Save");
      // Pair Zigbee button (legacy, removed)

      // Section: Network
      lv_obj_t *sec_net = lv_obj_create(settings);
      lv_obj_remove_style_all(sec_net);
      lv_obj_set_width(sec_net, LV_PCT(100));
      lv_obj_set_height(sec_net, LV_SIZE_CONTENT);
      lv_obj_set_style_bg_color(sec_net, lv_palette_lighten(LV_PALETTE_GREY,2), 0);
      lv_obj_set_style_bg_opa(sec_net, LV_OPA_20, 0);
      lv_obj_set_style_pad_all(sec_net, 8, 0);
      lv_obj_set_style_pad_row(sec_net, 12, 0);
      lv_obj_set_flex_flow(sec_net, LV_FLEX_FLOW_COLUMN);
      lv_obj_t *title_net = lv_label_create(sec_net); lv_obj_set_style_text_color(title_net, lv_color_black(), 0); lv_label_set_text(title_net, "Network");
      lv_obj_t *row_net = lv_obj_create(sec_net); lv_obj_remove_style_all(row_net); lv_obj_set_width(row_net, LV_PCT(100)); lv_obj_set_height(row_net, LV_SIZE_CONTENT); lv_obj_set_flex_flow(row_net, LV_FLEX_FLOW_ROW); lv_obj_set_style_pad_column(row_net, 12, 0);
      lv_obj_t *lblNet = lv_label_create(row_net); lv_obj_set_style_text_color(lblNet, lv_color_black(), 0); lv_label_set_text(lblNet, "WiFi setup portal"); lv_obj_set_flex_grow(lblNet, 1);
      lv_obj_t *btnCfgWifi = lv_btn_create(row_net); lv_obj_set_size(btnCfgWifi, 120, 28); lv_label_set_text(lv_label_create(btnCfgWifi), "Configure WiFi");
      lv_obj_add_event_cb(btnCfgWifi, [](lv_event_t *e){ (void)e; portal.setStorage(&storage); portal.beginAP("PoolLab-Setup"); }, LV_EVENT_CLICKED, NULL);
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
    // Use modern UI module instead of legacy builder
    #if !(defined(BOARD_ESP32S3_35) && defined(USE_JC3248W535))
    ui::build(false);
    ui::updateValues();
    #endif
    ESP_LOGI("UI", "After layout timer");
    #if defined(BOARD_ESP32S3_35)
    LVGL_UNLOCK();
    #endif
  }

  // Begin touch AFTER LVGL init (never on S3 JC path; BSP handles it)
  #if !(defined(BOARD_ESP32S3_35) && defined(USE_JC3248W535))
  touchDriver.begin();
  #endif
  // If touch is noisy at boot it can stall UI. Add a short debounce warmup.
  delay(50);

  // Init buttons (BOOT) with pull-up and debounce state
  // Initialize debounced buttons helper
  g_buttons.begin(io::ButtonPins{ BTN_PIN1, BTN_PIN2 });

  // Init Zigbee client only on platforms that support it and when enabled
  #if ZB_ENABLED && !(defined(BOARD_ESP32S3_35) && defined(USE_JC3248W535))
  io::ZigbeeConfig zcfg{};
  zigbee.begin(zcfg);
  zbPrefs.begin(ZB_PREF_NS, true);
  bool doPair = zbPrefs.getBool(ZB_PREF_PAIR, false);
  zbEverJoined = zbPrefs.getBool(ZB_PREF_BOUND, false);
  zbPrefs.end();
  ESP_LOGI("ZB", "Commissioning flag: %d", doPair ? 1 : 0);
  ESP_LOGI("ZB", "Zigbee bound flag: %d", zbEverJoined ? 1 : 0);
  if (doPair) {
    // Clear flag and start pairing flow on clean boot
    zbPrefs.begin(ZB_PREF_NS, false);
    zbPrefs.putBool(ZB_PREF_PAIR, false);
    zbPrefs.end();
    // Schakel WiFi uit vóór de Zigbee stack start om radio-contentie te voorkomen
    if (WiFi.isConnected()) WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    wifiOff = true;
    ESP_LOGI("ZB", "WiFi disabled for commissioning");
    ESP_LOGI("ZB", "Starting commissioning flow (steering, 120s)");
    zb_start_and_commission(120);
  }
  #endif

  if (!DIAG_MODE) {
    // UARTs (RX only) unless TX pins are provided
    TUYA_A.begin(TUYA_BAUD, SERIAL_8N1, RX_A_PIN, TX_A_PIN);
    if (USE_CHANNEL_B) TUYA_B.begin(TUYA_BAUD, SERIAL_8N1, RX_B_PIN, TX_B_PIN);
  }

  // Configure Tuya DP ids for new parser module
  io::tuyaConfigure(DP_TEMP, DP_ORP, DP_PH, DP_ORP_ALT1, DP_PH_ALT1);

  #if defined(BOARD_ESP32C6_TOUCH_1_47) && !defined(USE_JC3248W535)
  if (!USE_LVGL_UI) {
    pushLine("Ready. Waiting for frames...");
  drawStaticUI();
  updateValueAreas();
  }
  #endif
  // Load persisted configuration early when not using LVGL UI (so boot mode is honored)
  if (!USE_LVGL_UI) {
    storage.begin(false);
    PH_MIN = storage.getPhMin(PH_MIN);
    PH_MAX = storage.getPhMax(PH_MAX);
    ORP_MIN = storage.getOrpMin(ORP_MIN);
    ORP_MAX = storage.getOrpMax(ORP_MAX);
    M1_SPEED_PC = (uint8_t)storage.getM1Speed(M1_SPEED_PC);
    M2_SPEED_PC = (uint8_t)storage.getM2Speed(M2_SPEED_PC);
    runMode = storage.getMode(core::Storage::MODE_ZIGBEE);
    savedMode = runMode;
  }

  // Respect saved mode; do not force Zigbee on C6

  // WiFi + MQTT
  // Start or stop WiFi based on saved mode at boot (respect prior forced-off, e.g. commissioning)
  if (!wifiOff) {
    if (runMode == core::Storage::MODE_WIFI_MQTT) {
      // Load persisted WiFi creds first so we can decide between STA vs captive portal
      WIFI_SSID = storage.getWifiSsid(WIFI_SSID);
      WIFI_PASSWORD = storage.getWifiPass(WIFI_PASSWORD);
      if (WIFI_SSID.length() == 0) {
        wifiOff = false;
        ESP_LOGI("WiFi", "Boot: starting captive portal (no SSID)");
        portal.setStorage(&storage);
        portal.beginAP("PoolLab-Setup");
        if (USE_LVGL_UI) {
          #if defined(BOARD_ESP32S3_35)
          if (LVGL_LOCK()) { ui::setIp(WiFi.softAPIP().toString().c_str()); LVGL_UNLOCK(); }
          #else
          ui::setIp(WiFi.softAPIP().toString().c_str());
          #endif
        }
      } else {
        wifiOff = false;
        ESP_LOGI("WiFi", "Boot: WiFi STA starting");
        {
          String ssid = storage.getWifiSsid("");
          String pass = storage.getWifiPass("");
          wifiMgr.begin(ssid, pass, "poollab", [](const String &ip){ if (USE_LVGL_UI) { g_ui_ip_text = ip; g_ui_ip_dirty = true; } });
          if (USE_LVGL_UI) {
            #if defined(BOARD_ESP32S3_35)
            if (LVGL_LOCK()) { ui::setSsid(ssid.c_str()); LVGL_UNLOCK(); }
            #else
            ui::setSsid(ssid.c_str());
            #endif
          }
          // Ensure WebUI started on both boards identiek
          webui.setStorage(&storage);
          webui.setRefs(&PH_MIN, &PH_MAX, &ORP_MIN, &ORP_MAX, &M1_SPEED_PC, &M2_SPEED_PC);
          if (!webui.isActive()) webui.begin();
        }
        ensureMqtt();
      }
    } else {
      // Zigbee mode -> WiFi off (C6 behavior); S3 will show UI but WiFi remains off
      WiFi.mode(WIFI_OFF);
      wifiOff = true;
      ESP_LOGI("WiFi", "Boot: Zigbee mode -> WiFi OFF");
      // If we were previously joined, start Zigbee stack immediately
      #if ZB_ENABLED
      if (zbEverJoined) {
        zb_start_joined();
      }
      #endif
    }
  }

  if (USE_LVGL_UI) ui::setInitialMode(runMode == core::Storage::MODE_ZIGBEE);

  // Load custom speeds if present
  M1_SPEED_PC = (uint8_t)storage.getM1Speed(M1_SPEED_PC);
  M2_SPEED_PC = (uint8_t)storage.getM2Speed(M2_SPEED_PC);
  if (USE_LVGL_UI) ui::setInitialSpeeds(M1_SPEED_PC, M2_SPEED_PC);

  // TB6612 pins
  if (MOTOR_ENABLE) {
    g_motor.begin(io::MotorPins{TB_STBY, M1_IN1, M1_IN2, M1_PWM, M2_IN1, M2_IN2, M2_PWM}, PWM_FREQ, PWM_BITS);

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
    #if !defined(USE_JC3248W535)
    pushLine("TX: query product info"); drawScreen();
    io::tuyaSendQueryProductInfo(TUYA_A);
    delay(200);
    pushLine("TX: set wifi status 0x00"); drawScreen();
    io::tuyaSendSetWifiStatus(TUYA_A, 0x00);
    delay(200);
    pushLine("TX: DP query"); drawScreen();
    io::tuyaSendDpQuery(TUYA_A);
    #endif
  }
}

void loop() {
  if (USE_LVGL_UI) {
    #if !defined(BOARD_ESP32S3_35)
    lv_timer_handler();
    #endif
    // Let LVGL task run; then light yield
    delay(0);
    // Apply deferred UI IP update from WiFi callback without crossing threads
    if (g_ui_ip_dirty) {
      g_ui_ip_dirty = false;
      #if defined(BOARD_ESP32S3_35)
      if (LVGL_LOCK()) { ui::setIp(g_ui_ip_text.c_str()); LVGL_UNLOCK(); }
      #else
      ui::setIp(g_ui_ip_text.c_str());
      #endif
    }
    updateLvglValues();
    if (!MOTOR_ENABLE) {
      bool phActive = METRICS().havePh && (METRICS().phVal < PH_MIN || METRICS().phVal > PH_MAX);
      bool orpActive = METRICS().haveOrp && ((int)lrintf(METRICS().orpMv) < ORP_MIN || (int)lrintf(METRICS().orpMv) > ORP_MAX);
      // Only toggle legacy main-UI icons on non-S3 routes; S3/JC uses module UI via ui::setPumpActive
      #if !defined(BOARD_ESP32S3_35)
      if (lv_img_pump_ph && lv_img_pump_ph_shadow) {
        if (phActive) { lv_obj_clear_flag(lv_img_pump_ph, LV_OBJ_FLAG_HIDDEN); lv_obj_clear_flag(lv_img_pump_ph_shadow, LV_OBJ_FLAG_HIDDEN); }
        else { lv_obj_add_flag(lv_img_pump_ph, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(lv_img_pump_ph_shadow, LV_OBJ_FLAG_HIDDEN); }
      }
      if (lv_img_pump_orp && lv_img_pump_orp_shadow) {
        if (orpActive) { lv_obj_clear_flag(lv_img_pump_orp, LV_OBJ_FLAG_HIDDEN); lv_obj_clear_flag(lv_img_pump_orp_shadow, LV_OBJ_FLAG_HIDDEN); }
        else { lv_obj_add_flag(lv_img_pump_orp, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(lv_img_pump_orp_shadow, LV_OBJ_FLAG_HIDDEN); }
      }
      #endif
      // Ensure module UI icons reflect threshold state too
      ui::setPumpActive(phActive, orpActive);
    }
  } else if (DIAG_MODE) {
    static uint32_t last = 0;
    uint32_t now = millis();
    if (now - last > 1000) {
      last = now;
      ESP_LOGI("DIAG", "millis=%u", (unsigned)now);
      // visual heartbeat on screen border
      static bool toggle = false; toggle = !toggle;
      #if !defined(USE_JC3248W535)
      #if !defined(USE_JC3248W535)
      uint16_t c = toggle ? YELLOW : CYAN;
      gfx->drawRect(0, 0, 171, 319, c);
      #endif
      #endif
    }
    return;
  }
  // Apply deferred navigation back to main UI (one-shot)
  if (g_showMainRequested) {
    g_showMainRequested = false;
    #if defined(BOARD_ESP32S3_35)
    // Defer to LVGL context to avoid cross-thread UI calls
    lv_timer_t *t = lv_timer_create([](lv_timer_t *tm){ (void)tm; ui::showMain(); }, 0, NULL);
    lv_timer_set_repeat_count(t, 1);
    #else
    ui::showMain();
    #endif
  }
  // Removed alive ticker

  // --- Button long-press detection for Zigbee commissioning ---
  if (g_buttons.pollLongPress(3000)) {
      ESP_LOGI("ZB", "Long press: start commissioning now (120s)");
    if (USE_LVGL_UI) ui::showHoldToPair();
    #if ZB_ENABLED
      savedMode = runMode;
      modeForced = true;
      runMode = core::Storage::MODE_ZIGBEE; storage.setMode(runMode);
      if (WiFi.isConnected()) WiFi.disconnect(true, true);
      WiFi.mode(WIFI_OFF);
      wifiOff = true;
      zb_start_and_commission(120);
#endif
    }

  // If commissioning finished, optionally restore WiFi
  #if ZB_ENABLED
  if (wifiOff) {
#if ZB_ENABLED
  if (zbStarted && (zbCommissionUntilMs && millis() > zbCommissionUntilMs)) {
    ESP_LOGI("ZB", "Commissioning window ended");
    if (modeForced) {
      // Restore user's previous mode selection
      runMode = savedMode;
      storage.setMode(runMode);
      modeForced = false;
    }
    if (runMode == core::Storage::MODE_WIFI_MQTT) {
      ESP_LOGI("WiFi", "Restoring WiFi STA (WiFi/MQTT mode)");
      WiFi.mode(WIFI_STA);
      wifiOff = false;
      wifiMgr.ensureSta();
      ensureMqtt();
    } else {
      // Remain in Zigbee-only mode; keep WiFi fully off
      WiFi.mode(WIFI_OFF);
      wifiOff = true;
      ESP_LOGI("WiFi", "Remain OFF (Zigbee mode)");
    }
  }
#endif
  }
  #endif

  // Defer settings screen outside LVGL event context (lock-protected)
  if (g_settingsRequested) {
    if (USE_LVGL_UI) {
      bool opened = false;
      #if defined(BOARD_ESP32S3_35)
      if (LVGL_LOCK()) {
        // Defer UI changes to a zero-delay LVGL timer to avoid event/reentrancy issues
        lv_timer_t *t = lv_timer_create([](lv_timer_t *tm){ (void)tm; ui::showSettings(); }, 0, NULL);
        lv_timer_set_repeat_count(t, 1);
        // Populate fields shortly after the UI is created to ensure textareas exist
        lv_timer_t *t2 = lv_timer_create([](lv_timer_t *tm){ (void)tm; ui::setSavedWifi(storage.getWifiSsid("").c_str(), storage.getWifiPass("").c_str()); ui::setSavedMqtt(storage.getMqttHost("").c_str(), storage.getMqttPort(1883), storage.getMqttUser("").c_str(), storage.getMqttPass("").c_str()); }, 20, NULL);
        lv_timer_set_repeat_count(t2, 1);
        LVGL_UNLOCK(); opened = true;
      }
      #else
      // Non-S3 path: still defer to next tick
      lv_timer_t *t = lv_timer_create([](lv_timer_t *tm){ (void)tm; ui::showSettings(); }, 0, NULL);
      lv_timer_set_repeat_count(t, 1);
      lv_timer_t *t2 = lv_timer_create([](lv_timer_t *tm){ (void)tm; ui::setSavedWifi(storage.getWifiSsid("").c_str(), storage.getWifiPass("").c_str()); ui::setSavedMqtt(storage.getMqttHost("").c_str(), storage.getMqttPort(1883), storage.getMqttUser("").c_str(), storage.getMqttPass("").c_str()); }, 20, NULL);
      lv_timer_set_repeat_count(t2, 1);
      opened = true;
      #endif
      if (opened) g_settingsRequested = false;
    } else {
      g_settingsRequested = false;
    }
  }

  {
    int processed = 0;
    #if !defined(USE_JC3248W535)
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
      if ((rawCountA % 16) == 0) { /* skip noisy raw */ }
      rawCountA++;
    }
    if (USE_LVGL_UI && ++processed > 256) break; // yield to UI
  }
  #endif
  }
  #if !defined(USE_JC3248W535)
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
      if ((rawCountB % 16) == 0) { /* skip noisy raw */ }
      rawCountB++;
    }
  }
  #endif

  // Avoid drawing directly with Arduino_GFX while LVGL UI is active

  // Dummy telemetry (optional) - enabled for all paths so baseline shows values
  if (DUMMY_MODE) {
    updateDummyTelemetry();
    if (USE_LVGL_UI) updateLvglValues();
  } else {
    // Ensure LVGL reflects real telemetry as it updates
    if (USE_LVGL_UI) updateLvglValues();
  }

  // Start captive portal on request (e.g., after WiFi reset)
  if (g_startPortalRequested) {
    g_startPortalRequested = false;
    // Clear WiFiManager credentials to avoid STA attempts with empty SSID
    wifiMgr.setCredentials("", "");
    portal.setStorage(&storage);
    if (!portal.isActive()) portal.beginAP("PoolLab-Setup");
    if (USE_LVGL_UI) ui::setIp(WiFi.softAPIP().toString().c_str());
  }

  // UI task now handles lv_timer_handler on S3
  // Push live updates to WebUI websockets periodically
  static uint32_t lastWebPush=0;
  uint32_t nowMs = millis();
  if (WiFi.status()==WL_CONNECTED && nowMs - lastWebPush > 1000) {
    lastWebPush = nowMs;
    if (webui.isActive()) webui.broadcastMetrics();
  }

  // WiFi/MQTT service loop
  static uint32_t lastConnectAttempt = 0;
  uint32_t now = millis();
  if (!wifiOff) {
    if (runMode == core::Storage::MODE_WIFI_MQTT) {
      wifiMgr.loop();
      if (WiFi.status() == WL_CONNECTED) {
        // Only attempt MQTT if a host is configured
        if (MQTT_HOST.length() > 0) {
        if (!mqttClient.isConnected() && now - lastConnectAttempt > 5000) {
          lastConnectAttempt = now;
          mqttClient.ensureConnected();
        }
        if (mqttClient.isConnected()) {
          mqttClient.publishDiscoveryOnce();
          mqttClient.publishStatesIfReady(domain::Metrics::instance());
          mqttClient.loop();
        }
      }
    }
  }
  }

  #if !defined(USE_JC3248W535)
  // When LVGL UI is active (C6 on master), touch events are fed into LVGL input driver above
  // Legacy touch UI removed; LVGL indev handles touch
  #endif

  // Motor control policy (skip if forced-on test is active)
  if (MOTOR_ENABLE) {
    domain::ControlConfig cfg{PH_MAX, PH_HYST, ORP_MIN, ORP_HYST, M1_SPEED_PC, M2_SPEED_PC};
    if (!emergencyStop) {
      g_motor.tick(cfg,
                      domain::Metrics::instance().havePh,
                      domain::Metrics::instance().phVal,
                      domain::Metrics::instance().haveOrp,
                      domain::Metrics::instance().orpMv,
                 (bool)(FORCE_MOTOR_A_ON && !emergencyStop));
      m1Running = g_motor.isM1Running();
      m2Running = g_motor.isM2Running();
    } else {
      g_motor.stopAll();
      m1Running = false; m2Running = false;
    }
    if (USE_LVGL_UI) {
      updateLvglValues();
      // Toggle pump icons visibility (both legacy main UI and module UI)
      if (lv_img_pump_ph && lv_img_pump_ph_shadow) {
        if (m1Running) { lv_obj_clear_flag(lv_img_pump_ph, LV_OBJ_FLAG_HIDDEN); lv_obj_clear_flag(lv_img_pump_ph_shadow, LV_OBJ_FLAG_HIDDEN); }
        else { lv_obj_add_flag(lv_img_pump_ph, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(lv_img_pump_ph_shadow, LV_OBJ_FLAG_HIDDEN); }
      }
      if (lv_img_pump_orp && lv_img_pump_orp_shadow) {
        if (m2Running) { lv_obj_clear_flag(lv_img_pump_orp, LV_OBJ_FLAG_HIDDEN); lv_obj_clear_flag(lv_img_pump_orp_shadow, LV_OBJ_FLAG_HIDDEN); }
        else { lv_obj_add_flag(lv_img_pump_orp, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(lv_img_pump_orp_shadow, LV_OBJ_FLAG_HIDDEN); }
      }
      // Module UI pump icons
      ui::setPumpActive(m1Running, m2Running);
    }
  }

  // OTA + captive portal services
  if (WiFi.status() == WL_CONNECTED) {
    ArduinoOTA.handle();
    if (webui.isActive()) webui.loop();
    delay(0);
  }
  if (portal.isActive()) {
    portal.loop();
    delay(0);
  }

  // Zigbee periodic reporting (Arduino Zigbee runs internally; no explicit loop needed)
  static uint32_t lastZbReport = 0;
  if (now - lastZbReport > 2000) {
    lastZbReport = now;
    #if ZB_ENABLED
    if (zbStarted) {
      // Apply deferred AO changes safely in app thread context
      if (zbPhMinPending) { zbPhMinPending = false; PH_MIN = zbPhMinValue; storage.setPhMin(PH_MIN); }
      if (zbPhMaxPending) { zbPhMaxPending = false; PH_MAX = zbPhMaxValue; storage.setPhMax(PH_MAX); }
      if (zbOrpMinPending) { zbOrpMinPending = false; ORP_MIN = (int)lrintf(zbOrpMinValue); storage.setOrpMin(ORP_MIN); }
      if (zbOrpMaxPending) { zbOrpMaxPending = false; ORP_MAX = (int)lrintf(zbOrpMaxValue); storage.setOrpMax(ORP_MAX); }
      if (domain::Metrics::instance().havePh)   zbPh.setFlow(domain::Metrics::instance().phVal);
      if (domain::Metrics::instance().haveOrp)  zbOrp.setPressure((int16_t)lrintf(domain::Metrics::instance().orpMv));
      if (domain::Metrics::instance().haveTemp) { zbTempSensor.setTemperature(domain::Metrics::instance().tempC); }
      // Opportunistic re-steering if not yet connected (only when LVGL UI is active)
      if (USE_LVGL_UI) {
      bool connected_now = Zigbee.connected();
      bool joined_now = zb_is_joined();
        #if defined(BOARD_ESP32S3_35)
        if (LVGL_LOCK()) {
        #endif
          if (lv_img_link) {
      if (connected_now && joined_now) {
        lv_img_set_src(lv_img_link, &link_16dp_999999_FILL0_wght400_GRAD0_opsz20);
      } else {
        lv_img_set_src(lv_img_link, &link_off_16dp_999999_FILL0_wght400_GRAD0_opsz20);
            }
          }
        #if defined(BOARD_ESP32S3_35)
          LVGL_UNLOCK();
        }
        #endif
      }
    }
    #endif
    // Auto-close commissioning modal when commissioning ends
    #if ZB_ENABLED
    if (USE_LVGL_UI && lv_zb_modal && zbCommissionUntilMs && millis() > zbCommissionUntilMs) {
      lv_obj_del(lv_zb_modal);
      lv_zb_modal = nullptr;
      zbCommissionUntilMs = 0;
    }
    #endif
  }


}

// Legacy pagination removed