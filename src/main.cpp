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
#if !defined(USE_JC3248W535) && !defined(BOARD_ESP32P4_43)
#include <Arduino_GFX_Library.h>
#endif
#include <HardwareSerial.h>
#include <SPI.h>
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
#include <SPIFFS.h>
#include <FS.h>
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
#if defined(BOARD_ESP32P4_43)
// P4: No native Zigbee, uses C6 bridge
#define ZB_ENABLED 0
#elif ((defined(FORCE_ZIGBEE) && FORCE_ZIGBEE) || (defined(HAS_ZIGBEE) && HAS_ZIGBEE)) && __has_include(<Zigbee.h>)
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
#if !defined(USE_JC3248W535) && !defined(BOARD_ESP32P4_43)
#include "core/DisplayBridge.h"
#endif
#include "domain/Metrics.h"
#include "domain/ControlPolicy.h"
// IO modules
#include "io/MqttClient.h"
#include "io/Touch.h"
#include "io/Tuya.h"
#include "io/AnalogPhOrpSensor.h"
#include "io/AdsPhOrpSensor.h"
#include "io/ZigbeeClient.h"
#include "io/CaptivePortal.h"
#include "ui/UI.h"
// Provide C-linkage declarations for image assets used by UI when included here
extern "C" {
  extern const lv_img_dsc_t water_ph_32dp_E3E3E3_FILL0_wght400_GRAD0_opsz40;
  extern const lv_img_dsc_t water_orp_32dp_E3E3E3_FILL0_wght400_GRAD0_opsz40;
}
#if USES_ARDUINO_GFX && !defined(USE_JC3248W535)
#endif
#include "io/WebUI.h"
#include "io/WiFiManager.h"
#include "boards/BoardSelect.h"
#include "io/Buttons.h"
#include "io/MotorController.h"
#include <HTTPClient.h>  // For WhatsApp CallMeBot notifications
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

// Helper macro to group boards that use Arduino_GFX
// Note: ESP32-P4 does NOT use Arduino_GFX - it uses native MIPI-DSI
#define USES_ARDUINO_GFX (defined(BOARD_ESP32C6_TOUCH_1_47))

#if defined(BOARD_ESP32C6_TOUCH_1_47)
static const int LCD_BL_PIN = 23;        // C6 original working backlight pin
#elif defined(BOARD_ESP32P4_43)
static const int LCD_BL_PIN = -1;        // P4: TBD, adjust based on actual board
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

// ===== Analog sensor (PH4502C/ORP) integration =====
// Enable to read pH and ORP from two ADC pins instead of Tuya UART
#ifndef USE_ANALOG_SENSORS
#define USE_ANALOG_SENSORS 0
#endif
#ifndef USE_ADS1115
#define USE_ADS1115 0
#endif
#if USE_ANALOG_SENSORS
  // Define ADC-capable GPIOs here (set to actual free ADC pins on your board)
  #ifndef PH_ADC_PIN
  #define PH_ADC_PIN -1
  #endif
  #ifndef ORP_ADC_PIN
  #define ORP_ADC_PIN -1
  #endif
  io::AnalogPhOrpSensor g_analog(PH_ADC_PIN, ORP_ADC_PIN);
#endif
#if USE_ADS1115
  #ifndef ADS_ADDR
  #define ADS_ADDR 0x48
  #endif
  #ifndef ADS_SDA
  #define ADS_SDA 18
  #endif
  #ifndef ADS_SCL
  #define ADS_SCL 19
  #endif
  #ifndef ADS_CH_PH
  #define ADS_CH_PH 0
  #endif
  #ifndef ADS_CH_ORP
  #define ADS_CH_ORP 1
  #endif
  io::AdsPhOrpSensor g_ads(ADS_ADDR, ADS_SDA, ADS_SCL, ADS_CH_PH, ADS_CH_ORP, io::AdsPhOrpSensor::GAIN_1, 8, 1000);
#endif
#if USES_ARDUINO_GFX
static const bool USE_LVGL_UI = true;  // C6 & P4 use LVGL UI
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
static lv_obj_t *lv_lbl_pump_ph_stats = nullptr;   // Session volume + flow rate label
static lv_obj_t *lv_lbl_pump_orp_stats = nullptr;
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
#if USE_ANALOG_SENSORS
static const bool DUMMY_MODE = false;  // real sensors active
#else
static const bool DUMMY_MODE = true;   // simulate values
#endif
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
static const bool MOTOR_ENABLE = true; // compile-time guard; runtime toggle via storage
static const bool MOTOR_TEST   = false; // jog both motors at boot to verify wiring
// Force Motor A continuously on (test). Set true for hard-on at 100% duty.
static const bool FORCE_MOTOR_A_ON = false;

// Pinout (change to suit your wiring). All pins must be 3.3V tolerant GPIOs.
// TB6612 → ESP32 mapping (C6 defaults; S3 remapped per board below):
//  STBY  → TB_STBY
//  AIN1  → M1_IN1,  AIN2 → M1_IN2,  PWMA → M1_PWM  (Motor 1)
//  BIN1  → M2_IN1,  BIN2 → M2_IN2,  PWMB → M2_PWM  (Motor 2)
// Choose pins that are free; these are not used by LCD/UART.
#if defined(BOARD_ESP32S3_35)
// S3 3.5" mapping (header IO5/6/7/9/14/15/16/46): avoid IO46 (input-only)
static const int TB_STBY = 14;
static const int M1_IN1  = 15;
static const int M1_IN2  = 16;
static const int M1_PWM  = 5;   // LEDC PWM
static const int M2_IN1  = 6;
static const int M2_IN2  = 7;
static const int M2_PWM  = 9;   // LEDC PWM
#elif defined(BOARD_ESP32P4_43)
// P4 4.3" mapping: avoid GPIO 7/8 (I2C touch), GPIO 16/17 (C6 UART)
// Use free GPIOs: 11, 12, 13, 14, 15, 36, 37, 38 etc.
static const int TB_STBY = 11;  // STBY
static const int M1_IN1  = 12;
static const int M1_IN2  = 13;
static const int M1_PWM  = 14;  // LEDC PWM
static const int M2_IN1  = 15;
static const int M2_IN2  = 36;
static const int M2_PWM  = 37;  // LEDC PWM
#else
// C6 defaults
static const int TB_STBY = 3;  // use free GPIO3 (SPI MISO pad) for STBY
static const int M1_IN1  = 7;
static const int M1_IN2  = 8;
static const int M1_PWM  = 5;  // LEDC PWM
static const int M2_IN1  = 4;
static const int M2_IN2  = 6;
static const int M2_PWM  = 9;  // LEDC PWM
#endif

// Control policy thresholds and timing
static float PH_MIN = 6.80f, PH_MAX = 7.60f;   // outside → run Motor1
static int   ORP_MIN = 250, ORP_MAX = 850;     // mV outside → run Motor2
// static const uint32_t MOTOR_RUN_MS = 5000;     // run time per correction burst (disabled: policy controls)
static uint8_t  M1_SPEED_PC = 60;     // PWM duty % (pH)
static uint8_t  M2_SPEED_PC = 60;     // PWM duty % (ORP)
// Pump flow rates (ml/min at 100% speed) - default 50ml/min for 5x3mm tube
static float M1_FLOW_RATE = 50.0f;   // ml/min at 100% speed for Motor 1
static float M2_FLOW_RATE = 50.0f;   // ml/min at 100% speed for Motor 2
// static const uint32_t MOTOR_COOLDOWN_MS = 2000; // pause after burst (disabled)

// Safety limits (loaded from storage, with sensible defaults)
static float MAX_DAILY_VOLUME = 500.0f;       // ml per day per pump (prevents runaway)
static float MAX_SESSION_VOLUME = 50.0f;      // ml per session (prevents single overdose)
static int MAX_SESSION_DURATION = 300;        // seconds (5 minutes max continuous run)
static float PH_SANITY_MIN = 4.0f;            // pH below this is sensor error
static float PH_SANITY_MAX = 10.0f;           // pH above this is sensor error
static int ORP_SANITY_MIN = -200;             // mV below this is sensor error
static int ORP_SANITY_MAX = 1200;             // mV above this is sensor error
static int SENSOR_TIMEOUT = 300;              // seconds without valid sensor data before stop

// WhatsApp notifications (CallMeBot API)
static const char* CALLMEBOT_API_KEY = "1378196";  // User's API key
static String WHATSAPP_PHONE = "";             // +31... format (loaded from storage)
static bool WHATSAPP_ENABLED = false;          // Enable/disable notifications

// PWM setup (use Arduino analogWrite APIs for C6)
static const int PWM_FREQ = 6000; // 6 kHz (smoother for TB6612)
static const int PWM_BITS = 10;   // 0..1023 finer control

// Internal motor state
// static uint32_t m1StopAt = 0, m2StopAt = 0;     // disabled
// static uint32_t m1CoolUntil = 0, m2CoolUntil = 0; // disabled
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
static char MQTT_CLIENTID_BUF[48] = {0};
static const char* MQTT_CLIENTID = MQTT_CLIENTID_BUF;
#if defined(BOARD_ESP32S3_35)
static volatile uint32_t g_s3_lvgl_heartbeat_ms = 0;
#endif

// MQTT is handled by io::MqttClient now
core::Storage g_storage("poolcfg");
#if !defined(USE_JC3248W535) && !defined(BOARD_ESP32P4_43)
static core::DisplayBridge *displayBridge = nullptr;
static uint32_t g_ui_last_lvgl_ms = 0;
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
static bool motorsEnabled = true; // persisted via Storage
// Minimal UI heartbeat (JC simple mode)
#if defined(USE_JC3248W535)
static bool g_minimal_ui_active = false;
#endif
// --- Button for Zigbee commissioning (monitor both common BOOT pins)
#if defined(BOARD_ESP32C6_TOUCH_1_47)
static const int BTN_PIN1 = 9;   // C6: BOOT (GPIO9)
static const int BTN_PIN2 = 0;   // backup
#elif defined(BOARD_ESP32P4_43)
static const int BTN_PIN1 = -1;  // P4: TBD
static const int BTN_PIN2 = -1;  // P4: TBD
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
#elif defined(BOARD_ESP32P4_43)
#include "core/Board.h"
#include "core/boards/Esp32P4Board.h"
#include "boards/pins_config_p4.h"
#include "st7701_lcd.h"
#include "gt911_touch.h"
#include "driver/i2c_master.h"

