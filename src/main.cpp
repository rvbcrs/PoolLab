// Pura — ESP32 pool monitor (pH, ORP, temp) with LVGL UI, MQTT, motors

#include <Arduino.h>
#include "driver/ledc.h"
#include <stdio.h>
#include <stdarg.h>
#include <HardwareSerial.h>
#include <SPI.h>
#include <vector>
#include <lvgl.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <esp_log.h>
#include <SPIFFS.h>
#include <FS.h>
#include "domain/DummySensor.h"
#include "domain/Telemetry.h"
// Board-specific display/touch/BSP includes are in core/boards/ implementations

// Board-agnostic LVGL locking — delegates to the active Board implementation
#define LVGL_LOCK()   getBoard().lvglLock()
#define LVGL_UNLOCK() getBoard().lvglUnlock()
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
static bool wifiOff = false;
static volatile bool g_settingsRequested = false;
static volatile bool g_startPortalRequested = false;
static volatile bool g_ui_ip_dirty = false;
static char g_ui_ip_buf[24] = {0};  // Thread-safe fixed buffer for IP text (replaces String)
static volatile bool g_showMainRequested = false;

extern "C" void requestShowMain(){ g_showMainRequested = true; }
// Global boot timestamp for UI grace periods
static uint32_t APP_BOOT_MS = 0;

// Core modules
#include "core/Storage.h"
#include "core/Board.h"
#include "domain/Metrics.h"
#include "domain/History.h"
#include "domain/ControlPolicy.h"
// IO modules
#include "io/MqttClient.h"
#include "io/Touch.h"
#include "io/AnalogPhOrpSensor.h"
#include "io/AdsPhOrpSensor.h"
#include "io/ZigbeeClient.h"
#include "io/CaptivePortal.h"
#include "io/PowerManager.h"
#include "io/Speaker.h"
#include "ui/UI.h"
// Provide C-linkage declarations for image assets used by UI when included here
extern "C" {
  extern const lv_img_dsc_t water_ph_32dp_E3E3E3_FILL0_wght400_GRAD0_opsz40;
  extern const lv_img_dsc_t water_orp_32dp_E3E3E3_FILL0_wght400_GRAD0_opsz40;
}
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
// ====== USER CONFIG ======
// Set to true for a minimal diagnostic mode (serial prints + color flashes)
static const bool DIAG_MODE = false;
// Board pins snapshot — populated in setup() from getBoard().pins()
static core::BoardPins g_pins;

// ===== Analog sensor (PH4502C/ORP) integration =====
// Enable to read pH and ORP from two ADC pins (PH-4502C / ORP-4502C boards)
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
  // Expose to UI namespace for calibration
  namespace ui { extern io::AnalogPhOrpSensor& g_analog; }
  io::AnalogPhOrpSensor& ui::g_analog = ::g_analog;
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
  io::AdsPhOrpSensor g_ads(ADS_ADDR, ADS_SDA, ADS_SCL, ADS_CH_PH, ADS_CH_ORP, io::AdsPhOrpSensor::GAIN_2_3, 8, 1000);
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
static lv_obj_t *lv_lbl_batt = nullptr;  // Battery level label
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
#if USE_ANALOG_SENSORS || USE_ADS1115
static const bool DUMMY_MODE = false;  // sensors are connected
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

// Motor pin aliases resolved from g_pins (populated in setup() via getBoard().pins()).
// Defined as references to g_pins fields for readable use at call sites.
#define TB_STBY (g_pins.motorStby)
#define M1_IN1  (g_pins.m1in1)
#define M1_IN2  (g_pins.m1in2)
#define M1_PWM  (g_pins.m1pwm)
#define M2_IN1  (g_pins.m2in1)
#define M2_IN2  (g_pins.m2in2)
#define M2_PWM  (g_pins.m2pwm)

