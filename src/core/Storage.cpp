#include "Storage.h"

namespace core {

Storage::Storage(const char* ns): _ns(ns) {}

bool Storage::begin(bool readOnly) {
  return _prefs.begin(_ns.c_str(), readOnly);
}

float Storage::getPhMin(float def) const { return _prefs.getFloat("ph_min", def); }
float Storage::getPhMax(float def) const { return _prefs.getFloat("ph_max", def); }
int   Storage::getOrpMin(int def) const { return _prefs.getInt("orp_min", def); }
int   Storage::getOrpMax(int def) const { return _prefs.getInt("orp_max", def); }

void  Storage::setPhMin(float v) { _prefs.putFloat("ph_min", v); }
void  Storage::setPhMax(float v) { _prefs.putFloat("ph_max", v); }
void  Storage::setOrpMin(int v) { _prefs.putInt("orp_min", v); }
void  Storage::setOrpMax(int v) { _prefs.putInt("orp_max", v); }

int Storage::getM1Speed(int def) const { return _prefs.getInt("m1_speed", def); }
int Storage::getM2Speed(int def) const { return _prefs.getInt("m2_speed", def); }
void Storage::setM1Speed(int v) { _prefs.putInt("m1_speed", v); }
void Storage::setM2Speed(int v) { _prefs.putInt("m2_speed", v); }

float Storage::getM1FlowRate(float def) const { return _prefs.getFloat("m1_flow", def); }
float Storage::getM2FlowRate(float def) const { return _prefs.getFloat("m2_flow", def); }
void Storage::setM1FlowRate(float v) { _prefs.putFloat("m1_flow", v); }
void Storage::setM2FlowRate(float v) { _prefs.putFloat("m2_flow", v); }

float Storage::getM1TotalVolume(float def) const { return _prefs.getFloat("m1_total", def); }
float Storage::getM2TotalVolume(float def) const { return _prefs.getFloat("m2_total", def); }
void Storage::setM1TotalVolume(float v) { _prefs.putFloat("m1_total", v); }
void Storage::setM2TotalVolume(float v) { _prefs.putFloat("m2_total", v); }

float Storage::getM1DailyVolume(float def) const { return _prefs.getFloat("m1_daily", def); }
float Storage::getM2DailyVolume(float def) const { return _prefs.getFloat("m2_daily", def); }
void Storage::setM1DailyVolume(float v) { _prefs.putFloat("m1_daily", v); }
void Storage::setM2DailyVolume(float v) { _prefs.putFloat("m2_daily", v); }

int Storage::getLastResetDay(int def) const { return _prefs.getInt("last_day", def); }
void Storage::setLastResetDay(int v) { _prefs.putInt("last_day", v); }

bool Storage::getMotorsEnabled(bool def) const {
  return _prefs.getBool("motors_en", def);
}
void Storage::setMotorsEnabled(bool en) {
  _prefs.putBool("motors_en", en);
}

Storage::Mode Storage::getMode(Mode def) const {
  int v = _prefs.getInt("mode", static_cast<int>(def));
  return (v == static_cast<int>(MODE_ZIGBEE)) ? MODE_ZIGBEE : MODE_WIFI_MQTT;
}
void Storage::setMode(Mode m) {
  _prefs.putInt("mode", static_cast<int>(m));
}

String Storage::getWifiSsid(const String &def) const {
  return _prefs.getString("wifi_ssid", def);
}
String Storage::getWifiPass(const String &def) const {
  return _prefs.getString("wifi_pass", def);
}
void Storage::setWifiSsid(const String &ssid) {
  size_t written = _prefs.putString("wifi_ssid", ssid);
  if (ssid.length() == 0) {
    ESP_LOGI("Storage", "Cleared WiFi SSID");
  } else if (written > 0) {
    ESP_LOGI("Storage", "Wrote WiFi SSID successfully");
  } else {
    ESP_LOGW("Storage", "WiFi SSID unchanged");
  }
}
void Storage::setWifiPass(const String &pass) {
  size_t written = _prefs.putString("wifi_pass", pass);
  if (pass.length() == 0) {
    ESP_LOGI("Storage", "Cleared WiFi Password");
  } else if (written > 0) {
    ESP_LOGI("Storage", "Wrote WiFi Password successfully");
  } else {
    ESP_LOGW("Storage", "WiFi Password unchanged");
  }
}

String Storage::getMqttHost(const String &def) const { return _prefs.getString("mqtt_host", def); }
uint16_t Storage::getMqttPort(uint16_t def) const { return _prefs.getUShort("mqtt_port", def); }
String Storage::getMqttUser(const String &def) const { return _prefs.getString("mqtt_user", def); }
String Storage::getMqttPass(const String &def) const { return _prefs.getString("mqtt_pass", def); }
void Storage::setMqttHost(const String &host) {
  _prefs.putString("mqtt_host", host);
}
void Storage::setMqttPort(uint16_t port) {
  _prefs.putUShort("mqtt_port", port);
}
void Storage::setMqttUser(const String &user) {
  _prefs.putString("mqtt_user", user);
}
void Storage::setMqttPass(const String &pass) {
  _prefs.putString("mqtt_pass", pass);
}

// ---- Analog sensor calibration ----
float Storage::getPhVAt4(float def) const { return _prefs.getFloat("ph_v_at4", def); }
float Storage::getPhVAt10(float def) const { return _prefs.getFloat("ph_v_at10", def); }
void  Storage::setPhVAt4(float v) { _prefs.putFloat("ph_v_at4", v); }
void  Storage::setPhVAt10(float v) { _prefs.putFloat("ph_v_at10", v); }
float Storage::getOrpVAt0(float def) const { return _prefs.getFloat("orp_v_at0", def); }
float Storage::getOrpMvPerV(float def) const { return _prefs.getFloat("orp_mv_per_v", def); }
void  Storage::setOrpVAt0(float v) { _prefs.putFloat("orp_v_at0", v); }
void  Storage::setOrpMvPerV(float v) { _prefs.putFloat("orp_mv_per_v", v); }

// ---- Safety limits ----
float Storage::getMaxDailyVolume(float def) const { return _prefs.getFloat("max_daily", def); }
void Storage::setMaxDailyVolume(float v) { _prefs.putFloat("max_daily", v); }
float Storage::getMaxSessionVolume(float def) const { return _prefs.getFloat("max_session", def); }
void Storage::setMaxSessionVolume(float v) { _prefs.putFloat("max_session", v); }
int Storage::getMaxSessionDuration(int def) const { return _prefs.getInt("max_dur", def); }
void Storage::setMaxSessionDuration(int v) { _prefs.putInt("max_dur", v); }
float Storage::getPhSanityMin(float def) const { return _prefs.getFloat("ph_sane_min", def); }
float Storage::getPhSanityMax(float def) const { return _prefs.getFloat("ph_sane_max", def); }
void Storage::setPhSanityMin(float v) { _prefs.putFloat("ph_sane_min", v); }
void Storage::setPhSanityMax(float v) { _prefs.putFloat("ph_sane_max", v); }
int Storage::getOrpSanityMin(int def) const { return _prefs.getInt("orp_sane_min", def); }
int Storage::getOrpSanityMax(int def) const { return _prefs.getInt("orp_sane_max", def); }
void Storage::setOrpSanityMin(int v) { _prefs.putInt("orp_sane_min", v); }
void Storage::setOrpSanityMax(int v) { _prefs.putInt("orp_sane_max", v); }
int Storage::getSensorTimeout(int def) const { return _prefs.getInt("sens_timeout", def); }
void Storage::setSensorTimeout(int v) { _prefs.putInt("sens_timeout", v); }

// ---- WhatsApp notifications ----
String Storage::getWhatsAppPhone(const String &def) const { return _prefs.getString("wa_phone", def); }
void Storage::setWhatsAppPhone(const String &v) { _prefs.putString("wa_phone", v); }
bool Storage::getWhatsAppEnabled(bool def) const { return _prefs.getBool("wa_enabled", def); }
void Storage::setWhatsAppEnabled(bool en) { _prefs.putBool("wa_enabled", en); }

// Audio
bool Storage::getSoundEnabled(bool def) const {
  return _prefs.getBool("snd_en", def);
}
void Storage::setSoundEnabled(bool en) {
  _prefs.putBool("snd_en", en);
}

} // namespace core


