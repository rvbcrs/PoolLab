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
extern "C" const lv_font_t lv_font_source_code_pro_16;
extern "C" const lv_font_t lv_font_montserrat_14;

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

// Main page widgets
static lv_obj_t *lv_lbl_ph = nullptr;
static lv_obj_t *lv_lbl_orp = nullptr;
static lv_obj_t *lv_lbl_temp = nullptr;
static lv_obj_t *lv_lbl_orp_unit = nullptr;
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
static const char* TOPIC_STATE_PH   = "pool/sensor/ph";
static const char* TOPIC_STATE_ORP  = "pool/sensor/orp";
static const char* TOPIC_STATE_TEMP = "pool/sensor/temp";
static const char* DISCOVERY_PH     = "homeassistant/sensor/pool_ph/config";
static const char* DISCOVERY_ORP    = "homeassistant/sensor/pool_orp/config";
static const char* DISCOVERY_TEMP   = "homeassistant/sensor/pool_temp/config";

static WiFiClient wifiClient;
static PubSubClient mqtt(wifiClient);
static bool mqttAnnounced=false;
static Preferences prefs;

// ---- MQTT command topics for threshold control ----
static const char* TOPIC_CFG_PH_MIN   = "pool/cfg/ph_min";
static const char* TOPIC_CFG_PH_MAX   = "pool/cfg/ph_max";
static const char* TOPIC_CFG_ORP_MIN  = "pool/cfg/orp_min";
static const char* TOPIC_CFG_ORP_MAX  = "pool/cfg/orp_max";
static const char* TOPIC_CMD_PH_MIN   = "pool/cmd/ph_min";
static const char* TOPIC_CMD_PH_MAX   = "pool/cmd/ph_max";
static const char* TOPIC_CMD_ORP_MIN  = "pool/cmd/orp_min";
static const char* TOPIC_CMD_ORP_MAX  = "pool/cmd/orp_max";

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
static bool haveTemp=false, havePh=false, haveOrp=false;
static float tempC=0.0f, phVal=0.0f, orpMv=0.0f;
// Once primary DP (106) has been observed for pH, do not allow ALT to overwrite
static bool preferPhPrimary=false;

// ===== LVGL helpers (definitions) =====
static void lv_update_speed_labels(){
  if (lv_lbl_speed1) lv_label_set_text_fmt(lv_lbl_speed1, "%u%%", (unsigned)M1_SPEED_PC);
  if (lv_lbl_speed2) lv_label_set_text_fmt(lv_lbl_speed2, "%u%%", (unsigned)M2_SPEED_PC);
}
static void on_ph_minus_cb(lv_event_t *e){ (void)e; if (M1_SPEED_PC>=5) M1_SPEED_PC-=5; else M1_SPEED_PC=0; lv_update_speed_labels(); }
static void on_ph_plus_cb (lv_event_t *e){ (void)e; if (M1_SPEED_PC<=95) M1_SPEED_PC+=5; else M1_SPEED_PC=100; lv_update_speed_labels(); }
static void on_orp_minus_cb(lv_event_t *e){ (void)e; if (M2_SPEED_PC>=5) M2_SPEED_PC-=5; else M2_SPEED_PC=0; lv_update_speed_labels(); }
static void on_orp_plus_cb (lv_event_t *e){ (void)e; if (M2_SPEED_PC<=95) M2_SPEED_PC+=5; else M2_SPEED_PC=100; lv_update_speed_labels(); }
static void on_speed_save_cb(lv_event_t *e){ (void)e; prefs.putInt("m1_speed", M1_SPEED_PC); prefs.putInt("m2_speed", M2_SPEED_PC); }

// Alert margins and border thickness for near/exceed thresholds (used by LVGL updater)
static const float WARN_MARGIN_PH = 0.05f;   // pH within 0.05 of min/max
static const int   WARN_MARGIN_ORP = 20;     // mV within 20 of min/max