static core::Esp32P4Board g_boardP4;

// ESP32-P4 uses MIPI-DSI display (ST7701) + GT911 touch
// NO Arduino_GFX - uses native ESP-IDF MIPI-DSI driver
// Direct instances like in the example
static st7701_lcd p4_lcd(LCD_RST);
static gt911_touch p4_touch(TP_I2C_SDA, TP_I2C_SCL, TP_RST, TP_INT);
static bsp_lcd_handles_t p4_lcd_panels;
// MUST be non-static so GT911 driver can access it via extern declaration
i2c_master_bus_handle_t p4_i2c_handle = NULL;

// LVGL buffers for P4 (480x800 requires large buffers in PSRAM)
static lv_disp_draw_buf_t p4_draw_buf;
static lv_color_t *p4_buf = nullptr;
static lv_color_t *p4_buf1 = nullptr;

// Callback to sync touch driver rotation with display rotation
static void p4_lvgl_port_update_callback(lv_disp_drv_t *drv)
{
    switch (drv->rotated) {
    case LV_DISP_ROT_NONE:
        p4_touch.set_rotation(0);
        break;
    case LV_DISP_ROT_90:
        p4_touch.set_rotation(1);
        break;
    case LV_DISP_ROT_180:
        p4_touch.set_rotation(2);
        break;
    case LV_DISP_ROT_270:
        p4_touch.set_rotation(3);
        break;
    }
}
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
static void on_ph_minus_cb(lv_event_t *e){ (void)e; if (M1_SPEED_PC>=5) M1_SPEED_PC-=5; else M1_SPEED_PC=0; g_storage.setM1Speed(M1_SPEED_PC); lv_update_speed_labels(); }
static void on_ph_plus_cb (lv_event_t *e){ (void)e; if (M1_SPEED_PC<=95) M1_SPEED_PC+=5; else M1_SPEED_PC=100; g_storage.setM1Speed(M1_SPEED_PC); lv_update_speed_labels(); }
static void on_orp_minus_cb(lv_event_t *e){ (void)e; if (M2_SPEED_PC>=5) M2_SPEED_PC-=5; else M2_SPEED_PC=0; g_storage.setM2Speed(M2_SPEED_PC); lv_update_speed_labels(); }
static void on_orp_plus_cb (lv_event_t *e){ (void)e; if (M2_SPEED_PC<=95) M2_SPEED_PC+=5; else M2_SPEED_PC=100; g_storage.setM2Speed(M2_SPEED_PC); lv_update_speed_labels(); }
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
static void on_speed_save_cb(lv_event_t *e){ (void)e; g_storage.setM1Speed(M1_SPEED_PC); g_storage.setM2Speed(M2_SPEED_PC); }

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
  mqttClient.setStorage(&g_storage);
  mqttClient.setThresholdRefs(&PH_MIN, &PH_MAX, &ORP_MIN, &ORP_MAX);
  // Load MQTT config from storage (fallback to defaults)
  {
    String h = g_storage.getMqttHost(MQTT_HOST);
    uint16_t p = g_storage.getMqttPort(MQTT_PORT);
    String u = g_storage.getMqttUser(MQTT_USER);
    String pw= g_storage.getMqttPass(MQTT_PASS);
    MQTT_HOST = h; MQTT_PORT = p; MQTT_USER = u; MQTT_PASS = pw;
  }
  // Skip MQTT setup entirely if no host configured
  if (MQTT_HOST.length() == 0) {
      ESP_LOGI("MQTT", "Skipping MQTT: empty host");
      return;
    }
  ESP_LOGI("MQTT", "Config: host='%s' port=%u user='%s' passlen=%u", MQTT_HOST.c_str(), (unsigned)MQTT_PORT, MQTT_USER.c_str(), (unsigned)MQTT_PASS.length());
  mqttClient.begin(MQTT_HOST.c_str(), MQTT_PORT, MQTT_USER.length()?MQTT_USER.c_str():nullptr, MQTT_PASS.length()?MQTT_PASS.c_str():nullptr, MQTT_CLIENTID);
  // One-shot debugged connect probe at boot to print clear outcome
  static bool s_mqtt_probe_done = false;
  if (!s_mqtt_probe_done) {
    s_mqtt_probe_done = true;
    if (WiFi.status() == WL_CONNECTED) {
      mqttClient.setDebug(true);
      mqttClient.ensureConnected();
      mqttClient.setDebug(false);
    } else {
      ESP_LOGI("MQTT", "Deferring initial connect probe: WiFi not connected yet");
    }
  }
}

static void publishDiscoveryOnce() { mqttClient.publishDiscoveryOnce(); }