// Control policy thresholds and timing
static float PH_MIN = 6.80f, PH_MAX = 7.60f;   // outside → run Motor1
static int   ORP_MIN = 250, ORP_MAX = 850;     // mV outside → run Motor2
static uint8_t  M1_SPEED_PC = 60;     // PWM duty % (pH)
static uint8_t  M2_SPEED_PC = 60;     // PWM duty % (ORP)
// Pump flow rates (ml/min at 100% speed) - default 50ml/min for 5x3mm tube
static float M1_FLOW_RATE = 50.0f;   // ml/min at 100% speed for Motor 1
static float M2_FLOW_RATE = 50.0f;   // ml/min at 100% speed for Motor 2

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
static String CALLMEBOT_API_KEY = "";             // Loaded from storage (NVS)
static String WHATSAPP_PHONE = "";             // +31... format (loaded from storage)
static bool WHATSAPP_ENABLED = false;          // Enable/disable notifications

// PWM setup (use Arduino analogWrite APIs for C6)
static const int PWM_FREQ = 6000; // 6 kHz (smoother for TB6612)
static const int PWM_BITS = 10;   // 0..1023 finer control

// Internal motor state
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

// MQTT is handled by io::MqttClient now
core::Storage g_storage("poolcfg");
static uint32_t g_ui_last_lvgl_ms = 0;
static io::MotorController g_motor;
static io::MqttClient mqttClient;
static io::WiFiManager wifiMgr;
static io::ZigbeeClient zigbee;
static io::CaptivePortal portal;
io::WebUI webui;
static core::Storage::Mode runMode = core::Storage::MODE_WIFI_MQTT;
static core::Storage::Mode savedMode = core::Storage::MODE_WIFI_MQTT;
static bool modeForced = false;
static bool motorsEnabled = true; // persisted via Storage
// Minimal UI heartbeat (JC simple mode)
#if defined(USE_JC3248W535)
static bool g_minimal_ui_active = false;
#endif
// Button pin aliases resolved from g_pins (populated in setup())
#define BTN_PIN1 (g_pins.btn1)
#define BTN_PIN2 (g_pins.btn2)
// If BOOT is not wired on this board, fall back to GPIO0 only
static io::Buttons g_buttons;

// (WiFi events handled in io::WiFiManager)
// Extra: in DIAG_MODE we try multiple candidates in case of board revision
static const int BL_CANDIDATES[] = {2, 1, 3, 20, 21, 19, 18, 17, 16, 15, 14, 13, 12, 11, 5, 4};
static const size_t BL_CANDIDATES_COUNT = sizeof(BL_CANDIDATES) / sizeof(BL_CANDIDATES[0]);
// =========================

// ---- Display / touch objects now live in core/boards/ implementations ----
// getBoard().initDisplay() and getBoard().initTouch() replace the old #ifdef blocks.



