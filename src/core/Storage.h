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
  
  // Pump flow rates (ml/min at 100% speed)
  float getM1FlowRate(float def) const;
  float getM2FlowRate(float def) const;
  void setM1FlowRate(float v);
  void setM2FlowRate(float v);
  
  // Pump total volumes (ml)
  float getM1TotalVolume(float def) const;
  float getM2TotalVolume(float def) const;
  void setM1TotalVolume(float v);
  void setM2TotalVolume(float v);
  
  // Pump daily volumes (ml)
  float getM1DailyVolume(float def) const;
  float getM2DailyVolume(float def) const;
  void setM1DailyVolume(float v);
  void setM2DailyVolume(float v);
  
  // Last reset date (day of year)
  int getLastResetDay(int def) const;
  void setLastResetDay(int v);

  // Motors enabled flag
  bool getMotorsEnabled(bool def) const;
  void setMotorsEnabled(bool en);

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

  // Safety limits
  float getMaxDailyVolume(float def) const;      // ml per day per pump
  void setMaxDailyVolume(float v);
  float getMaxSessionVolume(float def) const;    // ml per session
  void setMaxSessionVolume(float v);
  int getMaxSessionDuration(int def) const;      // seconds
  void setMaxSessionDuration(int v);
  float getPhSanityMin(float def) const;         // sensor sanity check
  float getPhSanityMax(float def) const;
  void setPhSanityMin(float v);
  void setPhSanityMax(float v);
  int getOrpSanityMin(int def) const;            // mV
  int getOrpSanityMax(int def) const;
  void setOrpSanityMin(int v);
  void setOrpSanityMax(int v);
  int getSensorTimeout(int def) const;           // seconds
  void setSensorTimeout(int v);

  // WhatsApp notifications (CallMeBot)
  String getWhatsAppPhone(const String &def = "") const;  // +31612345678 format
  void setWhatsAppPhone(const String &v);
  bool getWhatsAppEnabled(bool def = false) const;
  void setWhatsAppEnabled(bool en);
  String getCallMeBotApiKey(const String &def = "") const;
  void setCallMeBotApiKey(const String &v);

  // Audio / Sound enabled
  bool getSoundEnabled(bool def = true) const;
  void setSoundEnabled(bool en);

  // UI theme
  bool getThemeDark(bool def = true) const;
  void setThemeDark(bool dark);

  // Pool dimensions (cm) — used for dosing hints in alert banners
  int getPoolLengthCm(int def = 500) const;
  int getPoolWidthCm(int def = 300) const;
  int getPoolHeightCm(int def = 130) const;
  void setPoolLengthCm(int v);
  void setPoolWidthCm(int v);
  void setPoolHeightCm(int v);

  // UI style
  int getUiStyle(int def = 0) const;       // 0 = Vapor, 1 = Poolside, 2 = Helder
  void setUiStyle(int v);
  int getPoolsideTheme(int def = 0) const; // 0=Ocean, 1=Sunset, 2=Midnight, 3=Verdant
  void setPoolsideTheme(int v);

  // 7-day dosing history (ml/day, [0]=oldest .. [6]=yesterday)
  void pushDailyDose(float m1Ml, float m2Ml);
  void getDoseHistory(float m1Out[7], float m2Out[7]) const;

private:
  String _ns;
  mutable Preferences _prefs; // lazily opened in begin()
};

} // namespace core


