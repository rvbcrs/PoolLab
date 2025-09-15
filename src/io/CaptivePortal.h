#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "core/Storage.h"

namespace io {

class CaptivePortal {
public:
  CaptivePortal();
  void beginAP(const String &apSsid);
  void loop();
  bool isActive() const { return _active; }
  void stop();

private:
  void setupRoutes();

  DNSServer _dns;
  WebServer _http{80};
  bool _active = false;
  core::Storage *_storage = nullptr; // optional
public:
  void setStorage(core::Storage *s) { _storage = s; }
};

} // namespace io