static void publishStatesIfReady() { mqttClient.publishStatesIfReady(domain::Metrics::instance()); }
extern "C" void requestModeChange(int mode){
  // mode: 1 = Zigbee, 0 = WiFi
  core::Storage::Mode newMode = (mode==1) ? core::Storage::MODE_ZIGBEE : core::Storage::MODE_WIFI_MQTT;
  
  // Only restart if mode actually changed
  if (newMode == runMode) {
    ESP_LOGI("MAIN", "Mode unchanged (%s), no restart needed", 
             (runMode == core::Storage::MODE_ZIGBEE) ? "Zigbee" : "WiFi/MQTT");
    return;
  }
  
  runMode = newMode;
  g_storage.setMode(runMode);
  
  #if defined(BOARD_ESP32P4_43)
  // P4: Always restart to cleanly apply mode change (C6 will switch between WiFi-SDIO and Zigbee)
  ESP_LOGI("MAIN", "P4: Mode changed to %s, restarting...", 
           (runMode == core::Storage::MODE_ZIGBEE) ? "Zigbee (via C6)" : "WiFi/MQTT");
  delay(500); // Give C6 time to process command
  ESP.restart();
  #else
  // C6/S3: Original logic
  if (runMode == core::Storage::MODE_ZIGBEE) {
    if (WiFi.isConnected()) WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    wifiOff = true;
    #if ZB_ENABLED
    if (!zbStarted) {
      if (zbEverJoined) zb_start_joined(); else ui::showHoldToPair();
    }
    #endif
  } else {
    wifiOff = false;
    WIFI_SSID = g_storage.getWifiSsid(WIFI_SSID);
    WIFI_PASSWORD = g_storage.getWifiPass(WIFI_PASSWORD);
    #if ZB_ENABLED
    if (zbStarted) { ESP.restart(); }
    #endif
    if (WIFI_SSID.length()==0) { if (!portal.isActive()) { portal.setStorage(&g_storage); portal.beginAP("PoolLab-Setup"); } }
    else { if (portal.isActive()) portal.stop(); WiFi.mode(WIFI_STA); wifiMgr.ensureSta(); ensureMqtt(); }
  }
  #endif
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

#if USES_ARDUINO_GFX && !defined(USE_JC3248W535)
static inline void drawScreen() {}
#else
static inline void drawScreen() {}
#endif



// (legacy parser verwijderd; io/Tuya wordt gebruikt)

// Parser moved to io/Tuya

// Async WhatsApp sender task (runs in separate task to avoid LVGL stack overflow)
static void sendWhatsAppAsync(void* param) {
  domain::SafetyAlert alert = *((domain::SafetyAlert*)param);
  free(param);
  
  const char* alertMessages[] = {
    "No alert",
    "pH pump (M1) daily limit exceeded",
    "ORP pump (M2) daily limit exceeded",
    "pH pump (M1) session volume limit exceeded",
    "ORP pump (M2) session volume limit exceeded",
    "pH pump (M1) session duration limit exceeded",
    "ORP pump (M2) session duration limit exceeded",
    "pH sensor reading below sanity limit",
    "pH sensor reading above sanity limit",
    "ORP sensor reading below sanity limit",
    "ORP sensor reading above sanity limit",
    "pH sensor timeout - no valid data",
    "ORP sensor timeout - no valid data"
  };
  
  int alertIdx = (int)alert;
  if (alertIdx < 0 || alertIdx >= 13) {
    vTaskDelete(NULL);
    return;
  }
  
  const char* alertMsg = alertMessages[alertIdx];
  
  if (WHATSAPP_ENABLED && WHATSAPP_PHONE.length() > 0 && WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    
    // URL encode the message
    String encodedMsg = "POOLLAB+ALERT:+";
    encodedMsg += String(alertMsg).c_str();
    encodedMsg.replace(" ", "+");
    encodedMsg.replace(",", "%2C");
    encodedMsg.replace("-", "%2D");
    
    // Build CallMeBot URL
    String url = "https://api.callmebot.com/whatsapp.php?phone=";
    url += WHATSAPP_PHONE;
    url += "&text=";
    url += encodedMsg;
    url += "&apikey=";
    url += CALLMEBOT_API_KEY;
    
    ESP_LOGI("SAFETY", "Sending WhatsApp notification to %s...", WHATSAPP_PHONE.c_str());
    
    http.begin(url);
    int httpCode = http.GET();
    
    if (httpCode > 0) {
      ESP_LOGI("SAFETY", "WhatsApp API response: %d", httpCode);
      if (httpCode == 200) {
        String response = http.getString();
        ESP_LOGI("SAFETY", "WhatsApp sent successfully: %s", response.c_str());
      }
    } else {
      ESP_LOGE("SAFETY", "WhatsApp API failed: %s", http.errorToString(httpCode).c_str());
    }
    http.end();
  } else if (WHATSAPP_ENABLED && WHATSAPP_PHONE.length() == 0) {
    ESP_LOGW("SAFETY", "WhatsApp enabled but no phone number configured");
  }
  
  vTaskDelete(NULL);  // Delete this task when done
}

// Safety alert callback: sends MQTT alerts and WhatsApp notifications
void handleSafetyAlert(domain::SafetyAlert alert) {
  // Map alert enum to human-readable strings
  const char* alertNames[] = {
    "none", "daily_limit_m1", "daily_limit_m2", 
    "session_volume_m1", "session_volume_m2",
    "session_duration_m1", "session_duration_m2",
    "ph_sanity_low", "ph_sanity_high",
    "orp_sanity_low", "orp_sanity_high",
    "ph_sensor_timeout", "orp_sensor_timeout"
  };
  
  const char* alertMessages[] = {
    "No alert",
    "pH pump (M1) daily limit exceeded",
    "ORP pump (M2) daily limit exceeded",
    "pH pump (M1) session volume limit exceeded",
    "ORP pump (M2) session volume limit exceeded",
    "pH pump (M1) session duration limit exceeded",
    "ORP pump (M2) session duration limit exceeded",
    "pH sensor reading below sanity limit",
    "pH sensor reading above sanity limit",
    "ORP sensor reading below sanity limit",
    "ORP sensor reading above sanity limit",
    "pH sensor timeout - no valid data",
    "ORP sensor timeout - no valid data"
  };
  
  int alertIdx = (int)alert;
  if (alertIdx < 0 || alertIdx >= (int)(sizeof(alertNames)/sizeof(alertNames[0]))) return;
  
  const char* alertType = alertNames[alertIdx];
  const char* alertMsg = alertMessages[alertIdx];
  
  ESP_LOGE("SAFETY", "🚨 ALERT TRIGGERED: %s - %s", alertType, alertMsg);
  
  // 1. Publish to MQTT (if connected)
  if (mqttClient.isConnected()) {
    mqttClient.publishAlert(alertType, alertMsg);
    ESP_LOGI("SAFETY", "Alert published to MQTT: pool/alert/%s", alertType);
  }
  
  // 2. Send WhatsApp notification via CallMeBot (async in separate task to avoid stack overflow)
  ESP_LOGI("SAFETY", "WhatsApp check: enabled=%d phone_len=%d wifi=%d", 
           WHATSAPP_ENABLED, WHATSAPP_PHONE.length(), WiFi.status() == WL_CONNECTED);
  
  if (WHATSAPP_ENABLED && WHATSAPP_PHONE.length() > 0 && WiFi.status() == WL_CONNECTED) {
    // Create a copy of the alert for the async task
    domain::SafetyAlert* alertCopy = (domain::SafetyAlert*)malloc(sizeof(domain::SafetyAlert));
    *alertCopy = alert;
    
    // Launch WhatsApp sender in separate task with 8KB stack (enough for SSL/TLS)
    xTaskCreate(sendWhatsAppAsync, "whatsapp_send", 8192, alertCopy, 1, NULL);
    ESP_LOGI("SAFETY", "WhatsApp notification task launched");
  } else if (WHATSAPP_ENABLED && WHATSAPP_PHONE.length() == 0) {
    ESP_LOGW("SAFETY", "WhatsApp enabled but no phone number configured");
  } else if (!WHATSAPP_ENABLED) {
    ESP_LOGI("SAFETY", "WhatsApp notifications disabled");
  } else if (WiFi.status() != WL_CONNECTED) {
    ESP_LOGW("SAFETY", "WhatsApp notification skipped - WiFi not connected");
  }
}

// P4 WiFi init task
#if defined(BOARD_ESP32P4_43)
static void p4WifiInitTask(void *arg) {
  Serial.println("P4: WiFi task created, waiting 3s...");
  Serial.flush();
  
  // Wait for system to stabilize
  vTaskDelay(pdMS_TO_TICKS(3000));
  
  Serial.println("P4: WiFi task starting now");
  Serial.flush();
  
  Serial.printf("P4: wifiOff=%d runMode=%d\n", wifiOff ? 1 : 0, (int)runMode);
  Serial.flush();
  
  if (!wifiOff && runMode == core::Storage::MODE_WIFI_MQTT) {
    Serial.println("P4: Getting SSID from storage...");
    Serial.flush();
    
    String ssid = g_storage.getWifiSsid("");
    String pass = g_storage.getWifiPass("");
    
    Serial.printf("P4: SSID length=%d\n", ssid.length());
    Serial.flush();
    
    if (ssid.length() == 0) {
      Serial.println("P4: Starting captive portal");
      portal.setStorage(&g_storage);
      portal.beginAP("PoolLab-Setup");
      if (LVGL_LOCK()) { 
        ui::setIp(WiFi.softAPIP().toString().c_str()); 
        LVGL_UNLOCK(); 
      }
    } else {
      Serial.println("P4: WiFi STA mode starting...");
      Serial.flush();
      
      // Set WiFi to station mode and disconnect (like demo)
      WiFi.mode(WIFI_STA);
      Serial.println("P4: WiFi mode set to STA");
      Serial.flush();
      
      WiFi.disconnect();
      Serial.println("P4: WiFi disconnected");
      Serial.flush();
      
      Serial.println("P4: Setting hostname...");
      Serial.flush();
      
      // Set hostname for mDNS
      uint64_t chipid = ESP.getEfuseMac();
      char hostname[32];
      snprintf(hostname, sizeof(hostname), "poollab-%06llX", (unsigned long long)(chipid & 0xFFFFFFULL));
      WiFi.setHostname(hostname);
      Serial.printf("P4: Hostname set to: %s.local\n", hostname);
      Serial.flush();
      
      delay(100);
      
      Serial.println("P4: Setting up WiFi event handler...");
      Serial.flush();
      
      // Configure event directly
      WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
          Serial.printf("P4: Got IP: %s\n", WiFi.localIP().toString().c_str());
          Serial.flush();
          
          if (USE_LVGL_UI) { 
            g_ui_ip_text = WiFi.localIP().toString(); 
            g_ui_ip_dirty = true; 
          }
          
          // Setup ArduinoOTA with hostname
          uint64_t chipid = ESP.getEfuseMac();
          char hostname[32];
          snprintf(hostname, sizeof(hostname), "poollab-%06llX", (unsigned long long)(chipid & 0xFFFFFFULL));
          ArduinoOTA.setHostname(hostname);
          ArduinoOTA.begin();
          Serial.printf("P4: OTA server at: %s.local:3232\n", hostname);
          Serial.flush();
        }
      });
      
      Serial.printf("P4: Connecting to SSID: %s\n", ssid.c_str());
      Serial.flush();
      WiFi.begin(ssid.c_str(), pass.c_str());
      Serial.println("P4: WiFi.begin() called");
      Serial.flush();
      
      Serial.println("P4: Setting SSID in UI...");
      Serial.flush();
      
      if (USE_LVGL_UI) {
        if (LVGL_LOCK()) { 
          ui::setSsid(ssid.c_str()); 
          LVGL_UNLOCK(); 
        }
      }
      
      Serial.println("P4: Starting WebUI...");
      Serial.flush();
      
      // Start WebUI
      webui.setStorage(&g_storage);
      webui.setMotor(&g_motor);
      webui.setRefs(&PH_MIN, &PH_MAX, &ORP_MIN, &ORP_MAX, &M1_SPEED_PC, &M2_SPEED_PC, &motorsEnabled, &M1_FLOW_RATE, &M2_FLOW_RATE);
      if (!webui.isActive()) webui.begin();
      
      Serial.println("P4: Ensuring MQTT...");
      Serial.flush();
      
      ensureMqtt();
      
      Serial.println("P4: MQTT setup done");
      Serial.flush();
    }
  }
  
  Serial.println("P4: WiFi init task complete");
  Serial.flush();
  
  // Delete this task
  vTaskDelete(NULL);
}
#endif

