#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include "core/Storage.h"
#include "domain/Metrics.h"

namespace io {

class WebUI {
public:
  void setStorage(core::Storage *s) { _storage = s; }
  void setRefs(float *phMin, float *phMax, int *orpMin, int *orpMax, uint8_t *m1, uint8_t *m2) {
    _phMin = phMin; _phMax = phMax; _orpMin = orpMin; _orpMax = orpMax; _m1 = m1; _m2 = m2;
  }
  void begin();
  void loop();
  void stop();
  bool isActive() const { return _active; }
  void broadcastMetrics();

private:
  void handleIndex();
  void handleSettings();
  void handleApiState();
  void handleApiSave();
  void sendStyleHeader(String &html);
  void sendFooter(String &html);

  WebServer _http{80};
  WebSocketsServer _ws{81};
  bool _active = false;
  core::Storage *_storage = nullptr;
  float *_phMin = nullptr, *_phMax = nullptr;
  int *_orpMin = nullptr, *_orpMax = nullptr;
  uint8_t *_m1 = nullptr, *_m2 = nullptr;
};

} // namespace io