static void updateLvglValues(){
  if (!USE_LVGL_UI) return;
  if (lv_lbl_ph) {
    if (havePh) { char b[24]; snprintf(b, sizeof(b), "%.2f", (double)phVal); lv_label_set_text(lv_lbl_ph, b); }
    else lv_label_set_text(lv_lbl_ph, "--.--");
  }
  if (lv_lbl_orp) {
    if (haveOrp) {
      char b[16]; snprintf(b, sizeof(b), "%d", (int)lrintf(orpMv));
      lv_label_set_text(lv_lbl_orp, b);
      if (lv_lbl_orp_unit) lv_obj_clear_flag(lv_lbl_orp_unit, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_label_set_text(lv_lbl_orp, "----");
      if (lv_lbl_orp_unit) lv_obj_clear_flag(lv_lbl_orp_unit, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (lv_lbl_temp) {
    if (haveTemp) { char b[24]; snprintf(b, sizeof(b), "%.1f C", (double)tempC); lv_label_set_text(lv_lbl_temp, b); }
    else lv_label_set_text(lv_lbl_temp, "--.- C");
  }
  if (lv_lbl_ip) {
    String ip = (WiFi.status()==WL_CONNECTED)? WiFi.localIP().toString() : String("--");
    char b[48]; snprintf(b, sizeof(b), "IP: %s", ip.c_str()); lv_label_set_text(lv_lbl_ip, b);
  }
  if (lv_lbl_m1) {
    if (m1Running) { lv_obj_clear_flag(lv_lbl_m1, LV_OBJ_FLAG_HIDDEN); lv_obj_set_style_text_color(lv_lbl_m1, lv_palette_main(LV_PALETTE_GREEN), 0); }
    else lv_obj_add_flag(lv_lbl_m1, LV_OBJ_FLAG_HIDDEN);
  }
  if (lv_lbl_m2) {
    if (m2Running) { lv_obj_clear_flag(lv_lbl_m2, LV_OBJ_FLAG_HIDDEN); lv_obj_set_style_text_color(lv_lbl_m2, lv_palette_main(LV_PALETTE_GREEN), 0); }
    else lv_obj_add_flag(lv_lbl_m2, LV_OBJ_FLAG_HIDDEN);
  }

  // Card background color by status
  if (lv_card_ph) {
    lv_color_t base = lv_palette_main(LV_PALETTE_BLUE);
    if (havePh) {
      bool below = phVal < PH_MIN; bool above = phVal > PH_MAX;
      bool warn = (!below && !above) && (phVal <= PH_MIN + WARN_MARGIN_PH || phVal >= PH_MAX - WARN_MARGIN_PH);
      base = below || above ? lv_palette_main(LV_PALETTE_RED) : (warn ? lv_palette_main(LV_PALETTE_ORANGE) : lv_palette_main(LV_PALETTE_BLUE));
    }
    lv_obj_set_style_bg_color(lv_card_ph, base, 0);
  }
  if (lv_card_orp) {
    lv_color_t base = lv_palette_main(LV_PALETTE_RED);
    if (haveOrp) {
      int v = (int)lrintf(orpMv);
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

// Modal dialog to change min/max for pH or ORP
static void showRangeDialog(bool isPh){
  const char *title = isPh ? "pH range" : "ORP range";
  lv_obj_t *modal = lv_obj_create(lv_layer_top());
  lv_obj_set_size(modal, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
  lv_obj_set_style_bg_opa(modal, LV_OPA_50, 0);
  lv_obj_set_style_bg_color(modal, lv_color_black(), 0);

  lv_obj_t *dlg = lv_obj_create(modal);
  lv_obj_set_size(dlg, lv_disp_get_hor_res(NULL)-40, 120);
  lv_obj_center(dlg);
  lv_obj_set_style_radius(dlg, 10, 0);
  lv_obj_set_style_pad_all(dlg, 8, 0);

  lv_obj_t *lbl = lv_label_create(dlg); lv_label_set_text(lbl, title); lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, 0);

  // Min
  lv_obj_t *minLbl = lv_label_create(dlg); lv_label_set_text(minLbl, "Min:"); lv_obj_align(minLbl, LV_ALIGN_LEFT_MID, 0, -20);
  lv_obj_t *taMin = lv_textarea_create(dlg); lv_obj_set_width(taMin, 80); lv_obj_align_to(taMin, minLbl, LV_ALIGN_OUT_RIGHT_MID, 6, 0);
  char bmin[16]; if (isPh) snprintf(bmin,sizeof(bmin),"%.2f", PH_MIN); else snprintf(bmin,sizeof(bmin),"%d", ORP_MIN); lv_textarea_set_text(taMin, bmin);

  // Max
  lv_obj_t *maxLbl = lv_label_create(dlg); lv_label_set_text(maxLbl, "Max:"); lv_obj_align(maxLbl, LV_ALIGN_LEFT_MID, 0, 20);
  lv_obj_t *taMax = lv_textarea_create(dlg); lv_obj_set_width(taMax, 80); lv_obj_align_to(taMax, maxLbl, LV_ALIGN_OUT_RIGHT_MID, 6, 0);
  char bmax[16]; if (isPh) snprintf(bmax,sizeof(bmax),"%.2f", PH_MAX); else snprintf(bmax,sizeof(bmax),"%d", ORP_MAX); lv_textarea_set_text(taMax, bmax);

  // Buttons
  lv_obj_t *btnCancel = lv_btn_create(dlg); lv_obj_set_size(btnCancel, 80, 32); lv_obj_align(btnCancel, LV_ALIGN_BOTTOM_LEFT, 0, 0); lv_label_set_text(lv_label_create(btnCancel), "Cancel");
  lv_obj_t *btnSave   = lv_btn_create(dlg); lv_obj_set_size(btnSave, 80, 32); lv_obj_align(btnSave, LV_ALIGN_BOTTOM_RIGHT, 0, 0); lv_label_set_text(lv_label_create(btnSave), "Save");

  struct RangeCtx { bool isPh; lv_obj_t *taMin; lv_obj_t *taMax; lv_obj_t *dlg; };
  RangeCtx *ctx = (RangeCtx*)lv_mem_alloc(sizeof(RangeCtx));
  ctx->isPh = isPh; ctx->taMin = taMin; ctx->taMax = taMax; ctx->dlg = dlg;

  lv_obj_add_event_cb(btnCancel, [](lv_event_t *e){ lv_obj_t *dlg = lv_event_get_target(e)->parent; lv_obj_del(dlg); }, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(btnSave, [](lv_event_t *e){
    RangeCtx *c = (RangeCtx*)lv_event_get_user_data(e);
    const char *smin = lv_textarea_get_text(c->taMin);
    const char *smax = lv_textarea_get_text(c->taMax);
    if (c->isPh) { PH_MIN = atof(smin); PH_MAX = atof(smax); }
    else { ORP_MIN = atoi(smin); ORP_MAX = atoi(smax); }
    prefs.putFloat("ph_min", PH_MIN); prefs.putFloat("ph_max", PH_MAX);
    prefs.putInt("orp_min", ORP_MIN); prefs.putInt("orp_max", ORP_MAX);
    updateLvglValues();
    lv_obj_del(c->dlg);
    lv_mem_free(c);
  }, LV_EVENT_CLICKED, ctx);
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
  int phScaled = havePh ? (int)lrintf(phVal * 100.0f) : INT32_MIN;
  if (phScaled != lastPhScaled) {
    lastPhScaled = phScaled;
    if (phCanvas) {
      phCanvas->fillScreen(BLACK);
      phCanvas->setFont(&FreeSansBold24pt7b);
      uint16_t c = WHITE;
      if (havePh) {
        bool nearMin = phVal <= PH_MIN + WARN_MARGIN_PH;
        bool nearMax = phVal >= PH_MAX - WARN_MARGIN_PH;
        bool below   = phVal < PH_MIN;
        bool above   = phVal > PH_MAX;
        if (below || above) c = RED;
        else if (nearMin || nearMax) c = ORANGE;
      }
      phCanvas->setTextColor(c);
      phCanvas->setCursor(0, 34);
      if (havePh) { char b[16]; snprintf(b,sizeof(b),"%.2f", phVal); phCanvas->print(b); } else { phCanvas->print("--.--"); }
      gfx->draw16bitRGBBitmap(PH_BOX_X, PH_BOX_Y, phCanvas->getBuffer(), PH_BOX_W, PH_BOX_H);
      phCanvas->setFont(nullptr);
    }
  }

  // ORP value box (canvas)
  int orpInt = haveOrp ? (int)lrintf(orpMv) : INT32_MIN;
  if (orpInt != lastOrpInt) {
    lastOrpInt = orpInt;
    if (orpCanvas) {
      orpCanvas->fillScreen(BLACK);
      orpCanvas->setFont(&FreeSansBold24pt7b);
      uint16_t c = WHITE;
      if (haveOrp) {
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
      if (haveOrp) { snprintf(vb,sizeof(vb),"%d", orpInt); orpCanvas->print(vb); } else { strcpy(vb, "----"); orpCanvas->print(vb); }
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
  int tempScaled = haveTemp ? (int)lrintf(tempC * 10.0f) : INT32_MIN;
  if (tempScaled != lastTempScaled) {
    lastTempScaled = tempScaled;
    if (tempCanvas) {
      tempCanvas->fillScreen(BLACK);
      tempCanvas->setFont(&FreeSans12pt7b);
      tempCanvas->setTextColor(WHITE);
      tempCanvas->setCursor(0, 16);
      if (haveTemp) { char b[16]; snprintf(b,sizeof(b),"%.1f\xC2\xB0C", tempC); tempCanvas->print(b); } else { tempCanvas->print("--.-\xC2\xB0C"); }
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

// ===== Touch (AXS5106L) minimal driver =====
static const int TOUCH_SDA = 18;
static const int TOUCH_SCL = 19;
static const int TOUCH_RST = 20;
static const int TOUCH_INT = 21;
static const uint8_t AXS5106L_ADDR = 0x63;
static const uint8_t AXS5106L_ID_REG = 0x08;
static const uint8_t AXS5106L_TOUCH_DATA_REG = 0x01;

static volatile bool touchIrq = false;
static void IRAM_ATTR onTouchInt(){ touchIrq = true; }

static bool i2cWrite8(uint8_t addr, uint8_t reg, const uint8_t* data, uint32_t len){
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (len) Wire.write(data, len);
  return Wire.endTransmission() == 0;
}
static bool i2cRead(uint8_t addr, uint8_t reg, uint8_t* buf, uint32_t len){
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) return false;
  uint32_t got = Wire.requestFrom(addr, (uint8_t)len);
  if (got != len) return false;
  for (uint32_t i=0;i<len;i++) buf[i] = Wire.read();
  return true;
}

struct TouchPoint { int16_t x; int16_t y; bool pressed; };
static bool readTouchOnce(TouchPoint &p){
  p.pressed = false;
  if (!touchIrq) return false; // no new data
  touchIrq = false;
  uint8_t data[14] = {0};
  if (!i2cRead(AXS5106L_ADDR, AXS5106L_TOUCH_DATA_REG, data, sizeof(data))) return false;
  uint8_t n = data[1];
  if (n == 0) return false;
  uint16_t rx = (((uint16_t)(data[2] & 0x0F)) << 8) | data[3];
  uint16_t ry = (((uint16_t)(data[4] & 0x0F)) << 8) | data[5];
  // Our display uses rotation(1): swap x/y compared to raw, per vendor demo
  int16_t x = ry; // swapped
  int16_t y = rx;
  // Clamp to bounds
  x = constrain(x, 0, (int)gfx->width()-1);
  y = constrain(y, 0, (int)gfx->height()-1);
  p.x = x; p.y = y; p.pressed = true; return true;
}

static void beginTouch(){
  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  pinMode(TOUCH_RST, OUTPUT);
  digitalWrite(TOUCH_RST, LOW); delay(50); digitalWrite(TOUCH_RST, HIGH); delay(150);
  pinMode(TOUCH_INT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TOUCH_INT), onTouchInt, FALLING);
  uint8_t id[3] = {0};
  i2cRead(AXS5106L_ADDR, AXS5106L_ID_REG, id, 3);
}

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
  mqtt.publish(TOPIC_CFG_PH_MIN, String(PH_MIN,2).c_str(), true);
  mqtt.publish(TOPIC_CFG_PH_MAX, String(PH_MAX,2).c_str(), true);
  mqtt.publish(TOPIC_CFG_ORP_MIN, String(ORP_MIN).c_str(), true);
  mqtt.publish(TOPIC_CFG_ORP_MAX, String(ORP_MAX).c_str(), true);
  prefs.putFloat("ph_min", PH_MIN);
  prefs.putFloat("ph_max", PH_MAX);
  prefs.putInt("orp_min", ORP_MIN);
  prefs.putInt("orp_max", ORP_MAX);
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
  TouchPoint tp; 
  bool hasTouch = readTouchOnce(tp);
  
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
      prefs.putInt("m1_speed", M1_SPEED_PC);
      prefs.putInt("m2_speed", M2_SPEED_PC);
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
  if (mqtt.connected()) return;
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback([](char* topic, uint8_t* payload, unsigned int length){
    String t(topic);
    String v;
    for (unsigned int i=0;i<length;i++) v += (char)payload[i];
    v.trim();
    float f = v.toFloat();
    if (t == TOPIC_CMD_PH_MIN)  {
      if (!isnan(f)) { PH_MIN = f; prefs.putFloat("ph_min", PH_MIN); mqtt.publish(TOPIC_CFG_PH_MIN, v.c_str(), true); }
    } else if (t == TOPIC_CMD_PH_MAX) {
      if (!isnan(f)) { PH_MAX = f; prefs.putFloat("ph_max", PH_MAX); mqtt.publish(TOPIC_CFG_PH_MAX, v.c_str(), true); }
    } else if (t == TOPIC_CMD_ORP_MIN){
      if (!isnan(f)) { ORP_MIN = (int)lrintf(f); prefs.putInt("orp_min", ORP_MIN); mqtt.publish(TOPIC_CFG_ORP_MIN, String(ORP_MIN).c_str(), true); }
    } else if (t == TOPIC_CMD_ORP_MAX){
      if (!isnan(f)) { ORP_MAX = (int)lrintf(f); prefs.putInt("orp_max", ORP_MAX); mqtt.publish(TOPIC_CFG_ORP_MAX, String(ORP_MAX).c_str(), true); }
    }
  });
  if (mqtt.connect(MQTT_CLIENTID, MQTT_USER, MQTT_PASS)) {
    mqtt.subscribe(TOPIC_CMD_PH_MIN);
    mqtt.subscribe(TOPIC_CMD_PH_MAX);
    mqtt.subscribe(TOPIC_CMD_ORP_MIN);
    mqtt.subscribe(TOPIC_CMD_ORP_MAX);
    // Publish current config as retained
    mqtt.publish(TOPIC_CFG_PH_MIN, String(PH_MIN,2).c_str(), true);
    mqtt.publish(TOPIC_CFG_PH_MAX, String(PH_MAX,2).c_str(), true);
    mqtt.publish(TOPIC_CFG_ORP_MIN, String(ORP_MIN).c_str(), true);
    mqtt.publish(TOPIC_CFG_ORP_MAX, String(ORP_MAX).c_str(), true);
    // LVGL labels refresh via periodic updates; no direct call needed here
  }
}

static void publishDiscoveryOnce() {
  if (mqttAnnounced || !mqtt.connected()) return;
  String ph;   ph.reserve(160);
  ph   = F("{\"name\":\"Pool pH\",\"state_topic\":\"");
  ph  += TOPIC_STATE_PH;
  ph  += F("\",\"unit_of_measurement\":\"pH\",\"unique_id\":\"pool_ph\",\"icon\":\"mdi:beaker-outline\"}");

  String orp;  orp.reserve(160);
  orp  = F("{\"name\":\"Pool ORP\",\"state_topic\":\"");
  orp += TOPIC_STATE_ORP;
  orp += F("\",\"unit_of_measurement\":\"mV\",\"unique_id\":\"pool_orp\",\"icon\":\"mdi:flash\"}");

  String temp; temp.reserve(200);
  temp = F("{\"name\":\"Pool Temp\",\"state_topic\":\"");
  temp+= TOPIC_STATE_TEMP;
  temp+= F("\",\"unit_of_measurement\":\"°C\",\"device_class\":\"temperature\",\"unique_id\":\"pool_temp\"}");
  // Discovery for HA Numbers to adjust thresholds
  String phmin; phmin.reserve(220);
  phmin = F("{\"name\":\"pH min\",\"command_topic\":\""); phmin += TOPIC_CMD_PH_MIN; phmin += F("\",\"state_topic\":\""); phmin += TOPIC_CFG_PH_MIN; phmin += F("\",\"unique_id\":\"pool_cfg_ph_min\",\"min\":0,\"max\":14,\"step\":0.01,\"icon\":\"mdi:arrow-down\"}");

  String phmax; phmax.reserve(220);
  phmax = F("{\"name\":\"pH max\",\"command_topic\":\""); phmax += TOPIC_CMD_PH_MAX; phmax += F("\",\"state_topic\":\""); phmax += TOPIC_CFG_PH_MAX; phmax += F("\",\"unique_id\":\"pool_cfg_ph_max\",\"min\":0,\"max\":14,\"step\":0.01,\"icon\":\"mdi:arrow-up\"}");

  String orpmin; orpmin.reserve(240);
  orpmin = F("{\"name\":\"ORP min\",\"command_topic\":\""); orpmin += TOPIC_CMD_ORP_MIN; orpmin += F("\",\"state_topic\":\""); orpmin += TOPIC_CFG_ORP_MIN; orpmin += F("\",\"unique_id\":\"pool_cfg_orp_min\",\"min\":0,\"max\":3000,\"step\":1,\"unit_of_measurement\":\"mV\"}");

  String orpmax; orpmax.reserve(240);
  orpmax = F("{\"name\":\"ORP max\",\"command_topic\":\""); orpmax += TOPIC_CMD_ORP_MAX; orpmax += F("\",\"state_topic\":\""); orpmax += TOPIC_CFG_ORP_MAX; orpmax += F("\",\"unique_id\":\"pool_cfg_orp_max\",\"min\":0,\"max\":3000,\"step\":1,\"unit_of_measurement\":\"mV\"}");
  mqtt.publish(DISCOVERY_PH, ph.c_str(), true);
  mqtt.publish(DISCOVERY_ORP, orp.c_str(), true);
  mqtt.publish(DISCOVERY_TEMP, temp.c_str(), true);
  // Use separate discovery topics for number entities
  mqtt.publish("homeassistant/number/pool_ph_min/config", phmin.c_str(), true);
  mqtt.publish("homeassistant/number/pool_ph_max/config", phmax.c_str(), true);
  mqtt.publish("homeassistant/number/pool_orp_min/config", orpmin.c_str(), true);
  mqtt.publish("homeassistant/number/pool_orp_max/config", orpmax.c_str(), true);
  mqttAnnounced=true;
}

static void publishStatesIfReady() {
  if (!mqtt.connected()) return;
  static uint32_t lastPub=0; uint32_t now=millis();
  if (now - lastPub < 1000) return; // rate limit
  lastPub = now;
  if (havePh)   { char b[16]; snprintf(b,sizeof(b),"%.2f", phVal); mqtt.publish(TOPIC_STATE_PH, b, true); }
  if (haveOrp)  { char b[16]; snprintf(b,sizeof(b),"%d", (int)lrintf(orpMv)); mqtt.publish(TOPIC_STATE_ORP, b, true); }
  if (haveTemp) { char b[16]; snprintf(b,sizeof(b),"%.1f", tempC); mqtt.publish(TOPIC_STATE_TEMP, b, true); }
}

// ---- Dummy telemetry generator ----
static void updateDummyTelemetry() {
  static bool inited=false;
  static uint32_t last=0; uint32_t now=millis();
  if (!inited) {
    // Seed with hardware RNG if available
    randomSeed((uint32_t)esp_random());
    phVal = (PH_MIN + PH_MAX) * 0.5f; orpMv = (float)ORP_MIN + 50.0f; tempC = 25.0f;
    havePh = haveOrp = haveTemp = true; preferPhPrimary = true;
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
      phVal += phStep;
      if (phVal >= phHighTarget) { phVal = phHighTarget; phState = 1; phPlateauUntil = now + 6000; }
      break;
    case 1: // plateau high (motor zou lopen)
      if ((int32_t)(now - phPlateauUntil) >= 0) { phState = 2; }
      break;
    case 2: // fall
      phVal -= phStep;
      if (phVal <= phLowTarget) { phVal = phLowTarget; phState = 3; phPlateauUntil = now + 4000; }
      break;
    default: // plateau low
      if ((int32_t)(now - phPlateauUntil) >= 0) { phState = 0; }
      break;
  }
  phVal = constrain(phVal, 3.0f, 14.0f);

  // --- ORP state machine: fall → plateau_low → rise → plateau_high → fall ---
  static uint8_t orpState = 0; // 0 fall, 1 plateau_low, 2 rise, 3 plateau_high
  static uint32_t orpPlateauUntil = 0;
  const float orpStep = 4.0f;            // mV per tick
  const float orpLowTarget  = (float)ORP_MIN - 40.0f;  // onder MIN
  const float orpHighTarget = (float)ORP_MIN + 140.0f; // ruim boven MIN
  switch (orpState) {
    case 0: // fall
      orpMv -= orpStep;
      if (orpMv <= orpLowTarget) { orpMv = orpLowTarget; orpState = 1; orpPlateauUntil = now + 6000; }
      break;
    case 1: // plateau low (motor zou lopen)
      if ((int32_t)(now - orpPlateauUntil) >= 0) { orpState = 2; }
      break;
    case 2: // rise
      orpMv += orpStep;
      if (orpMv >= orpHighTarget) { orpMv = orpHighTarget; orpState = 3; orpPlateauUntil = now + 4000; }
      break;
    default: // plateau high
      if ((int32_t)(now - orpPlateauUntil) >= 0) { orpState = 0; }
      break;
  }
  orpMv = constrain(orpMv, -2000.0f, 2000.0f);
  tempC  += (float)random(-3, 4) / 10.0f;      // ±0.3 °C
  tempC   = constrain(tempC, 5.0f, 40.0f);

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
    gfx->print("Temp: "); if (haveTemp) { char b[16]; snprintf(b,sizeof(b),"%.1f",tempC); gfx->print(b); gfx->print(" \xC2\xB0C"); } else { gfx->print("--"); }
    gfx->setCursor(0, 80);
    gfx->print("pH: "); if (havePh) { char b[16]; snprintf(b,sizeof(b),"%.2f",phVal); gfx->print(b); } else { gfx->print("--"); }
    gfx->setCursor(0, 100);
    gfx->print("ORP: "); if (haveOrp) { char b[16]; snprintf(b,sizeof(b),"%.1f",orpMv); gfx->print(b); gfx->print(" mV"); } else { gfx->print("--"); }

    int y = 124; // below header + counters + metrics
    for (auto &s : lines) {
      gfx->setCursor(0, y);
      gfx->println(s);
      y += 20;
    }
  }
}

// ---- Frame parser ----
struct Parser {
  enum { HDR1,HDR2,VER,CMD,LH,LL,DATA,CK } st = HDR1;
  uint8_t ver=0, cmd=0;
  uint16_t len=0, idx=0;
  static const size_t MAX = 600;
  uint8_t buf[MAX];
  const char* tag; // "A" or "B"

  Parser(const char* t): tag(t) {}

  void reset(){ st=HDR1; len=idx=0; }

  void feed(uint8_t b) {
    switch (st) {
      case HDR1:  st = (b==0x55) ? HDR2 : HDR1; break;
      case HDR2:  st = (b==0xAA) ? VER  : HDR1; break;
      case VER:   ver=b; st=CMD; break;
      case CMD:   cmd=b; st=LH; break;
      case LH:    len=((uint16_t)b<<8); st=LL; break;
      case LL:    len|=b; idx=0; st = (len?DATA:CK); break;
      case DATA:  if (idx<MAX) buf[idx++]=b; if (idx>=len) st=CK; break;
      case CK: {
        // Some Tuya variants include 0x55,0xAA in checksum; your capture shows that behavior.
        uint32_t sum = 0;
        sum += 0x55; sum += 0xAA; // include header bytes
        sum += ver; sum += cmd; sum += (uint8_t)(len>>8); sum += (uint8_t)(len&0xFF);
        for (uint16_t i=0; i<len; i++) { sum += buf[i]; }
        uint8_t want = (uint8_t)(sum & 0xFF);
        bool ok = (b==want);

        // Build a compact status line for the display
        String line = String(tag) + " cmd=0x";
        char hc[3]; sprintf(hc, "%02X", cmd); line += hc;
        line += " len=" + String(len) + (ok ? " OK " : " BAD ");

        // Parse and display ALL DPs in the frame (dpid, type, len, value...)
        if (len >= 4) {
          uint16_t p=0;
          bool phSet=false, orpSet=false, tempSet=false;
          while (p + 4 <= len) {
            uint8_t dpid = buf[p++];
            uint8_t dtype = buf[p++];
            uint16_t dl = ((uint16_t)buf[p++]<<8) | buf[p++];
            if (p + dl > len) break; // safety

            String dpline = String(tag) + " DP" + String(dpid) + "/T" + String(dtype) + "=";
            // Update live metrics cache for header (prefer primary DP over ALT)
            auto setMetric = [&](uint8_t id, float value){
              if (id == DP_TEMP && !tempSet) { tempC = value; haveTemp = true; tempSet=true; }
              else if (id == DP_PH) { phVal = value; havePh = true; phSet=true; preferPhPrimary=true; }
              else if (id == DP_PH_ALT1 && !phSet && !preferPhPrimary) { phVal = value; havePh = true; phSet=true; }
              else if (id == DP_ORP && !orpSet) { orpMv = value; haveOrp = true; orpSet=true; }
              else if (id == DP_ORP_ALT1 && !orpSet) { orpMv = value; haveOrp = true; orpSet=true; }
            };
            if (dtype == 0x02 && dl >= 4) { // VALUE (4-byte BE)
              uint32_t v = ((uint32_t)buf[p] << 24) | ((uint32_t)buf[p+1] << 16) | ((uint32_t)buf[p+2] << 8) | buf[p+3];
              dpline += String(v);
              // Derived metrics
              if (dpid == DP_TEMP) { setMetric(dpid, v/10.0f); }
              else if (dpid == DP_PH) { setMetric(dpid, v/100.0f); }
              else if (dpid == DP_ORP) { setMetric(dpid, (int32_t)v*1.0f); }
              else if (dpid == DP_ORP_ALT1) { setMetric(dpid, (int32_t)v*1.0f); }
              else if (dpid == DP_PH_ALT1) { setMetric(dpid, v/100.0f); }
            } else if (dtype == 0x01 && dl >= 1) { // BOOL
              dpline += (buf[p] ? "1" : "0");
            } else if (dtype == 0x04 && dl >= 1) { // ENUM
              dpline += String(buf[p]);
            } else {
              // short hex preview
              for (uint16_t k=0; k<dl && k<6; k++){ dpline += (k?":":""); char hb[3]; sprintf(hb,"%02X",buf[p+k]); dpline += hb; }
            }

            // Serial detail
            Serial.print("  DP"); Serial.print(dpid);
            Serial.print(" T"); Serial.print(dtype);
            Serial.print(" len="); Serial.print(dl);
            Serial.print(" val=");
            if (dtype == 0x02 && dl >= 4) {
              uint32_t v = ((uint32_t)buf[p] << 24) | ((uint32_t)buf[p+1] << 16) | ((uint32_t)buf[p+2] << 8) | buf[p+3];
              Serial.println(v);
            } else if (dtype == 0x01 && dl >= 1) {
              Serial.println(buf[p] ? 1 : 0);
            } else if (dtype == 0x04 && dl >= 1) {
              Serial.println(buf[p]);
            } else {
              for (uint16_t k=0; k<dl && k<12; k++){ if(k) Serial.print(' '); hexByte(Serial, buf[p+k]); } Serial.println();
            }

            pushLine(dpline);
            p += dl;
          }
        }

        // Serial dump (raw & parsed)
        Serial.print("\n["); Serial.print(millis()); Serial.print(" ms] ");
        Serial.print(tag); Serial.print(" FRAME ver=0x"); hexByte(Serial, ver);
        Serial.print(" cmd=0x"); hexByte(Serial, cmd);
        Serial.print(" len="); Serial.print(len);
        Serial.print(" ck="); Serial.println(ok ? "OK" : "BAD");
        if (len) {
          Serial.print("  data:");
          for (uint16_t i=0;i<len;i++){ Serial.print(' '); hexByte(Serial, buf[i]); }
          Serial.println();
        }

        pushLine(line);
        // Update only the value areas to avoid flicker
        updateValueAreas();

        reset();
        break;
      }
    }
  }
};

Parser pa("A"), pb("B");

void setup() {
  // USB serial
  Serial.begin(115200);
  Serial.setTimeout(50);
  unsigned long t0 = millis();
  while (!Serial && (millis() - t0) < 2000) { delay(10); }
  Serial.setDebugOutput(true);
  Serial.println("\nESP32-C6 Tuya Sniffer + Display");

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

  if (USE_LVGL_UI) {
    lv_init();
    // Minimal display buffer and flush bridge using Arduino_GFX
    static lv_color_t disp_buf1[320 * 20];
    static lv_disp_draw_buf_t draw_buf;
    lv_disp_draw_buf_init(&draw_buf, disp_buf1, NULL, 320 * 20);

    // Flush callback
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = gfx->width();
    disp_drv.ver_res = gfx->height();
    disp_drv.flush_cb = [](lv_disp_drv_t *d, const lv_area_t *a, lv_color_t *p){
      int32_t w = (a->x2 - a->x1 + 1);
      int32_t h = (a->y2 - a->y1 + 1);
      // Correct pointer to pixel buffer
#if (LV_COLOR_16_SWAP != 0)
      gfx->draw16bitBeRGBBitmap(a->x1, a->y1, (uint16_t*)p, w, h);
#else
      gfx->draw16bitRGBBitmap(a->x1, a->y1, (uint16_t*)p, w, h);
#endif
      lv_flush_count++;
      lv_disp_flush_ready(d);
    };
    disp_drv.draw_buf = &draw_buf;
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

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
        if (!i2cRead(AXS5106L_ADDR, AXS5106L_TOUCH_DATA_REG, buf, sizeof(buf))) {
          data->state = LV_INDEV_STATE_RELEASED; return;
        }
        uint8_t n = buf[1];
        if (n == 0) { data->state = LV_INDEV_STATE_RELEASED; return; }
        uint16_t rx = (((uint16_t)(buf[2] & 0x0F)) << 8) | buf[3];
        uint16_t ry = (((uint16_t)(buf[4] & 0x0F)) << 8) | buf[5];
        int16_t x = (int16_t)ry;
        int16_t y = (int16_t)rx;
        x = constrain(x, 0, (int)gfx->width()-1);
        y = constrain(y, 0, (int)gfx->height()-1);
        data->point.x = x; data->point.y = y; data->state = LV_INDEV_STATE_PRESSED;
      };
      (void)lv_indev_drv_register(&indev_drv);
    }

    // Helpers are global C-style functions now (lv_update_speed_labels, on_*_cb, updateLvglValues)

    // Build LVGL UI (tileview + dots)
    auto build_lvgl_ui = [=](){
      lv_obj_t *scr = lv_scr_act();
      // overall background: light grey
      lv_obj_set_style_bg_color(scr, lv_palette_lighten(LV_PALETTE_GREY, 3), 0);
      lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
      lv_obj_set_style_text_color(scr, lv_color_black(), 0);
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
        updateLvglValues();
        return;
      }
      // overall background: light grey
      lv_obj_set_style_bg_color(scr, lv_palette_lighten(LV_PALETTE_GREY, 3), 0);
      lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
      lv_obj_set_style_text_color(scr, lv_color_white(), 0);
      lv_obj_set_style_pad_bottom(scr, 16, 0); // space for dots below frame

      // (moved lower) create debug label after frame to ensure foreground

      // Fullscreen root container
      lv_obj_t *frame = lv_obj_create(scr);
      lv_obj_set_size(frame, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
      lv_obj_set_pos(frame, 0, 0);
      lv_obj_set_style_border_width(frame, 0, 0);
      lv_obj_set_style_radius(frame, 0, 0);
      lv_obj_set_style_bg_opa(frame, LV_OPA_TRANSP, 0);

      // Tileview with 2 pages (main, settings)
      lv_tv = lv_tileview_create(frame);
      lv_obj_set_size(lv_tv, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
      lv_obj_set_pos(lv_tv, 0, 0);
      lv_obj_set_scroll_dir(lv_tv, LV_DIR_HOR);
      lv_obj_set_scroll_snap_x(lv_tv, LV_SCROLL_SNAP_CENTER);
      lv_obj_set_style_bg_opa(lv_tv, LV_OPA_TRANSP, 0);
      // Keep scrolling enabled for swipe; just hide scrollbars
      lv_obj_set_scrollbar_mode(lv_tv, LV_SCROLLBAR_MODE_OFF);
      lv_obj_set_style_text_color(lv_tv, lv_color_white(), 0);

      lv_tile_main = lv_tileview_add_tile(lv_tv, 0, 0, LV_DIR_HOR);
      lv_tile_settings = lv_tileview_add_tile(lv_tv, 1, 0, LV_DIR_HOR);
      lv_obj_set_style_bg_opa(lv_tile_main, LV_OPA_TRANSP, 0);
      lv_obj_clear_flag(lv_tile_main, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_scrollbar_mode(lv_tile_main, LV_SCROLLBAR_MODE_OFF);
      lv_obj_set_style_text_color(lv_tile_main, lv_color_white(), 0);
      lv_obj_set_style_bg_opa(lv_tile_settings, LV_OPA_TRANSP, 0);
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
      lv_obj_align(content, LV_ALIGN_TOP_MID, 0, 6);
      lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_OFF);
      static lv_style_t st_card; static bool st_inited=false; if(!st_inited){ st_inited=true; lv_style_init(&st_card); lv_style_set_radius(&st_card, 12); lv_style_set_bg_opa(&st_card, LV_OPA_COVER); lv_style_set_bg_grad_dir(&st_card, LV_GRAD_DIR_VER); lv_style_set_shadow_width(&st_card, 10); lv_style_set_shadow_opa(&st_card, LV_OPA_30); lv_style_set_shadow_ofs_y(&st_card, 4); lv_style_set_pad_all(&st_card, 10); }
      auto make_tile = [&](bool left, lv_color_t c1, lv_color_t c2, const char *title){ lv_obj_t *tile = lv_btn_create(content); lv_obj_remove_style_all(tile); lv_obj_add_style(tile, &st_card, 0); lv_obj_set_style_bg_color(tile, c1, 0); lv_obj_set_style_bg_grad_color(tile, c2, 0); int tw = (content_w - gap)/2; int th = content_h - 4; if (th < 60) th = 60; lv_obj_set_size(tile, tw, th); lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE); lv_obj_set_scrollbar_mode(tile, LV_SCROLLBAR_MODE_OFF); if(left) lv_obj_align(tile, LV_ALIGN_LEFT_MID, -4, 0); else lv_obj_align(tile, LV_ALIGN_RIGHT_MID, 4, 0); 
        // title small bottom-left
        lv_obj_t *lbl = lv_label_create(tile); lv_obj_set_style_text_color(lbl, lv_color_white(), 0); lv_label_set_text(lbl, title); lv_obj_align(lbl, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        // icon small top-left
        lv_obj_t *icon = lv_obj_create(tile); lv_obj_remove_style_all(icon); lv_obj_set_size(icon, 10, 10); lv_obj_align(icon, LV_ALIGN_TOP_LEFT, 0, 0); lv_obj_set_style_radius(icon, 5, 0); lv_obj_set_style_border_width(icon, 2, 0); lv_obj_set_style_border_color(icon, lv_color_white(), 0); lv_obj_set_style_bg_opa(icon, LV_OPA_TRANSP, 0);
        return tile; };
      lv_obj_t *ph_tile   = make_tile(true,  lv_palette_main(LV_PALETTE_INDIGO), lv_palette_darken(LV_PALETTE_INDIGO, 2), "pH");
      lv_obj_t *orp_tile  = make_tile(false, lv_palette_main(LV_PALETTE_GREEN),  lv_palette_darken(LV_PALETTE_GREEN, 2),  "ORP");

      // value labels inside tiles (centered)
      if (ph_tile) { lv_lbl_ph = lv_label_create(ph_tile); lv_obj_set_style_text_color(lv_lbl_ph, lv_color_white(), 0); lv_obj_set_style_text_font(lv_lbl_ph, &lv_font_montserrat_28, 0); lv_obj_align(lv_lbl_ph, LV_ALIGN_CENTER, 0, 0); lv_label_set_text(lv_lbl_ph, "--.--"); }
      if (orp_tile){
        lv_lbl_orp = lv_label_create(orp_tile);
        lv_obj_set_style_text_color(lv_lbl_orp, lv_color_white(), 0);
        lv_obj_set_style_text_font(lv_lbl_orp, &lv_font_montserrat_28, 0);
        lv_obj_align(lv_lbl_orp, LV_ALIGN_CENTER, -10, 0);
        lv_label_set_text(lv_lbl_orp, "----");
        // small unit label "mV"
        lv_lbl_orp_unit = lv_label_create(orp_tile);
        lv_obj_set_style_text_color(lv_lbl_orp_unit, lv_color_white(), 0);
        lv_obj_set_style_text_font(lv_lbl_orp_unit, &lv_font_montserrat_14, 0);
        lv_label_set_text(lv_lbl_orp_unit, " mV");
        lv_obj_align_to(lv_lbl_orp_unit, lv_lbl_orp, LV_ALIGN_OUT_RIGHT_MID, 4, 2);
      }
      // attach tap -> range dialog
      if (ph_tile)  lv_obj_add_event_cb(ph_tile, [](lv_event_t *e){ if(lv_event_get_code(e)==LV_EVENT_CLICKED) showRangeDialog(true); }, LV_EVENT_ALL, NULL);
      if (orp_tile) lv_obj_add_event_cb(orp_tile, [](lv_event_t *e){ if(lv_event_get_code(e)==LV_EVENT_CLICKED) showRangeDialog(false); }, LV_EVENT_ALL, NULL);

      // Footer IP at bottom-right (always show)
      lv_lbl_ip = lv_label_create(lv_tile_main); lv_obj_set_style_text_color(lv_lbl_ip, lv_palette_darken(LV_PALETTE_GREY, 4), 0); lv_obj_set_style_text_font(lv_lbl_ip, &lv_font_montserrat_14, 0); lv_label_set_long_mode(lv_lbl_ip, LV_LABEL_LONG_CLIP); lv_obj_set_width(lv_lbl_ip, LV_SIZE_CONTENT); lv_obj_set_style_text_align(lv_lbl_ip, LV_TEXT_ALIGN_RIGHT, 0); lv_obj_align(lv_lbl_ip, LV_ALIGN_BOTTOM_RIGHT, -2, -1); lv_label_set_text(lv_lbl_ip, "IP: --");

      lv_lbl_m1 = lv_label_create(lv_tile_main);
      lv_label_set_text(lv_lbl_m1, "M1");
      lv_obj_set_style_text_color(lv_lbl_m1, lv_color_white(), 0);
      lv_obj_align(lv_lbl_m1, LV_ALIGN_TOP_LEFT, 10, 40);
      lv_obj_add_flag(lv_lbl_m1, LV_OBJ_FLAG_HIDDEN);

      lv_lbl_m2 = lv_label_create(lv_tile_main);
      lv_label_set_text(lv_lbl_m2, "M2");
      lv_obj_set_style_text_color(lv_lbl_m2, lv_color_white(), 0);
      lv_obj_align(lv_lbl_m2, LV_ALIGN_TOP_RIGHT, -10, 40);
      lv_obj_add_flag(lv_lbl_m2, LV_OBJ_FLAG_HIDDEN);

      updateLvglValues();

      // Settings tile content: motor speed controls (clean styling)
      lv_obj_t *row1 = lv_obj_create(lv_tile_settings);
      lv_obj_remove_style_all(row1);
      lv_obj_set_size(row1, lv_obj_get_width(lv_tile_settings)-20, 40);
      lv_obj_align(row1, LV_ALIGN_TOP_MID, 0, 16);
      lv_obj_set_style_bg_color(row1, lv_palette_darken(LV_PALETTE_GREY,2), 0);
      lv_obj_set_style_bg_opa(row1, LV_OPA_30, 0);
      lv_obj_set_style_pad_all(row1, 6, 0);

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
      lv_obj_t *lbl2 = lv_label_create(row2); lv_label_set_text(lbl2, "ORP Motor Speed"); lv_obj_align(lbl2, LV_ALIGN_LEFT_MID, 0, 0);
      lv_obj_t *btn2m = lv_btn_create(row2); lv_obj_set_size(btn2m, 36, 32); lv_obj_align(btn2m, LV_ALIGN_RIGHT_MID, -120, 0); lv_obj_add_event_cb(btn2m, on_orp_minus_cb, LV_EVENT_CLICKED, NULL); lv_label_set_text(lv_label_create(btn2m), "-");
      lv_lbl_speed2 = lv_label_create(row2); lv_label_set_text(lv_lbl_speed2, "--%"); lv_obj_align(lv_lbl_speed2, LV_ALIGN_RIGHT_MID, -70, 0);
      lv_obj_t *btn2p = lv_btn_create(row2); lv_obj_set_size(btn2p, 36, 32); lv_obj_align(btn2p, LV_ALIGN_RIGHT_MID, -20, 0); lv_obj_add_event_cb(btn2p, on_orp_plus_cb, LV_EVENT_CLICKED, NULL); lv_label_set_text(lv_label_create(btn2p), "+");

      // Save button
      lv_obj_t *btnSave = lv_btn_create(lv_tile_settings); lv_obj_set_size(btnSave, 100, 34); lv_obj_align(btnSave, LV_ALIGN_BOTTOM_MID, 0, -16); lv_obj_add_event_cb(btnSave, on_speed_save_cb, LV_EVENT_CLICKED, NULL); lv_label_set_text(lv_label_create(btnSave), "Save");
      lv_update_speed_labels();

      // Pagination dots removed to simplify and avoid event-related issues
    };
    build_lvgl_ui();
  }

  if (!DIAG_MODE) {
    // UARTs (RX only) unless TX pins are provided
    TUYA_A.begin(TUYA_BAUD, SERIAL_8N1, RX_A_PIN, TX_A_PIN);
    if (USE_CHANNEL_B) TUYA_B.begin(TUYA_BAUD, SERIAL_8N1, RX_B_PIN, TX_B_PIN);
  }

  pushLine("Ready. Waiting for frames...");
  if (!USE_LVGL_UI) {
  drawStaticUI();
  updateValueAreas();
  }

  // WiFi + MQTT
  setupWiFiEvents();
  connectWiFiIfNeeded();

  // Load persisted thresholds
  prefs.begin("poolcfg", false);
  PH_MIN = prefs.getFloat("ph_min", PH_MIN);
  PH_MAX = prefs.getFloat("ph_max", PH_MAX);
  ORP_MIN = prefs.getInt("orp_min", ORP_MIN);
  ORP_MAX = prefs.getInt("orp_max", ORP_MAX);

  // Begin touch
  beginTouch();

  // Load custom speeds if present
  M1_SPEED_PC = (uint8_t)prefs.getInt("m1_speed", M1_SPEED_PC);
  M2_SPEED_PC = (uint8_t)prefs.getInt("m2_speed", M2_SPEED_PC);

  // TB6612 pins
  if (MOTOR_ENABLE) {
    pinMode(TB_STBY, OUTPUT);
    pinMode(M1_IN1, OUTPUT); pinMode(M1_IN2, OUTPUT);
    pinMode(M2_IN1, OUTPUT); pinMode(M2_IN2, OUTPUT);
    digitalWrite(TB_STBY, HIGH);
    // PWM setup via Arduino analogWrite for ESP32-C6
    // Configure frequency first, then resolution per pin
    analogWriteFrequency(M1_PWM, PWM_FREQ);
    analogWriteFrequency(M2_PWM, PWM_FREQ);
    analogWriteResolution(M1_PWM, PWM_BITS);
    analogWriteResolution(M2_PWM, PWM_BITS);
    pinMode(M1_PWM, OUTPUT); analogWrite(M1_PWM, 0);
    pinMode(M2_PWM, OUTPUT); analogWrite(M2_PWM, 0);

    if (MOTOR_TEST && !FORCE_MOTOR_A_ON) {
      uint8_t duty = (uint8_t)(M1_SPEED_PC * 255 / 100);
      // M1 forward
      digitalWrite(M1_IN1, HIGH); digitalWrite(M1_IN2, LOW);
      analogWrite(M1_PWM, duty); delay(1000); analogWrite(M1_PWM, 0); delay(300);
      // M1 reverse
      digitalWrite(M1_IN1, LOW); digitalWrite(M1_IN2, HIGH);
      analogWrite(M1_PWM, duty); delay(1000); analogWrite(M1_PWM, 0); delay(500);
      // M2 forward
      digitalWrite(M2_IN1, HIGH); digitalWrite(M2_IN2, LOW);
      analogWrite(M2_PWM, duty); delay(1000); analogWrite(M2_PWM, 0); delay(300);
      // M2 reverse
      digitalWrite(M2_IN1, LOW); digitalWrite(M2_IN2, HIGH);
      analogWrite(M2_PWM, duty); delay(1000); analogWrite(M2_PWM, 0);
    }

    // Hard force Motor A on continuously for test (AIN1=LOW, AIN2=HIGH, 100% duty)
    if (FORCE_MOTOR_A_ON) {
      digitalWrite(M1_IN1, LOW);
      digitalWrite(M1_IN2, HIGH);
      analogWrite(M1_PWM, 255);
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
    delay(5);
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

  while (TUYA_A.available()) {
    uint8_t b = TUYA_A.read();
    rxA_count++;
    lastA[idxA] = b; idxA = (uint8_t)((idxA + 1) % 7);
    pa.feed(b);
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
  if (USE_CHANNEL_B && !USE_LVGL_UI) {
    while (TUYA_B.available()) {
      uint8_t b = TUYA_B.read();
      rxB_count++;
      lastB[idxB] = b; idxB = (uint8_t)((idxB + 1) % 7);
      pb.feed(b);
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
  static uint32_t lastConnect=0; uint32_t now=millis();
  if (WiFi.status() != WL_CONNECTED && now - lastConnect > 2000) { lastConnect = now; connectWiFiIfNeeded(); }
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqtt.connected() && now - lastConnect > 2000) { lastConnect = now; ensureMqtt(); }
    if (mqtt.connected()) { publishDiscoveryOnce(); publishStatesIfReady(); mqtt.loop(); }
  }

  if (!USE_LVGL_UI) {
    // Touch handling
    handleTouchUI();
  }

  // Motor control policy (skip if forced-on test is active)
  if (MOTOR_ENABLE && !FORCE_MOTOR_A_ON) {
    // CONTINUOUS POLICY with hysteresis: run while beyond threshold, stop when back in band by hysteresis
    // Motor 1: pH high → dose acid (direction B)
    bool phHigh = havePh && phVal > PH_MAX;
    bool phBack = havePh && phVal < (PH_MAX - PH_HYST);
    if (phHigh) {
      bool dirA = false;
      digitalWrite(M1_IN1, dirA ? HIGH : LOW);
      digitalWrite(M1_IN2, dirA ? LOW  : HIGH);
      uint8_t duty = (uint8_t)(M2_SPEED_PC * 255 / 100);
      analogWrite(M1_PWM, duty);
      if (!m1Running) { m1Running = true; if (!USE_LVGL_UI) drawMotorIcon(gfx, M1_ICON_X, M1_ICON_Y, true); else updateLvglValues(); }
    } else if (phBack) {
      analogWrite(M1_PWM, 0);
      if (m1Running) { m1Running = false; if (!USE_LVGL_UI) drawMotorIcon(gfx, M1_ICON_X, M1_ICON_Y, false); else updateLvglValues(); }
    }

    // Motor 2: ORP low → dose (direction A)
    int orpIntNow = (int)lrintf(orpMv);
    bool orpLow = haveOrp && (orpIntNow < ORP_MIN);
    bool orpBack = haveOrp && (orpIntNow > (ORP_MIN + ORP_HYST));
    if (orpLow) {
      bool dirA = true;
      digitalWrite(M2_IN1, dirA ? HIGH : LOW);
      digitalWrite(M2_IN2, dirA ? LOW  : HIGH);
      uint8_t duty = (uint8_t)(M2_SPEED_PC * 255 / 100);
      analogWrite(M2_PWM, duty);
      if (!m2Running) { m2Running = true; if (!USE_LVGL_UI) drawMotorIcon(gfx, M2_ICON_X, M2_ICON_Y, true); else updateLvglValues(); }
    } else if (orpBack) {
      analogWrite(M2_PWM, 0);
      if (m2Running) { m2Running = false; if (!USE_LVGL_UI) drawMotorIcon(gfx, M2_ICON_X, M2_ICON_Y, false); else updateLvglValues(); }
    }
  }
}
