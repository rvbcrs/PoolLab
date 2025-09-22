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
void Storage::setWifiSsid(const String &v) {
  _prefs.putString("wifi_ssid", v);
}
void Storage::setWifiPass(const String &v) {
  _prefs.putString("wifi_pass", v);
}

String Storage::getMqttHost(const String &def) const { return _prefs.getString("mqtt_host", def); }
uint16_t Storage::getMqttPort(uint16_t def) const { return (uint16_t)_prefs.getUInt("mqtt_port", def); }
String Storage::getMqttUser(const String &def) const { return _prefs.getString("mqtt_user", def); }
String Storage::getMqttPass(const String &def) const { return _prefs.getString("mqtt_pass", def); }
void Storage::setMqttHost(const String &v) { _prefs.putString("mqtt_host", v); }
void Storage::setMqttPort(uint16_t v) { _prefs.putUInt("mqtt_port", v); }
void Storage::setMqttUser(const String &v) { _prefs.putString("mqtt_user", v); }
void Storage::setMqttPass(const String &v) { _prefs.putString("mqtt_pass", v); }

} // namespace core


