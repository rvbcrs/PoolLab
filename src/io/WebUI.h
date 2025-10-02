#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include "core/Storage.h"
#include "domain/Metrics.h"

namespace io {

// Forward declaration
class MotorController;

class WebUI {
public:
  void setStorage(core::Storage *s) { _storage = s; }
  void setMotor(MotorController *m) { _motor = m; }
  void setRefs(float *phMin, float *phMax, int *orpMin, int *orpMax, uint8_t *m1, uint8_t *m2, bool *motorsEn = nullptr, float *m1Flow = nullptr, float *m2Flow = nullptr) {
    _phMin = phMin; _phMax = phMax; _orpMin = orpMin; _orpMax = orpMax; _m1 = m1; _m2 = m2; _motorsEnabled = motorsEn; _m1Flow = m1Flow; _m2Flow = m2Flow;
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
  MotorController *_motor = nullptr;
  float *_phMin = nullptr, *_phMax = nullptr;
  int *_orpMin = nullptr, *_orpMax = nullptr;
  uint8_t *_m1 = nullptr, *_m2 = nullptr;
  bool *_motorsEnabled = nullptr;
  float *_m1Flow = nullptr, *_m2Flow = nullptr;
};

} // namespace io


