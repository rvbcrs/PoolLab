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
  if (_prefs.putString("mqtt_host", host) > 0) {
    ESP_LOGI("Storage", "Wrote MQTT Host successfully");
  } else {
    ESP_LOGE("Storage", "Failed to write MQTT Host");
  }
}
void Storage::setMqttPort(uint16_t port) {
  if (_prefs.putUShort("mqtt_port", port) > 0) {
    ESP_LOGI("Storage", "Wrote MQTT Port successfully");
  } else {
    ESP_LOGE("Storage", "Failed to write MQTT Port");
  }
}
void Storage::setMqttUser(const String &user) {
  if (_prefs.putString("mqtt_user", user) > 0) {
    ESP_LOGI("Storage", "Wrote MQTT User successfully");
  } else {
    ESP_LOGE("Storage", "Failed to write MQTT User");
  }
}
void Storage::setMqttPass(const String &pass) {
  if (_prefs.putString("mqtt_pass", pass) > 0) {
    ESP_LOGI("Storage", "Wrote MQTT Password successfully");
  } else {
    ESP_LOGE("Storage", "Failed to write MQTT Password");
  }
}

} // namespace core


