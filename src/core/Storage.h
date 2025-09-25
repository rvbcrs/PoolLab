// Lightweight wrapper around Preferences (NVS) for app configuration
#pragma once

#include <Arduino.h>
#include <Preferences.h>

namespace core {

class Storage {
public:
  explicit Storage(const char* ns = "poolcfg");
  bool begin(bool readOnly = false);

  // Thresholds
  float getPhMin(float def) const;
  float getPhMax(float def) const;
  int   getOrpMin(int def) const;
  int   getOrpMax(int def) const;

  void  setPhMin(float v);
  void  setPhMax(float v);
  void  setOrpMin(int v);
  void  setOrpMax(int v);

  // Motor speeds
  int getM1Speed(int def) const;
  int getM2Speed(int def) const;
  void setM1Speed(int v);
  void setM2Speed(int v);

  // Connectivity mode
  enum Mode { MODE_WIFI_MQTT = 0, MODE_ZIGBEE = 1 };
  Mode getMode(Mode def) const;
  void setMode(Mode m);

  // WiFi credentials
  String getWifiSsid(const String &def = "") const;
  String getWifiPass(const String &def = "") const;
  void setWifiSsid(const String &v);
  void setWifiPass(const String &v);

  // MQTT/Home Assistant broker
  String getMqttHost(const String &def = "") const;
  uint16_t getMqttPort(uint16_t def = 1883) const;
  String getMqttUser(const String &def = "") const;
  String getMqttPass(const String &def = "") const;
  void setMqttHost(const String &v);
  void setMqttPort(uint16_t v);
  void setMqttUser(const String &v);
  void setMqttPass(const String &v);

  // Analog sensor calibration (persisted)
  // pH: two-point calibration (volts at pH4 and pH10)
  float getPhVAt4(float def) const;
  float getPhVAt10(float def) const;
  void  setPhVAt4(float v);
  void  setPhVAt10(float v);
  // ORP: center voltage at 0 mV and scale (mV per Volt)
  float getOrpVAt0(float def) const;
  float getOrpMvPerV(float def) const;
  void  setOrpVAt0(float v);
  void  setOrpMvPerV(float v);

private:
  String _ns;
  mutable Preferences _prefs; // lazily opened in begin()
};

} // namespace core