#ifndef ARDUINO_USB_CDC_ON_BOOT
#define ARDUINO_USB_CDC_ON_BOOT 1
#endif

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
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_5, 0);  // M1
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_5);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_4, 0);  // M2
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_4);
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
  if (!LVGL_LOCK()) return;
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
      if (zigbee.isConnected() && zigbee.isJoined()) {
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
  LVGL_UNLOCK();
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


// ---- Simple vector icons (drawn with primitives) ----
// Legacy Arduino_GFX icon helpers removed

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
    if (!zigbee.isStarted()) {
      if (zigbee.everJoined()) zigbee.startJoined(); else ui::showHoldToPair();
    }
    #endif
  } else {
    wifiOff = false;
    WIFI_SSID = g_storage.getWifiSsid(WIFI_SSID);
    WIFI_PASSWORD = g_storage.getWifiPass(WIFI_PASSWORD);
    #if ZB_ENABLED
    if (zigbee.isStarted()) { ESP.restart(); }
    #endif
    if (WIFI_SSID.length()==0) { if (!portal.isActive()) { portal.setStorage(&g_storage); portal.beginAP("Pura-Setup"); } }
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
  
  if (WHATSAPP_ENABLED && WHATSAPP_PHONE.length() > 0 && CALLMEBOT_API_KEY.length() > 0 && WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    // URL encode the message
    String encodedMsg = "PURA+ALERT:+";
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
  // Trigger audible alarm
  speaker.alarm();

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
  
  if (WHATSAPP_ENABLED && WHATSAPP_PHONE.length() > 0 && CALLMEBOT_API_KEY.length() > 0 && WiFi.status() == WL_CONNECTED) {
    // Create a copy of the alert for the async task
    domain::SafetyAlert* alertCopy = (domain::SafetyAlert*)malloc(sizeof(domain::SafetyAlert));
    *alertCopy = alert;

    // Launch WhatsApp sender in separate task with 8KB stack (enough for SSL/TLS)
    xTaskCreate(sendWhatsAppAsync, "whatsapp_send", 8192, alertCopy, 1, NULL);
    ESP_LOGI("SAFETY", "WhatsApp notification task launched");
  } else if (WHATSAPP_ENABLED && (WHATSAPP_PHONE.length() == 0 || CALLMEBOT_API_KEY.length() == 0)) {
    ESP_LOGW("SAFETY", "WhatsApp enabled but phone or API key not configured");
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
      portal.beginAP("Pura-Setup");
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
      snprintf(hostname, sizeof(hostname), "pura-%06llX", (unsigned long long)(chipid & 0xFFFFFFULL));
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
            strncpy(g_ui_ip_buf, WiFi.localIP().toString().c_str(), sizeof(g_ui_ip_buf)-1); g_ui_ip_buf[sizeof(g_ui_ip_buf)-1] = '\0'; 
            g_ui_ip_dirty = true; 
          }
          
          // Setup ArduinoOTA with hostname
          uint64_t chipid = ESP.getEfuseMac();
          char hostname[32];
          snprintf(hostname, sizeof(hostname), "pura-%06llX", (unsigned long long)(chipid & 0xFFFFFFULL));
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

// Feedback callback for LVGL input devices (touch)
// CRITICAL: Do NOT call blocking functions (like i2s_write) here if called from ISR/Timer!
volatile bool g_click_requested = false;
void my_input_feedback_cb(lv_indev_drv_t *drv, uint8_t code) {
  if (code == LV_EVENT_PRESSED) {
    g_click_requested = true;
  }
}

void setup() {
  // USB serial (do not block UI waiting for monitor)
  Serial.begin(115200);
  
  #if defined(BOARD_ESP32P4_43)
  delay(500);  // Extra delay for P4 to let ESP-HOSTED/SDIO stabilize
  #else
  delay(200);  // Increased delay for serial stability
  #endif
  
  Serial.setTimeout(50);
  
  
  Serial.println("\n\n=== Pura Boot Start ===");
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

  // Populate board pin map early (needed before speaker.setup() and motor init)
  g_pins = getBoard().pins();

  // S3 JC path: Power Manager (IP5306) + Speaker
  // (Display init is handled by getBoard().initDisplay() below)
  #if defined(USE_JC3248W535)
    Power.begin(18, 19);
    speaker.setup(g_pins.i2sBclk, g_pins.i2sLrc, g_pins.i2sDin);
    if (!Power.isConnected()) {
      Serial.println("PowerManager: IP5306 not found on 18,19.");
    }
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

  // Board-agnostic: early init and peripheral setup
  getBoard().earlyInit();
  getBoard().initPeripherals();

  // Display + LVGL initialisation — all board-specific code is in core/boards/
  lv_disp_t *disp = getBoard().initDisplay();
  #if defined(USE_JC3248W535)
  g_minimal_ui_active = true;
  #endif

  // Open NVS before UI init so the persisted theme is honoured on first paint.
  g_storage.begin(false);

  if (USE_LVGL_UI) {
    // Acquire LVGL mutex (no-op on C6/P4; BSP semaphore on S3)
    LVGL_LOCK();
    ESP_LOGI("UI", "Before ui::init; res=%dx%d", (int)lv_disp_get_hor_res(NULL), (int)lv_disp_get_ver_res(NULL));
    ui::init(disp);
    ESP_LOGI("UI", "After ui::init");
    
    // Register audio feedback callback for all input devices
    lv_indev_t *indev = lv_indev_get_next(NULL);
    while (indev) {
        if (indev->driver->type == LV_INDEV_TYPE_POINTER) {
            indev->driver->feedback_cb = my_input_feedback_cb;
            ESP_LOGI("MAIN", "Registered audio feedback for pointer device");
        }
        indev = lv_indev_get_next(indev);
    }
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
    CALLMEBOT_API_KEY = g_storage.getCallMeBotApiKey("");
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
        if (!zigbee.isStarted() && zigbee.everJoined()) {
          zigbee.startJoined();
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
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_5, 1023); // 100%
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_5);
      } else if (motor_num == 2) {
        // M2: ORP pump
        digitalWrite(M2_IN1, M2_DIR_A ? HIGH : LOW);
        digitalWrite(M2_IN2, M2_DIR_A ? LOW : HIGH);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_4, 1023); // 100%
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_4);
      }
      digitalWrite(TB_STBY, HIGH);  // Ensure driver enabled
    };
    h.onPumpCalStop = [](int motor_num){
      ESP_LOGI("UI", "Stopping pump calibration for M%d", motor_num);
      // Stop motor
      if (motor_num == 1) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_5, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_5);
        digitalWrite(M1_IN1, LOW);
        digitalWrite(M1_IN2, LOW);
      } else if (motor_num == 2) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_4, 0);  // M2
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_4);
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

    // Register board-specific touch input device driver
    getBoard().initTouch();

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
      // Note: Battery label is now handled by UI module (lv_lbl_battery in UI.cpp)

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
      lv_label_set_text(lblMode, getBoard().uiConfig().zigbeeLabelText);
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
          if (!zigbee.isStarted()) {
            if (!zigbee.everJoined()) {
              ui::showHoldToPair();
            }
          }
          #endif
        } else {
          // Switch to WiFi/MQTT immediately
          if (WIFI_SSID.length() == 0) {
            // No creds → start captive portal
            if (!portal.isActive()) { portal.setStorage(&g_storage); portal.beginAP("Pura-Setup"); }
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
      if (zigbee.everJoined()) { lv_obj_set_style_bg_color(btnPair, lv_palette_main(LV_PALETTE_RED), 0); lv_label_set_text(lv_label_create(btnPair), "UNPAIR"); }
      else { lv_label_set_text(lv_label_create(btnPair), "PAIR"); }
      lv_obj_add_event_cb(btnPair, [](lv_event_t *e){
        if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
        #if ZB_ENABLED
        if (zigbee.everJoined()) {
          // Perform a Zigbee factory reset (will reboot)
          ESP_LOGI("ZB", "Unpair requested -> factory reset Zigbee");
          zigbee.factoryReset();
        } else {
          ui::showHoldToPair();
          ESP_LOGI("ZB", "Manual commissioning (60s)");
          zigbee.startCommission(60);
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
      lv_obj_add_event_cb(btnCfgWifi, [](lv_event_t *e){ (void)e; portal.setStorage(&g_storage); portal.beginAP("Pura-Setup"); }, LV_EVENT_CLICKED, NULL);
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
        // Build full Pura UI (now with debounced touch for P4 landscape)
        ui::build(false);
        ui::updateValues();
        // Force LVGL to render the first frame immediately
        lv_timer_handler();
        delay(50);
        lv_timer_handler();
      #endif
    #endif
    ESP_LOGI("UI", "After layout timer");
    LVGL_UNLOCK();
  }

  // If touch is noisy at boot it can stall UI. Add a short debounce warmup.
  delay(50);

  // Init buttons (BOOT) with pull-up and debounce state
  // Initialize debounced buttons helper
  g_buttons.begin(io::ButtonPins{ BTN_PIN1, BTN_PIN2 });

  // Init Zigbee client only on platforms that support it and when enabled
  #if ZB_ENABLED && !(defined(BOARD_ESP32S3_35) && defined(USE_JC3248W535))
  {
    io::ZigbeeConfig zcfg{ &PH_MIN, &PH_MAX, &ORP_MIN, &ORP_MAX, &g_storage };
    bool doPair = zigbee.begin(zcfg);
    if (doPair) {
      if (WiFi.isConnected()) WiFi.disconnect(true, true);
      WiFi.mode(WIFI_OFF);
      wifiOff = true;
      ESP_LOGI("ZB", "WiFi disabled for commissioning");
      zigbee.startCommission(120);
    }
  }
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
    float testV = g_ads.sampleVoltsPh();
    Serial.printf("[ADS1115] init: probe AIN0=%.3fV -> %s\n",
      testV, (testV > 0.001f || testV < -0.001f) ? "OK (device responding)" : "no response (check wiring/address)");
    io::AdsPhOrpSensor::PhCal ph{}; ph.voltsAtPh4 = g_storage.getPhVAt4(3.00f); ph.voltsAtPh10 = g_storage.getPhVAt10(2.00f);
    io::AdsPhOrpSensor::OrpCal orp{}; orp.voltsAt0mV = g_storage.getOrpVAt0(2.50f); orp.mVPerVolt = g_storage.getOrpMvPerV(1000.0f);
    g_ads.setPhCalibration(ph);
    g_ads.setOrpCalibration(orp);
  }
  #endif

  #if USES_ARDUINO_GFX && !defined(USE_JC3248W535)
  if (!USE_LVGL_UI) {
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
    // Restore persisted pump volumes from NVS so daily/total survive reboots
    g_motor.restoreM1Volumes(g_storage.getM1TotalVolume(0.0f), g_storage.getM1DailyVolume(0.0f));
    g_motor.restoreM2Volumes(g_storage.getM2TotalVolume(0.0f), g_storage.getM2DailyVolume(0.0f));
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
        portal.beginAP("Pura-Setup");
        delay(1000);  // Give captive portal time to fully initialize
        if (USE_LVGL_UI) {
          if (LVGL_LOCK()) { ui::setIp(WiFi.softAPIP().toString().c_str()); LVGL_UNLOCK(); }
        }
      } else {
        wifiOff = false;
        ESP_LOGI("WiFi", "Boot: WiFi STA starting");
        {
          String ssid = g_storage.getWifiSsid("");
          String pass = g_storage.getWifiPass("");
          wifiMgr.begin(ssid, pass, "pura", [](const String &ip){ if (USE_LVGL_UI) { strncpy(g_ui_ip_buf, ip.c_str(), sizeof(g_ui_ip_buf)-1); g_ui_ip_buf[sizeof(g_ui_ip_buf)-1] = '\0'; g_ui_ip_dirty = true; } });
          if (USE_LVGL_UI) {
            if (LVGL_LOCK()) { ui::setSsid(ssid.c_str()); LVGL_UNLOCK(); }
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
      if (zigbee.everJoined()) {
        zigbee.startJoined();
      }
      #endif
    }
  }

  if (USE_LVGL_UI) ui::setInitialMode(runMode == core::Storage::MODE_ZIGBEE);

  motorsEnabled = g_storage.getMotorsEnabled(true);

  if (USE_LVGL_UI) ui::setInitialSpeeds(M1_SPEED_PC, M2_SPEED_PC);

  // TB6612 motor init already done above (before P4 goto)
  

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
         }
    #endif
  
  ESP_LOGI("BOOT", "Setup complete");
  Serial.println("Setup done");
}

void loop() {
     // Audio Feedback (Offloaded from ISR/Timer context - GLOBAL)
    if (g_click_requested) {
        g_click_requested = false;
        if (g_storage.getSoundEnabled(true)) {
            speaker.beep();
        }
    }
    speaker.loop();

    if (USE_LVGL_UI) {
    delay(0);
    // Apply deferred UI IP update from WiFi callback without crossing threads
    if (g_ui_ip_dirty) {
      g_ui_ip_dirty = false;
      if (LVGL_LOCK()) { ui::setIp(g_ui_ip_buf); LVGL_UNLOCK(); }
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
      // visual heartbeat (serial only — display objects are now board-local)
      static bool toggle = false; toggle = !toggle;
      ESP_LOGI("DIAG", "heartbeat %s", toggle ? "ON" : "OFF");
    }
    return;
  }
  // Apply deferred navigation back to main UI (one-shot)
  if (g_showMainRequested) {
    g_showMainRequested = false;
    // Always defer via zero-delay timer so showMain() runs in the LVGL context
    lv_timer_t *t = lv_timer_create([](lv_timer_t *tm){ (void)tm; ui::showMain(); }, 0, NULL);
    lv_timer_set_repeat_count(t, 1);
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
      zigbee.startCommission(120);
#endif
    }

  // If commissioning finished, optionally restore WiFi
  #if ZB_ENABLED
  if (wifiOff && zigbee.isStarted() && zigbee.commissionUntilMs() && millis() > zigbee.commissionUntilMs()) {
    ESP_LOGI("ZB", "Commissioning window ended");
    zigbee.clearCommissionTimer();
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

  // Defer settings screen outside LVGL event context (lock-protected)
  if (g_settingsRequested) {
    if (USE_LVGL_UI) {
      bool opened = false;
      if (LVGL_LOCK()) {
        lv_timer_t *t = lv_timer_create([](lv_timer_t *tm){ (void)tm; ui::showSettings(); }, 0, NULL);
        lv_timer_set_repeat_count(t, 1);
        lv_timer_t *t2 = lv_timer_create([](lv_timer_t *tm){ (void)tm; ui::setSavedWifi(g_storage.getWifiSsid("").c_str(), g_storage.getWifiPass("").c_str()); ui::setSavedMqtt(g_storage.getMqttHost("").c_str(), g_storage.getMqttPort(1883), g_storage.getMqttUser("").c_str(), g_storage.getMqttPass("").c_str()); }, 20, NULL);
        lv_timer_set_repeat_count(t2, 1);
        LVGL_UNLOCK(); opened = true;
      }
      if (opened) g_settingsRequested = false;
    } else {
      g_settingsRequested = false;
    }
  }


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
    
  }
  // updateLvglValues() already called at top of loop(); no need to repeat here

  // Read sensors (internal ADC or ADS1115) and update Metrics when enabled
  #if USE_ANALOG_SENSORS
  if (!DUMMY_MODE) {
    static uint32_t lastRead = 0;
    static uint32_t lastLog = 0;
    domain::Telemetry t{};
    if (g_analog.read(t)) {
      if (t.havePh)   { METRICS().phVal = t.phVal; METRICS().havePh = true; }
      if (t.haveOrp)  { METRICS().orpMv = t.orpMv; METRICS().haveOrp = true; }
      if (t.haveTemp) { METRICS().tempC = t.tempC; METRICS().haveTemp = true; }
      uint32_t now = millis();
      if (now - lastLog >= 2000) {
        lastLog = now;
        Serial.printf("[ANALOG] pH=%.3f (%.3fV)  ORP=%.0fmV (%.3fV)\n",
          t.havePh ? t.phVal : NAN, g_analog.sampleVoltsPh(),
          t.haveOrp ? t.orpMv : NAN, g_analog.sampleVoltsOrp());
      }
    }
  }
  #endif
  #if USE_ADS1115
  if (!DUMMY_MODE) {
    static uint32_t lastLog = 0;
    domain::Telemetry t{};
    if (g_ads.read(t)) {
      if (t.havePh)   { METRICS().phVal = t.phVal; METRICS().havePh = true; }
      if (t.haveOrp)  { METRICS().orpMv = t.orpMv; METRICS().haveOrp = true; }
      if (t.haveTemp) { METRICS().tempC = t.tempC; METRICS().haveTemp = true; }
      uint32_t now = millis();
      if (now - lastLog >= 2000) {
        lastLog = now;
        char b[128];
        snprintf(b, sizeof(b), "[ADS1115] pH=%.3f (%.3fV)  ORP=%.0fmV (%.3fV)",
          t.havePh ? t.phVal : NAN, g_ads.sampleVoltsPh(),
          t.haveOrp ? t.orpMv : NAN, g_ads.sampleVoltsOrp());
        Serial.println(b);
        if (webui.isActive()) webui.log(b);
      }
    }
  }
  #endif

  // Start captive portal on request (e.g., after WiFi reset)
  if (g_startPortalRequested) {
    g_startPortalRequested = false;
    // Clear WiFiManager credentials to avoid STA attempts with empty SSID
    wifiMgr.setCredentials("", "");
    portal.setStorage(&g_storage);
    if (!portal.isActive()) portal.beginAP("Pura-Setup");
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

  // Feed the 24h sensor history (5-min averages for the WebUI sparklines)
  static uint32_t lastHistSample = 0;
  if (nowMs - lastHistSample > 5000) {
    lastHistSample = nowMs;
    domain::History::instance().sample();
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
  {
    static uint32_t lastGateLog = 0;
    uint32_t nowG = millis();
    if (nowG - lastGateLog > 3000) {
      lastGateLog = nowG;
      char buf[200];
      snprintf(buf, sizeof(buf), "[MOTOR-GATE] enabled=%d motorsEn=%d emergency=%d havePh=%d ph=%.2f haveOrp=%d orp=%.0f m1Run=%d m2Run=%d",
        MOTOR_ENABLE, motorsEnabled, emergencyStop,
        domain::Metrics::instance().havePh, domain::Metrics::instance().phVal,
        domain::Metrics::instance().haveOrp, domain::Metrics::instance().orpMv,
        g_motor.isM1Running(), g_motor.isM2Running());
      Serial.println(buf);
      if (webui.isActive()) webui.log(buf);
    }
  }
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
      #if defined(USE_JC3248W535)
      Power.update();
      // Note: Battery UI is now handled by UI module (ui::refreshNetworkStatus calls Power.getBatteryLevel())
      #endif
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
      g_storage.setM1DailyVolume(m1.dailyVolumeMl);
      g_storage.setM2DailyVolume(m2.dailyVolumeMl);
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
      {
        domain::PumpStats m1h = g_motor.getM1Stats();
        domain::PumpStats m2h = g_motor.getM2Stats();
        g_storage.pushDailyDose(m1h.dailyVolumeMl, m2h.dailyVolumeMl);
      }
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

  // Zigbee periodic reporting and state management
  static uint32_t lastZbReport = 0;
  if (now - lastZbReport > 2000) {
    lastZbReport = now;
    #if ZB_ENABLED
    zigbee.loop();
    if (USE_LVGL_UI && zigbee.isStarted()) {
      // Update link icon
      if (LVGL_LOCK()) {
        if (lv_img_link) {
          if (zigbee.isConnected() && zigbee.isJoined())
            lv_img_set_src(lv_img_link, &link_16dp_999999_FILL0_wght400_GRAD0_opsz20);
          else
            lv_img_set_src(lv_img_link, &link_off_16dp_999999_FILL0_wght400_GRAD0_opsz20);
        }
        LVGL_UNLOCK();
      }
    }
    // Auto-close commissioning modal when commissioning ends
    if (USE_LVGL_UI && lv_zb_modal && zigbee.commissionUntilMs() && millis() > zigbee.commissionUntilMs()) {
      if (LVGL_LOCK()) {
        lv_obj_del(lv_zb_modal);
        lv_zb_modal = nullptr;
        LVGL_UNLOCK();
      }
      zigbee.clearCommissionTimer();
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

  // Board-specific LVGL watchdog (no-op on C6/P4; S3 checks for stalled UI task)
  getBoard().lvglWatchdogTick();
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

extern "C" void requestOrpCalReload(){
  #if USE_ADS1115
  io::AdsPhOrpSensor::OrpCal orp{};
  orp.voltsAt0mV = g_storage.getOrpVAt0(2.50f);
  orp.mVPerVolt  = g_storage.getOrpMvPerV(1000.0f);
  g_ads.setOrpCalibration(orp);
  #endif
  #if USE_ANALOG_SENSORS
  io::AnalogPhOrpSensor::OrpCal orpA{};
  orpA.voltsAt0mV = g_storage.getOrpVAt0(2.50f);
  orpA.mVPerVolt  = g_storage.getOrpMvPerV(1000.0f);
  g_analog.setOrpCalibration(orpA);
  #endif
}