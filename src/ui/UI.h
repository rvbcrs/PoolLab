#pragma once

#include <lvgl.h>

// Icons used by UI (provided by src/images/*.c)
#ifdef __cplusplus
extern "C" {
#endif
extern const lv_img_dsc_t water_ph_32dp_E3E3E3_FILL0_wght400_GRAD0_opsz40;
extern const lv_img_dsc_t water_orp_32dp_E3E3E3_FILL0_wght400_GRAD0_opsz40;
extern const lv_img_dsc_t device_thermostat_32dp_999999_FILL0_wght400_GRAD0_opsz40;
extern const lv_img_dsc_t water_pump_24dp_E3E3E3_FILL0_wght400_GRAD0_opsz24;
#ifdef __cplusplus
}
#endif

namespace ui {

void init(lv_disp_t* disp);
void build(bool safeBaseline);
void updateValues();
void setIp(const char *ipText);
void setSsid(const char *ssid);
void showSettings();
void showMain();
void setSavedWifi(const char *ssid, const char *pass);
void setSavedMqtt(const char *host, uint16_t port, const char *user, const char *pass);
void setThresholds(float phMin, float phMax, int orpMin, int orpMax);
void setPumpActive(bool phActive, bool orpActive);

// New: extracted dialogs/helpers
void showRangeEditor(bool isPh);
void showCommissioning(uint32_t seconds);
void showHoldToPair();

struct Handlers {
  void (*onSpeedChange)(int idx, int value) = nullptr; // idx: 1=pH motor, 2=ORP motor
  void (*onModeToggle)(bool zigbee) = nullptr;         // true -> Zigbee mode, false -> WiFi/MQTT
  void (*onSettings)() = nullptr;                      // open in-app settings
  void (*onWifiReset)() = nullptr;                     // clear wifi creds, start portal
  void (*onWifiSave)(const char *ssid, const char *pass) = nullptr; // save wifi creds
  void (*onMqttSave)(const char *host, uint16_t port, const char *user, const char *pass) = nullptr; // save MQTT broker
  void (*onSaveSettings)() = nullptr;                  // new: after saving, navigate/notify
  void (*onCancelSettings)() = nullptr;                // new: cancel and return
};

void configureHandlers(const Handlers &h);
void setInitialSpeeds(uint8_t m1, uint8_t m2);
void setInitialMode(bool zigbee);

// Calibration UI API
void showPhCalibration();
void showOrpCalibration();
// Populate saved calibration values in settings (optional helpers)
void setSavedPhCalibration(float v_at4, float v_at10);
void setSavedOrpCalibration(float v_at0, float mv_per_v);

} // namespace ui


