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

private:
  String _ns;
  mutable Preferences _prefs; // lazily opened in begin()
};

} // namespace core