void setup() {
  // USB serial (do not block UI waiting for monitor)
  Serial.begin(115200);
  
  #if defined(BOARD_ESP32P4_43)
  delay(500);  // Extra delay for P4 to let ESP-HOSTED/SDIO stabilize
  #else
  delay(200);  // Increased delay for serial stability
  #endif
  
  Serial.setTimeout(50);
  
  
  Serial.println("\n\n=== PoolLab Boot Start ===");
  Serial.flush();
  core::Log::init(true);
  ESP_LOGI("BOOT", "Boot start");
  APP_BOOT_MS = millis();
  
  // Build unique MQTT clientId using chip MAC
  uint64_t mac = ESP.getEfuseMac();
  snprintf(MQTT_CLIENTID_BUF, sizeof(MQTT_CLIENTID_BUF), "pool-%04X%08X",
           (unsigned)((mac >> 32) & 0xFFFF),
           (unsigned)(mac & 0xFFFFFFFF));
  Serial.println("MAC and MQTT ID initialized");
  Serial.flush();
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
  #elif defined(BOARD_ESP32P4_43)
    g_boardP4.earlyInit();
    g_boardP4.initPeripherals();
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

  #if USES_ARDUINO_GFX && !defined(USE_JC3248W535)
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
  
  // Initialize display - different paths for different boards
  #if defined(BOARD_ESP32P4_43)
    // ESP32-P4: Initialize MIPI-DSI display (ST7701) - matching lvgl_demo_v8.ino
    ESP_LOGI("MAIN", "Initializing P4 MIPI-DSI display (480x800)");
    
    // Initialize I2C master bus for touch (I2C_NUM_1)
    // IMPORTANT: Use global handle so GT911 driver can retrieve it later!
    i2c_master_bus_config_t i2c_bus_conf = {
      .i2c_port = I2C_NUM_1,
      .sda_io_num = (gpio_num_t)TP_I2C_SDA,
      .scl_io_num = (gpio_num_t)TP_I2C_SCL,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
      .intr_priority = 0,
      .trans_queue_depth = 0,
      .flags = {.enable_internal_pullup = 1},
    };
    i2c_new_master_bus(&i2c_bus_conf, &p4_i2c_handle);
    
    // Initialize LCD and touch (exact sequence like working example)
    p4_lcd.begin();
    
    p4_touch.begin();  // Will call i2c_master_get_bus_handle(1) internally
    
    p4_touch.set_rotation(0);  // Keep touch in portrait mode, we'll transform in callback
    p4_lcd.get_handle(&p4_lcd_panels);
    
    ESP_LOGI("MAIN", "P4 display initialized");
  #elif USES_ARDUINO_GFX && !defined(USE_JC3248W535)
    // ESP32-C6: Initialize Arduino_GFX SPI display
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
    
    #if defined(BOARD_ESP32P4_43)
    // ESP32-P4: Initialize LVGL with MIPI-DSI display in LANDSCAPE
    lv_init();
    // Use hardware resolution with software rotation
    size_t buffer_size = sizeof(lv_color_t) * LCD_V_RES * 100;  // 800 * 100 lines
    p4_buf = (lv_color_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
    p4_buf1 = (lv_color_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
    assert(p4_buf && p4_buf1 && "Failed to allocate LVGL buffers");
    
    lv_disp_draw_buf_init(&p4_draw_buf, p4_buf, p4_buf1, LCD_V_RES * 100);
    
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_H_RES;  // 480
    disp_drv.ver_res = LCD_V_RES;  // 800
    disp_drv.draw_buf = &p4_draw_buf;
    disp_drv.full_refresh = false;
    disp_drv.sw_rotate = 1;  // Enable software rotation
    disp_drv.rotated = LV_DISP_ROT_270;  // 270° for landscape
    disp_drv.flush_cb = [](lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
      const int offsetx1 = area->x1;
      const int offsetx2 = area->x2;
      const int offsety1 = area->y1;
      const int offsety2 = area->y2;
      p4_lcd.lcd_draw_bitmap(offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, &color_p->full);
    };
    disp = lv_disp_drv_register(&disp_drv);
    
    // Register DPI panel callback
    esp_lcd_dpi_panel_event_callbacks_t cbs = {0};
    cbs.on_color_trans_done = [](esp_lcd_panel_handle_t panel_io, esp_lcd_dpi_panel_event_data_t *edata, void *user_ctx) -> bool {
      lv_disp_drv_t *drv = (lv_disp_drv_t *)user_ctx;
      if (drv) lv_disp_flush_ready(drv);
      return false;
    };
    esp_lcd_dpi_panel_register_event_callbacks(p4_lcd_panels.panel, &cbs, &disp_drv);
    
    // Register touch input for portrait mode with debouncing
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = [](lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
      (void)indev_driver;
      static uint32_t last_press_time = 0;
      static uint32_t last_release_time = 0;
      static uint16_t stable_x = 0, stable_y = 0;
      static bool was_pressed = false;
      bool touched;
      uint16_t touchX, touchY;
      uint32_t now = millis();
      
      touched = p4_touch.getTouch(&touchX, &touchY);
      
      // Validate touch data (GT911 sometimes returns 0,0 or max values on error)
      if (touched && (touchX == 0 || touchY == 0 || touchX >= LCD_H_RES || touchY >= LCD_V_RES)) {
        touched = false;  // Ignore invalid touches
      }
      
      if (!touched) {
        // Released - but require minimum press time
        if (was_pressed && (now - last_press_time) > 100) {  // Minimum 100ms press
          data->state = LV_INDEV_STATE_REL;
          was_pressed = false;
          last_release_time = now;
        } else {
          // Still report as pressed to avoid spurious release
          data->state = was_pressed ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
          data->point.x = stable_x;
          data->point.y = stable_y;
        }
      } else {
        // Touched - require debounce after release
        if (!was_pressed && (now - last_release_time) < 200) {
          // Too soon after release, ignore to prevent double-tap
          data->state = LV_INDEV_STATE_REL;
        } else {
          data->state = LV_INDEV_STATE_PR;
          // Portrait mode - pass coordinates directly
          data->point.x = touchX;
          data->point.y = touchY;
          
          if (!was_pressed) {
            last_press_time = now;
            stable_x = touchX;
            stable_y = touchY;
            was_pressed = true;
          } else {
            // Update stable position for movement
            stable_x = touchX;
            stable_y = touchY;
          }
        }
      }
    };
    lv_indev_drv_register(&indev_drv);
    
    ESP_LOGI("MAIN", "P4 LVGL initialized (%dx%d)", LCD_H_RES, LCD_V_RES);
    #elif !defined(USE_JC3248W535)
    // C6/S3: Initialize LVGL via Arduino_GFX bridge
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
    g_storage.begin(false);
    PH_MIN = g_storage.getPhMin(PH_MIN);
    PH_MAX = g_storage.getPhMax(PH_MAX);
    ORP_MIN = g_storage.getOrpMin(ORP_MIN);
    ORP_MAX = g_storage.getOrpMax(ORP_MAX);
    M1_SPEED_PC = (uint8_t)g_storage.getM1Speed(M1_SPEED_PC);
    M2_SPEED_PC = (uint8_t)g_storage.getM2Speed(M2_SPEED_PC);
    M1_FLOW_RATE = g_storage.getM1FlowRate(M1_FLOW_RATE);
    M2_FLOW_RATE = g_storage.getM2FlowRate(M2_FLOW_RATE);
    // Load safety limits
    MAX_DAILY_VOLUME = g_storage.getMaxDailyVolume(MAX_DAILY_VOLUME);
    MAX_SESSION_VOLUME = g_storage.getMaxSessionVolume(MAX_SESSION_VOLUME);
    MAX_SESSION_DURATION = g_storage.getMaxSessionDuration(MAX_SESSION_DURATION);
    PH_SANITY_MIN = g_storage.getPhSanityMin(PH_SANITY_MIN);
    PH_SANITY_MAX = g_storage.getPhSanityMax(PH_SANITY_MAX);
    ORP_SANITY_MIN = g_storage.getOrpSanityMin(ORP_SANITY_MIN);
    ORP_SANITY_MAX = g_storage.getOrpSanityMax(ORP_SANITY_MAX);
    SENSOR_TIMEOUT = g_storage.getSensorTimeout(SENSOR_TIMEOUT);
    // Load WhatsApp notification settings
    WHATSAPP_PHONE = g_storage.getWhatsAppPhone("");
    WHATSAPP_ENABLED = g_storage.getWhatsAppEnabled(false);
  // Load mode from storage
  #if defined(BOARD_ESP32S3_35)
  // S3: Force WiFi mode (no Zigbee support)
  runMode = core::Storage::MODE_WIFI_MQTT;
  #else
  // C6 and P4: Load mode from storage (both support mode switching)
  runMode = g_storage.getMode(core::Storage::MODE_WIFI_MQTT);
  #endif
  savedMode = runMode;
    // Connect UI slider handlers to storage-backed speeds
    ui::Handlers h; h.onSpeedChange = [](int idx, int value){
      value = constrain(value, 0, 100);
      if (idx==1) { M1_SPEED_PC = (uint8_t)value; g_storage.setM1Speed(M1_SPEED_PC); }
      else if (idx==2) { M2_SPEED_PC = (uint8_t)value; g_storage.setM2Speed(M2_SPEED_PC); }
    };
    h.onModeToggle = [](bool zigbee){
      runMode = zigbee ? core::Storage::MODE_ZIGBEE : core::Storage::MODE_WIFI_MQTT;
      #if !defined(BOARD_ESP32S3_35)
      g_storage.setMode(runMode);
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
    h.onWifiReset = [](){ g_storage.setWifiSsid(""); g_storage.setWifiPass(""); g_startPortalRequested = true; ESP.restart(); };
    h.onWifiSave = [](const char *s, const char *p){
      ESP_LOGI("UI", "Saving WiFi: SSID='%s', Pass='%s'", s ? s : "", p ? p : "");
      g_storage.setWifiSsid(s?s:""); 
      g_storage.setWifiPass(p?p:"");
    };
    h.onMqttSave = [](const char *host, uint16_t port, const char *user, const char *pass){
      ESP_LOGI("UI", "Saving MQTT: Host='%s', Port=%u, User='%s', Pass='%s'", host ? host : "", port, user ? user : "", pass ? pass : "");
      g_storage.setMqttHost(host?host:"");
      g_storage.setMqttPort(port);
      g_storage.setMqttUser(user?user:"");
      g_storage.setMqttPass(pass?pass:"");
      MQTT_HOST = g_storage.getMqttHost(MQTT_HOST);
      MQTT_PORT = g_storage.getMqttPort(MQTT_PORT);
      MQTT_USER = g_storage.getMqttUser(MQTT_USER);
      MQTT_PASS = g_storage.getMqttPass(MQTT_PASS);
    };
    h.onSaveSettings = [](){ 
      // Optional: additional saves or restart logic
      requestShowMain();
    };
    h.onCancelSettings = [](){ 
      requestShowMain();
    };
    h.onPumpCalStart = [](int motor_num){
      ESP_LOGI("UI", "Starting pump calibration for M%d (60s @ 100%%)", motor_num);
      // Force motor on at 100% speed for calibration
      if (motor_num == 1) {
        // M1: pH pump
        digitalWrite(M1_IN1, M1_DIR_A ? HIGH : LOW);
        digitalWrite(M1_IN2, M1_DIR_A ? LOW : HIGH);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_4, 1023); // 100%
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_4);
      } else if (motor_num == 2) {
        // M2: ORP pump
        digitalWrite(M2_IN1, M2_DIR_A ? HIGH : LOW);
        digitalWrite(M2_IN2, M2_DIR_A ? LOW : HIGH);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_5, 1023); // 100%
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_5);
      }
      digitalWrite(TB_STBY, HIGH);  // Ensure driver enabled
    };
    h.onPumpCalStop = [](int motor_num){
      ESP_LOGI("UI", "Stopping pump calibration for M%d", motor_num);
      // Stop motor
      if (motor_num == 1) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_4, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_4);
        digitalWrite(M1_IN1, LOW);
        digitalWrite(M1_IN2, LOW);
      } else if (motor_num == 2) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_5, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_5);
        digitalWrite(M2_IN1, LOW);
        digitalWrite(M2_IN2, LOW);
      }
      // User should now measure the pumped volume and calculate: volume_ml / 1 min = ml/min
      // The flow rate will be saved via WebUI settings page
    };
    h.onSaveFlowRate = [](int motor_num, float rate){
      ESP_LOGI("UI", "Saving flow rate for M%d: %.1f ml/min", motor_num, rate);
      if (motor_num == 1) {
        M1_FLOW_RATE = rate;
        g_storage.setM1FlowRate(M1_FLOW_RATE);
      } else if (motor_num == 2) {
        M2_FLOW_RATE = rate;
        g_storage.setM2FlowRate(M2_FLOW_RATE);
      }
    };
    h.onClearEmergencyStop = [](){
      ESP_LOGW("SAFETY", "User requested Emergency Stop reset via UI");
      g_motor.clearEmergencyStop();
      ui::setEmergencyStop(false, "");
      ESP_LOGI("SAFETY", "Emergency Stop cleared - pumps can resume operation");
    };
    h.onTestSafety = [](int test_type){
      ESP_LOGW("SAFETY", "🧪 User triggered safety test: type=%d", test_type);
      domain::SafetyAlert alert = domain::SafetyAlert::NONE;
      const char* alertMsg = "";
      
      switch(test_type) {
        case 1: // Daily limit
          alert = domain::SafetyAlert::DAILY_LIMIT_M1;
          alertMsg = "Daily limit M1";
          break;
        case 2: // Session volume
          alert = domain::SafetyAlert::SESSION_VOLUME_M1;
          alertMsg = "Session volume M1";
          break;
        case 3: // Sensor timeout
          alert = domain::SafetyAlert::PH_SENSOR_TIMEOUT;
          alertMsg = "pH sensor timeout";
          break;
        case 4: // pH sanity
          alert = domain::SafetyAlert::PH_SANITY_HIGH;
          alertMsg = "pH sanity high";
          break;
        case 5: // ORP sanity
          alert = domain::SafetyAlert::ORP_SANITY_LOW;
          alertMsg = "ORP sanity low";
          break;
        default:
          ESP_LOGW("SAFETY", "Unknown test type: %d", test_type);
          return;
      }
      
      // Trigger emergency stop in ControlPolicy (this calls handleSafetyAlert via callback)
      g_motor.triggerTestAlert(alert);
      
      // Force UI update to show banner immediately
      ui::setEmergencyStop(true, alertMsg);
      
      ESP_LOGI("SAFETY", "🧪 Test alert triggered: %s", alertMsg);
      ESP_LOGI("SAFETY", "📱 Check MQTT topics: pool/alert/* and WhatsApp notifications");
      ESP_LOGI("SAFETY", "⚠️ Emergency stop ACTIVE - use 'Reset' button to clear");
    };
    ui::configureHandlers(h);
    ui::setThresholds(PH_MIN, PH_MAX, ORP_MIN, ORP_MAX);
    ui::setInitialSpeeds(M1_SPEED_PC, M2_SPEED_PC);

    // Theme already set earlier under lock for S3; C6 keeps default path

    // Input device (touch) bridge (enabled with safe polling read_cb)
    // P4 already registers its own touch handler earlier (GT911 via p4_touch.begin())
    #if !defined(BOARD_ESP32S3_35) && !defined(BOARD_ESP32P4_43)
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
    #elif defined(BOARD_ESP32S3_35)
    // S3 JC path: BSP touch disabled; register our own LVGL indev using io::Touch (new i2c_master)
    {
      static lv_indev_drv_t indev_drv;
      lv_indev_drv_init(&indev_drv);
      indev_drv.type = LV_INDEV_TYPE_POINTER;
      indev_drv.read_cb = [](lv_indev_drv_t *d, lv_indev_data_t *data){
        (void)d;
        
        // State tracking for debouncing
        static uint32_t last_touch_ms = 0;
        static int16_t last_x = 0, last_y = 0;
        static bool was_pressed = false;
        
        // Debouncing parameters (tuned for responsive but stable touch)
        const uint32_t TOUCH_DEBOUNCE_MS = 150;      // Minimum time between new touch events
        const uint32_t RELEASE_TIMEOUT_MS = 100;      // Time without touch before considering released
        const uint16_t JITTER_TOLERANCE_PX = 8;       // Ignore small movements (jitter)
        
        io::TouchPoint p{};
        uint32_t now = millis();
        
        if (io::readTouchOnce(p) && p.pressed) {
          // Touch detected
          int16_t dx = abs(p.x - last_x);
          int16_t dy = abs(p.y - last_y);
          
          // Check if this is a new touch or just jitter/bounce
          if (was_pressed && 
              (now - last_touch_ms < TOUCH_DEBOUNCE_MS) &&
              dx <= JITTER_TOLERANCE_PX && 
              dy <= JITTER_TOLERANCE_PX) {
            // Within debounce window and same location - maintain current state
            data->point.x = last_x;
            data->point.y = last_y;
            data->state = LV_INDEV_STATE_PRESSED;
            return;
          }
          
          // Valid new touch or continued press outside jitter zone
          if (!was_pressed || (now - last_touch_ms >= TOUCH_DEBOUNCE_MS) || 
              dx > JITTER_TOLERANCE_PX || dy > JITTER_TOLERANCE_PX) {
            last_touch_ms = now;
            last_x = p.x;
            last_y = p.y;
            was_pressed = true;
            data->point.x = p.x;
            data->point.y = p.y;
            data->state = LV_INDEV_STATE_PRESSED;
          }
        } else {
          // No touch detected
          if (was_pressed && (now - last_touch_ms < RELEASE_TIMEOUT_MS)) {
            // Still within release timeout - maintain press state to avoid flicker
            data->point.x = last_x;
            data->point.y = last_y;
            data->state = LV_INDEV_STATE_PRESSED;
          } else {
            // Confirmed release
            was_pressed = false;
            data->state = LV_INDEV_STATE_RELEASED;
          }
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
      // Stats label above pump icon (compact: "15@30" = 15ml @ 30ml/min)
      lv_lbl_pump_ph_stats = lv_label_create(ph_tile);
      lv_label_set_text(lv_lbl_pump_ph_stats, "");
      lv_obj_set_style_text_font(lv_lbl_pump_ph_stats, &lv_font_montserrat_14, 0);
      lv_obj_set_style_text_color(lv_lbl_pump_ph_stats, lv_color_white(), 0);
      lv_obj_set_style_bg_opa(lv_lbl_pump_ph_stats, LV_OPA_80, 0);
      lv_obj_set_style_bg_color(lv_lbl_pump_ph_stats, lv_color_black(), 0);
      lv_obj_set_style_pad_all(lv_lbl_pump_ph_stats, 2, 0);
      lv_obj_align(lv_lbl_pump_ph_stats, LV_ALIGN_BOTTOM_LEFT, 0, -26);
      lv_obj_add_flag(lv_lbl_pump_ph_stats, LV_OBJ_FLAG_HIDDEN);

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
      // Stats label above pump icon (compact: "15@30" = 15ml @ 30ml/min)
      lv_lbl_pump_orp_stats = lv_label_create(orp_tile);
      lv_label_set_text(lv_lbl_pump_orp_stats, "");
      lv_obj_set_style_text_font(lv_lbl_pump_orp_stats, &lv_font_montserrat_14, 0);
      lv_obj_set_style_text_color(lv_lbl_pump_orp_stats, lv_color_white(), 0);
      lv_obj_set_style_bg_opa(lv_lbl_pump_orp_stats, LV_OPA_80, 0);
      lv_obj_set_style_bg_color(lv_lbl_pump_orp_stats, lv_color_black(), 0);
      lv_obj_set_style_pad_all(lv_lbl_pump_orp_stats, 2, 0);
      lv_obj_align(lv_lbl_pump_orp_stats, LV_ALIGN_BOTTOM_LEFT, 0, -26);
      lv_obj_add_flag(lv_lbl_pump_orp_stats, LV_OBJ_FLAG_HIDDEN);

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
      #if HAS_ZIGBEE
      lv_obj_t *row_mode = lv_obj_create(sec_general); lv_obj_remove_style_all(row_mode); lv_obj_set_width(row_mode, LV_PCT(100)); lv_obj_set_height(row_mode, LV_SIZE_CONTENT); lv_obj_set_flex_flow(row_mode, LV_FLEX_FLOW_ROW); lv_obj_set_style_pad_column(row_mode, 12, 0);
      lv_obj_t *lblMode = lv_label_create(row_mode); lv_obj_set_style_text_color(lblMode, lv_color_black(), 0); 
      #if defined(BOARD_ESP32P4_43)
      lv_label_set_text(lblMode, "Zigbee (C6)"); 
      #else
      lv_label_set_text(lblMode, "Zigbee mode");
      #endif
      lv_obj_set_flex_grow(lblMode, 1);
      lv_obj_t *swMode = lv_switch_create(row_mode); lv_obj_set_size(swMode, 50, 24); if (runMode == core::Storage::MODE_ZIGBEE) lv_obj_add_state(swMode, LV_STATE_CHECKED); else lv_obj_clear_state(swMode, LV_STATE_CHECKED);
      lv_obj_add_event_cb(swMode, [](lv_event_t *e){
        if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
        bool zig = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
        int modeInt = zig ? 1 : 0;
        requestModeChange(modeInt);  // Use centralized mode change function
        
        #if !defined(BOARD_ESP32P4_43)
        // C6/S3: Additional inline logic for immediate UI updates
        if (runMode == core::Storage::MODE_ZIGBEE) {
          #if ZB_ENABLED
          if (!zbStarted) {
            if (!zbEverJoined) {
              ui::showHoldToPair();
            }
          }
          #endif
        } else {
          // Switch to WiFi/MQTT immediately
          if (WIFI_SSID.length() == 0) {
            // No creds → start captive portal
            if (!portal.isActive()) { portal.setStorage(&g_storage); portal.beginAP("PoolLab-Setup"); }
          } else {
            // Ensure portal is stopped and bring up STA now
            if (portal.isActive()) portal.stop();
            WiFi.mode(WIFI_STA);
            wifiMgr.ensureSta();
            ensureMqtt();
          }
        }
        #endif
      }, LV_EVENT_ALL, NULL);
      #endif
      // Row: Pair button (right)
      #if ZB_ENABLED
      lv_obj_t *row_pair = lv_obj_create(sec_general); lv_obj_remove_style_all(row_pair); lv_obj_set_width(row_pair, LV_PCT(100)); lv_obj_set_height(row_pair, LV_SIZE_CONTENT); lv_obj_set_flex_flow(row_pair, LV_FLEX_FLOW_ROW); lv_obj_set_style_pad_column(row_pair, 12, 0);
      lv_obj_t *spacer = lv_obj_create(row_pair); lv_obj_remove_style_all(spacer); lv_obj_set_width(spacer, LV_PCT(100)); lv_obj_set_height(spacer, 1); lv_obj_set_flex_grow(spacer, 1);
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
      lv_obj_add_event_cb(lv_sl_speed1, [](lv_event_t *e){ if (lv_event_get_code(e)!=LV_EVENT_VALUE_CHANGED) return; int v = lv_slider_get_value((lv_obj_t*)lv_event_get_target(e)); v = constrain(v,0,100); M1_SPEED_PC = (uint8_t)v; g_storage.setM1Speed(M1_SPEED_PC); if (lv_lbl_speed1) lv_label_set_text_fmt(lv_lbl_speed1, "%u%%", (unsigned)M1_SPEED_PC); }, LV_EVENT_ALL, NULL);
      // Row: ORP
      lv_obj_t *row_orp = lv_obj_create(sec_pumps); lv_obj_remove_style_all(row_orp); lv_obj_set_width(row_orp, LV_PCT(100)); lv_obj_set_height(row_orp, 36); lv_obj_set_flex_flow(row_orp, LV_FLEX_FLOW_ROW); lv_obj_set_style_pad_column(row_orp, 12, 0); lv_obj_set_style_pad_ver(row_orp, 6, 0);
      lv_obj_t *lbl2 = lv_label_create(row_orp); lv_obj_set_style_text_color(lbl2, lv_color_black(), 0); lv_label_set_text(lbl2, "ORP Speed");
      lv_sl_speed2 = lv_slider_create(row_orp); lv_obj_set_height(lv_sl_speed2, 12); lv_obj_set_flex_grow(lv_sl_speed2, 1); lv_obj_set_style_width(lv_sl_speed2, 14, LV_PART_KNOB); lv_obj_set_style_height(lv_sl_speed2, 14, LV_PART_KNOB); lv_slider_set_range(lv_sl_speed2, 0, 100); lv_slider_set_value(lv_sl_speed2, M2_SPEED_PC, LV_ANIM_OFF);
      lv_lbl_speed2 = lv_label_create(row_orp); lv_obj_set_style_text_color(lv_lbl_speed2, lv_color_black(), 0); lv_label_set_text_fmt(lv_lbl_speed2, "%u%%", (unsigned)M2_SPEED_PC);
      lv_obj_t *btn2m = lv_btn_create(row_orp); lv_obj_set_size(btn2m, 28, 24); { lv_obj_t *t = lv_label_create(btn2m); lv_label_set_text(t, "-"); lv_obj_center(t);} lv_obj_add_event_cb(btn2m, on_orp_minus_cb, LV_EVENT_CLICKED, NULL);
      lv_obj_t *btn2p = lv_btn_create(row_orp); lv_obj_set_size(btn2p, 28, 24); { lv_obj_t *t = lv_label_create(btn2p); lv_label_set_text(t, "+"); lv_obj_center(t);} lv_obj_add_event_cb(btn2p, on_orp_plus_cb, LV_EVENT_CLICKED, NULL);
      lv_obj_add_event_cb(lv_sl_speed2, [](lv_event_t *e){ if (lv_event_get_code(e)!=LV_EVENT_VALUE_CHANGED) return; int v = lv_slider_get_value((lv_obj_t*)lv_event_get_target(e)); v = constrain(v,0,100); M2_SPEED_PC = (uint8_t)v; g_storage.setM2Speed(M2_SPEED_PC); if (lv_lbl_speed2) lv_label_set_text_fmt(lv_lbl_speed2, "%u%%", (unsigned)M2_SPEED_PC); }, LV_EVENT_ALL, NULL);

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
      lv_obj_add_event_cb(btnCfgWifi, [](lv_event_t *e){ (void)e; portal.setStorage(&g_storage); portal.beginAP("PoolLab-Setup"); }, LV_EVENT_CLICKED, NULL);
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
    #if USES_ARDUINO_GFX
      build_lvgl_ui();
    #else
      #if !(defined(BOARD_ESP32S3_35) && defined(USE_JC3248W535))
        // Build full PoolLab UI (now with debounced touch for P4 landscape)
        ui::build(false);
        ui::updateValues();
        // Force LVGL to render the first frame immediately
        lv_timer_handler();
        delay(50);
        lv_timer_handler();
      #endif
    #endif
    ESP_LOGI("UI", "After layout timer");
    #if defined(BOARD_ESP32S3_35)
    LVGL_UNLOCK();
    #endif
  }

  // Begin touch AFTER LVGL init (never on S3 JC path; BSP handles it; P4 inits touch earlier)
  #if !(defined(BOARD_ESP32S3_35) && defined(USE_JC3248W535)) && !defined(BOARD_ESP32P4_43)
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

  #if !USE_ANALOG_SENSORS
  if (!DIAG_MODE) {
    // UARTs (RX only) unless TX pins are provided
    TUYA_A.begin(TUYA_BAUD, SERIAL_8N1, RX_A_PIN, TX_A_PIN);
    if (USE_CHANNEL_B) TUYA_B.begin(TUYA_BAUD, SERIAL_8N1, RX_B_PIN, TX_B_PIN);
  }
  #endif

  // Configure Tuya DP ids for new parser module
  #if !USE_ANALOG_SENSORS
  io::tuyaConfigure(DP_TEMP, DP_ORP, DP_PH, DP_ORP_ALT1, DP_PH_ALT1);
  #endif

  // Initialize analog sensors when enabled
  #if USE_ANALOG_SENSORS
  g_analog.begin();
  // Load calibration from storage
  {
    io::AnalogPhOrpSensor::PhCal ph{}; ph.voltsAtPh4 = g_storage.getPhVAt4(3.00f); ph.voltsAtPh10 = g_storage.getPhVAt10(2.00f);
    io::AnalogPhOrpSensor::OrpCal orp{}; orp.voltsAt0mV = g_storage.getOrpVAt0(2.50f); orp.mVPerVolt = g_storage.getOrpMvPerV(1000.0f);
    g_analog.setPhCalibration(ph);
    g_analog.setOrpCalibration(orp);
  }
  #endif
  #if USE_ADS1115
  g_ads.begin();
  {
    io::AdsPhOrpSensor::PhCal ph{}; ph.voltsAtPh4 = g_storage.getPhVAt4(3.00f); ph.voltsAtPh10 = g_storage.getPhVAt10(2.00f);
    io::AdsPhOrpSensor::OrpCal orp{}; orp.voltsAt0mV = g_storage.getOrpVAt0(2.50f); orp.mVPerVolt = g_storage.getOrpMvPerV(1000.0f);
    g_ads.setPhCalibration(ph);
    g_ads.setOrpCalibration(orp);
  }
  #endif

  #if USES_ARDUINO_GFX && !defined(USE_JC3248W535)
  if (!USE_LVGL_UI) {
    pushLine("Ready. Waiting for frames...");
  drawStaticUI();
  updateValueAreas();
  }
  #endif
  // Load persisted configuration early when not using LVGL UI (so boot mode is honored)
  if (!USE_LVGL_UI) {
    g_storage.begin(false);
    PH_MIN = g_storage.getPhMin(PH_MIN);
    PH_MAX = g_storage.getPhMax(PH_MAX);
    ORP_MIN = g_storage.getOrpMin(ORP_MIN);
    ORP_MAX = g_storage.getOrpMax(ORP_MAX);
    M1_SPEED_PC = (uint8_t)g_storage.getM1Speed(M1_SPEED_PC);
    M2_SPEED_PC = (uint8_t)g_storage.getM2Speed(M2_SPEED_PC);
    runMode = g_storage.getMode(core::Storage::MODE_ZIGBEE);
    savedMode = runMode;
  }

  // Respect saved mode; do not force Zigbee on C6
  
  // TB6612 motor init - MUST be before any goto for P4!
  if (MOTOR_ENABLE) {
    g_motor.begin(io::MotorPins{TB_STBY, M1_IN1, M1_IN2, M1_PWM, M2_IN1, M2_IN2, M2_PWM}, PWM_FREQ, PWM_BITS);
    // Register safety alert callback for MQTT + WhatsApp notifications
    g_motor.setAlertCallback(handleSafetyAlert);
    ESP_LOGI("SAFETY", "Alert callback registered (MQTT + WhatsApp)");
  }

  #if !defined(BOARD_ESP32P4_43)
  Serial.println("Starting WiFi/MQTT init");
  Serial.flush();
  #else
  Serial.println("WiFi/MQTT init deferred (P4+C6 SDIO)");
  Serial.flush();
  goto skip_wifi_init_p4;
  #endif
  
  // WiFi + MQTT
  // Start or stop WiFi based on saved mode at boot (respect prior forced-off, e.g. commissioning)
  if (!wifiOff) {
    if (runMode == core::Storage::MODE_WIFI_MQTT) {
      // Load persisted WiFi creds first so we can decide between STA vs captive portal
      WIFI_SSID = g_storage.getWifiSsid(WIFI_SSID);
      WIFI_PASSWORD = g_storage.getWifiPass(WIFI_PASSWORD);
      if (WIFI_SSID.length() == 0) {
        wifiOff = false;
        ESP_LOGI("WiFi", "Boot: starting captive portal (no SSID)");
        portal.setStorage(&g_storage);
        portal.beginAP("PoolLab-Setup");
        delay(1000);  // Give captive portal time to fully initialize
        if (USE_LVGL_UI) {
          #if defined(BOARD_ESP32S3_35) || defined(BOARD_ESP32P4_43)
          if (LVGL_LOCK()) { ui::setIp(WiFi.softAPIP().toString().c_str()); LVGL_UNLOCK(); }
          #else
          ui::setIp(WiFi.softAPIP().toString().c_str());
          #endif
        }
      } else {
        wifiOff = false;
        ESP_LOGI("WiFi", "Boot: WiFi STA starting");
        {
          String ssid = g_storage.getWifiSsid("");
          String pass = g_storage.getWifiPass("");
          wifiMgr.begin(ssid, pass, "poollab", [](const String &ip){ if (USE_LVGL_UI) { g_ui_ip_text = ip; g_ui_ip_dirty = true; } });
          if (USE_LVGL_UI) {
            #if defined(BOARD_ESP32S3_35) || defined(BOARD_ESP32P4_43)
            if (LVGL_LOCK()) { ui::setSsid(ssid.c_str()); LVGL_UNLOCK(); }
            #else
            ui::setSsid(ssid.c_str());
            #endif
          }
          // Ensure WebUI started on both boards
          webui.setStorage(&g_storage);
          webui.setMotor(&g_motor);
          webui.setRefs(&PH_MIN, &PH_MAX, &ORP_MIN, &ORP_MAX, &M1_SPEED_PC, &M2_SPEED_PC, &motorsEnabled, &M1_FLOW_RATE, &M2_FLOW_RATE);
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
  M1_SPEED_PC = (uint8_t)g_storage.getM1Speed(M1_SPEED_PC);
  M2_SPEED_PC = (uint8_t)g_storage.getM2Speed(M2_SPEED_PC);
  motorsEnabled = g_storage.getMotorsEnabled(true);
  
  // TEMPORARY: Force motors enabled if accidentally disabled (remove after confirming working)
  if (!motorsEnabled) {
    ESP_LOGW("MAIN", "⚠️ Motors were disabled! Re-enabling...");
    motorsEnabled = true;
    g_storage.setMotorsEnabled(true);
  }
  
  if (USE_LVGL_UI) ui::setInitialSpeeds(M1_SPEED_PC, M2_SPEED_PC);

  // TB6612 motor init already done above (before P4 goto)
  
  // Optionally send Tuya queries after boot (requires TX pin wired!)
  #if !USE_ANALOG_SENSORS
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
  #endif

         #if defined(BOARD_ESP32P4_43)
         skip_wifi_init_p4:
         ;  // Empty statement to fix C++ warning
         
         // Only start WiFi if in WiFi/MQTT mode
         if (runMode == core::Storage::MODE_WIFI_MQTT) {
          // Start WiFi init in separate task like the demo
           Serial.println("P4: About to create WiFi init task");
           Serial.flush();
           ESP_LOGI("MAIN", "P4: Creating WiFi init task");
           BaseType_t result = xTaskCreatePinnedToCore(p4WifiInitTask, "P4 WiFi Init", 4096, NULL, 4, NULL, 1);
           if (result == pdPASS) {
             Serial.println("P4: WiFi task creation SUCCESS");
             Serial.flush();
             ESP_LOGI("MAIN", "P4: WiFi init task created on core 1");
           } else {
             Serial.println("P4: WiFi task creation FAILED!");
             Serial.flush();
             ESP_LOGE("MAIN", "P4: Failed to create WiFi init task!");
           }
         } else {
           Serial.println("P4: Zigbee mode - WiFi disabled, C6 handles Zigbee");
           Serial.flush();
           ESP_LOGI("MAIN", "P4: Zigbee mode active (C6 bridge)");
         }
         #endif
         
         ESP_LOGI("BOOT", "Setup complete");
         Serial.println("Setup done");
      }

void loop() {
  if (USE_LVGL_UI) {
    #if !defined(BOARD_ESP32S3_35)
    lv_timer_handler();
    #if !defined(BOARD_ESP32P4_43)
    g_ui_last_lvgl_ms = millis();
    #endif
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
      #if !defined(USE_JC3248W535) && !defined(BOARD_ESP32P4_43)
      uint16_t c = toggle ? YELLOW : CYAN;
      gfx->drawRect(0, 0, 171, 319, c);
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
      runMode = core::Storage::MODE_ZIGBEE; g_storage.setMode(runMode);
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
      g_storage.setMode(runMode);
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
      lv_timer_t *t2 = lv_timer_create([](lv_timer_t *tm){ (void)tm; ui::setSavedWifi(g_storage.getWifiSsid("").c_str(), g_storage.getWifiPass("").c_str()); ui::setSavedMqtt(g_storage.getMqttHost("").c_str(), g_storage.getMqttPort(1883), g_storage.getMqttUser("").c_str(), g_storage.getMqttPass("").c_str()); }, 20, NULL);
        lv_timer_set_repeat_count(t2, 1);
        LVGL_UNLOCK(); opened = true;
      }
      #else
      // Non-S3 path: still defer to next tick
      lv_timer_t *t = lv_timer_create([](lv_timer_t *tm){ (void)tm; ui::showSettings(); }, 0, NULL);
      lv_timer_set_repeat_count(t, 1);
      lv_timer_t *t2 = lv_timer_create([](lv_timer_t *tm){ (void)tm; ui::setSavedWifi(g_storage.getWifiSsid("").c_str(), g_storage.getWifiPass("").c_str()); ui::setSavedMqtt(g_storage.getMqttHost("").c_str(), g_storage.getMqttPort(1883), g_storage.getMqttUser("").c_str(), g_storage.getMqttPass("").c_str()); }, 20, NULL);
      lv_timer_set_repeat_count(t2, 1);
      opened = true;
      #endif
      if (opened) g_settingsRequested = false;
    } else {
      g_settingsRequested = false;
    }
  }

  #if !USE_ANALOG_SENSORS
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
  #endif
  #if !defined(USE_JC3248W535)
  #if !USE_ANALOG_SENSORS
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
  #endif

  // Avoid drawing directly with Arduino_GFX while LVGL UI is active

  // Dummy telemetry (optional) - enabled for all paths so baseline shows values
  if (DUMMY_MODE) {
    static uint32_t lastDummyLog = 0;
    updateDummyTelemetry();
    
    // Debug log every 5 seconds BEFORE UI update
    if (millis() - lastDummyLog > 5000) {
      lastDummyLog = millis();
      ESP_LOGI("DUMMY", "Generating: pH=%.2f ORP=%.0fmV Temp=%.1f°C", 
               domain::Metrics::instance().phVal, 
               domain::Metrics::instance().orpMv,
               domain::Metrics::instance().tempC);
    }
    
    // Update LVGL values (this might hang if LVGL task is stuck!)
    if (USE_LVGL_UI) {
      ESP_LOGV("DUMMY", "Calling updateLvglValues...");
      updateLvglValues();
      ESP_LOGV("DUMMY", "updateLvglValues done");
    }
  } else {
    // Ensure LVGL reflects real telemetry as it updates
    if (USE_LVGL_UI) updateLvglValues();
  }

  // Read sensors (internal ADC or ADS1115) and update Metrics when enabled
  #if USE_ANALOG_SENSORS
  {
    static uint32_t lastRead = 0;
    domain::Telemetry t{};
    if (g_analog.read(t)) {
      if (t.havePh)   { METRICS().phVal = t.phVal; METRICS().havePh = true; }
      if (t.haveOrp)  { METRICS().orpMv = t.orpMv; METRICS().haveOrp = true; }
      if (t.haveTemp) { METRICS().tempC = t.tempC; METRICS().haveTemp = true; }
    }
  }
  #endif
  #if USE_ADS1115
  {
    domain::Telemetry t{};
    if (g_ads.read(t)) {
      if (t.havePh)   { METRICS().phVal = t.phVal; METRICS().havePh = true; }
      if (t.haveOrp)  { METRICS().orpMv = t.orpMv; METRICS().haveOrp = true; }
      if (t.haveTemp) { METRICS().tempC = t.tempC; METRICS().haveTemp = true; }
    }
  }
  #endif

  // Start captive portal on request (e.g., after WiFi reset)
  if (g_startPortalRequested) {
    g_startPortalRequested = false;
    // Clear WiFiManager credentials to avoid STA attempts with empty SSID
    wifiMgr.setCredentials("", "");
    portal.setStorage(&g_storage);
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
  if (MOTOR_ENABLE && motorsEnabled) {
    static uint32_t lastLogMs = 0;
    uint32_t nowMs = millis();
    
    domain::ControlConfig cfg{
      PH_MIN, PH_MAX, PH_HYST, ORP_MIN, ORP_HYST, M1_SPEED_PC, M2_SPEED_PC, M1_FLOW_RATE, M2_FLOW_RATE,
      MAX_DAILY_VOLUME, MAX_SESSION_VOLUME, MAX_SESSION_DURATION,
      PH_SANITY_MIN, PH_SANITY_MAX, ORP_SANITY_MIN, ORP_SANITY_MAX, SENSOR_TIMEOUT
    };
    if (!emergencyStop) {
      g_motor.tick(cfg,
                      domain::Metrics::instance().havePh,
                      domain::Metrics::instance().phVal,
                      domain::Metrics::instance().haveOrp,
                      domain::Metrics::instance().orpMv,
                 (bool)(FORCE_MOTOR_A_ON && !emergencyStop));
      m1Running = g_motor.isM1Running();
      m2Running = g_motor.isM2Running();
      
      // Debug log every 5 seconds
      if (nowMs - lastLogMs > 5000) {
        lastLogMs = nowMs;
        ESP_LOGI("MOTOR", "pH=%.2f (%.2f-%.2f) ORP=%.0f (%d-%d) M1=%d M2=%d", 
                 domain::Metrics::instance().phVal, PH_MIN, PH_MAX,
                 domain::Metrics::instance().orpMv, ORP_MIN, ORP_MAX,
                 m1Running, m2Running);
      }
    } else {
      g_motor.stopAll();
      m1Running = false; m2Running = false;
    }
    if (USE_LVGL_UI) {
      updateLvglValues();
      // Get pump stats for tile display
      domain::PumpStats m1Stats = g_motor.getM1Stats();
      domain::PumpStats m2Stats = g_motor.getM2Stats();
      
      // Module UI pump icons with stats (S3 uses ui:: module, C6 uses direct LVGL objects)
      #ifdef USE_JC3248W535
        // S3: Use module UI
        ui::setPumpStats(m1Running, m1Stats.sessionVolumeMl, m1Stats.currentFlowMlMin, 
                         m2Running, m2Stats.sessionVolumeMl, m2Stats.currentFlowMlMin);
        // Update emergency stop status in UI
        static bool lastEmergencyState = false;
        bool currentEmergencyState = g_motor.isEmergencyStop();
        if (currentEmergencyState != lastEmergencyState) {
          if (currentEmergencyState) {
            domain::SafetyAlert lastAlert = g_motor.getLastAlert();
            const char* alertMessages[] = {
              "", "Daily limit M1", "Daily limit M2", 
              "Session volume M1", "Session volume M2",
              "Session duration M1", "Session duration M2",
              "pH sanity low", "pH sanity high",
              "ORP sanity low", "ORP sanity high",
              "pH sensor timeout", "ORP sensor timeout"
            };
            int alertIdx = (int)lastAlert;
            const char* msg = (alertIdx >= 0 && alertIdx < 13) ? alertMessages[alertIdx] : "Unknown alert";
            ui::setEmergencyStop(true, msg);
            ESP_LOGW("UI", "Emergency stop banner shown: %s", msg);
          } else {
            ui::setEmergencyStop(false, "");
          }
          lastEmergencyState = currentEmergencyState;
        }
      #elif defined(BOARD_ESP32P4_43)
        // P4: Use module UI like S3
        ui::setPumpStats(m1Running, m1Stats.sessionVolumeMl, m1Stats.currentFlowMlMin, 
                         m2Running, m2Stats.sessionVolumeMl, m2Stats.currentFlowMlMin);
      #else
        // C6: Legacy direct LVGL object manipulation
        if (lv_img_pump_ph && lv_img_pump_ph_shadow && lv_lbl_pump_ph_stats) {
          if (m1Running) { 
            lv_obj_clear_flag(lv_img_pump_ph, LV_OBJ_FLAG_HIDDEN); 
            lv_obj_clear_flag(lv_img_pump_ph_shadow, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(lv_lbl_pump_ph_stats, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(lv_lbl_pump_ph_stats, "%.0f@%.0f", m1Stats.sessionVolumeMl, m1Stats.currentFlowMlMin);
          }
          else { 
            lv_obj_add_flag(lv_img_pump_ph, LV_OBJ_FLAG_HIDDEN); 
            lv_obj_add_flag(lv_img_pump_ph_shadow, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(lv_lbl_pump_ph_stats, LV_OBJ_FLAG_HIDDEN);
          }
        }
        if (lv_img_pump_orp && lv_img_pump_orp_shadow && lv_lbl_pump_orp_stats) {
          if (m2Running) { 
            lv_obj_clear_flag(lv_img_pump_orp, LV_OBJ_FLAG_HIDDEN); 
            lv_obj_clear_flag(lv_img_pump_orp_shadow, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(lv_lbl_pump_orp_stats, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(lv_lbl_pump_orp_stats, "%.0f@%.0f", m2Stats.sessionVolumeMl, m2Stats.currentFlowMlMin);
          }
          else { 
            lv_obj_add_flag(lv_img_pump_orp, LV_OBJ_FLAG_HIDDEN); 
            lv_obj_add_flag(lv_img_pump_orp_shadow, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(lv_lbl_pump_orp_stats, LV_OBJ_FLAG_HIDDEN);
          }
        }
      #endif
    }
    
    // Periodically save and log pump statistics
    static uint32_t lastPumpStatsSave = 0;
    static uint32_t lastPumpStatsLog = 0;
    if (now - lastPumpStatsSave > 60000) {  // Save every minute
      lastPumpStatsSave = now;
      domain::PumpStats m1 = g_motor.getM1Stats();
      domain::PumpStats m2 = g_motor.getM2Stats();
      g_storage.setM1TotalVolume(m1.totalVolumeMl);
      g_storage.setM2TotalVolume(m2.totalVolumeMl);
    }
    if (now - lastPumpStatsLog > 10000) {  // Log every 10 seconds
      lastPumpStatsLog = now;
      domain::PumpStats m1 = g_motor.getM1Stats();
      domain::PumpStats m2 = g_motor.getM2Stats();
      ESP_LOGI("PUMP", "M1(pH): Session=%.1fml Flow=%.1fml/min Daily=%.1fml Total=%.1fml", 
               m1.sessionVolumeMl, m1.currentFlowMlMin, m1.dailyVolumeMl, m1.totalVolumeMl);
      ESP_LOGI("PUMP", "M2(ORP): Session=%.1fml Flow=%.1fml/min Daily=%.1fml Total=%.1fml", 
               m2.sessionVolumeMl, m2.currentFlowMlMin, m2.dailyVolumeMl, m2.totalVolumeMl);
    }
    
    // Daily reset check (at midnight)
    static int lastDay = -1;
    time_t nowTime = time(NULL);
    struct tm *timeinfo = localtime(&nowTime);
    int currentDay = timeinfo->tm_yday;  // Day of year (0-365)
    if (lastDay == -1) {
      lastDay = g_storage.getLastResetDay(currentDay);
    }
    if (currentDay != lastDay) {
      ESP_LOGI("PUMP", "Daily reset: Day changed from %d to %d", lastDay, currentDay);
      g_motor.resetAllDaily();
      g_storage.setLastResetDay(currentDay);
      lastDay = currentDay;
    }
  } else if (MOTOR_ENABLE && !motorsEnabled) {
    // Ensure outputs are off when disabled
    g_motor.stopAll();
    m1Running = false; m2Running = false;
    if (USE_LVGL_UI) ui::setPumpStats(false, 0, 0, false, 0, 0);
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
      if (zbPhMinPending) { zbPhMinPending = false; PH_MIN = zbPhMinValue; g_storage.setPhMin(PH_MIN); }
      if (zbPhMaxPending) { zbPhMaxPending = false; PH_MAX = zbPhMaxValue; g_storage.setPhMax(PH_MAX); }
      if (zbOrpMinPending) { zbOrpMinPending = false; ORP_MIN = (int)lrintf(zbOrpMinValue); g_storage.setOrpMin(ORP_MIN); }
      if (zbOrpMaxPending) { zbOrpMaxPending = false; ORP_MAX = (int)lrintf(zbOrpMaxValue); g_storage.setOrpMax(ORP_MAX); }
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

  // C6-only: UI watchdog to recover from rare LVGL stalls
  #if !defined(BOARD_ESP32S3_35)
  if (USE_LVGL_UI) {
    static uint32_t next_watchdog_action = 0;
    uint32_t now_ms = millis();
    if (now_ms >= next_watchdog_action) {
      #if !defined(BOARD_ESP32P4_43)
      // If lv_timer_handler() hasn't executed in >5s, rebuild UI
      if (g_ui_last_lvgl_ms != 0 && (now_ms - g_ui_last_lvgl_ms) > 5000) {
        ESP_LOGW("UI", "LVGL watchdog: UI inactive for >5s, rebuilding screen");
        lv_obj_clean(lv_scr_act());
        ui::build(false);
        ui::updateValues();
        ESP_LOGI("UI", "LVGL watchdog: rebuild complete (C6)");
        g_ui_last_lvgl_ms = now_ms;
        next_watchdog_action = now_ms + 10000; // cool-down to avoid thrash
      } else {
        next_watchdog_action = now_ms + 1000;
      }
      #endif
    }
  }
  #endif

  // S3-only: LVGL watchdog to recover from UI stalls (both BSP and non-BSP paths)
  #if defined(BOARD_ESP32S3_35)
  if (USE_LVGL_UI) {
    static uint32_t s3_next_watchdog = 0;
    uint32_t now_ms = millis();
    if (now_ms >= s3_next_watchdog) {
      // If heartbeat older than 8s, try to rebuild UI safely under lock
      if (g_s3_lvgl_heartbeat_ms != 0 && (now_ms - g_s3_lvgl_heartbeat_ms) > 8000) {
        ESP_LOGW("UI", "S3 LVGL watchdog: heartbeat stalled, rebuilding UI");
        if (LVGL_LOCK()) {
          lv_obj_clean(lv_scr_act());
          ui::build(false);
          ui::updateValues();
          LVGL_UNLOCK();
          ESP_LOGI("UI", "S3 LVGL watchdog: rebuild complete");
          g_s3_lvgl_heartbeat_ms = now_ms;
        }
        s3_next_watchdog = now_ms + 15000; // cool-down
      } else {
        s3_next_watchdog = now_ms + 2000;
      }
    }
  }
  #endif
}

// Legacy pagination removed

extern "C" void requestMqttReload(){
  MQTT_HOST = g_storage.getMqttHost(MQTT_HOST);
  MQTT_PORT = g_storage.getMqttPort(MQTT_PORT);
  MQTT_USER = g_storage.getMqttUser(MQTT_USER);
  MQTT_PASS = g_storage.getMqttPass(MQTT_PASS);
  if (WiFi.status() == WL_CONNECTED) {
    ensureMqtt();
  }
}