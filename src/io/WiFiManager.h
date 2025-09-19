#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <functional>

namespace io {

class WiFiManager {
public:
  using IpCallback = std::function<void(const String&)>;

  void begin(const String &ssid, const String &pass, const char *clientPrefix, IpCallback onIpUpdate);
  void loop();
  void ensureSta();
  void startPortal(const String &apPrefix);
  void setIpCallback(IpCallback cb) { _onIp = cb; }

private:
  void setupEvents();
  void hardRestart();

  String _ssid, _pass;
  String _clientPrefix;
  uint32_t _lastAttemptMs = 0;
  bool _connecting = false;
  uint8_t _failCount = 0;
  IpCallback _onIp;
};

} // namespace io


