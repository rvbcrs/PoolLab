#pragma once

#include "domain/SensorSource.h"
#include <Arduino.h>

namespace domain {

class DummySensor : public SensorSource {
public:
  DummySensor(float phMin, float phMax, int orpMin)
  : _phMin(phMin), _phMax(phMax), _orpMin(orpMin) {}
  void begin() override { _last = 0; _inited=false; }
  bool read(Telemetry &out) override {
    uint32_t now = millis();
    if (!_inited) {
      _inited = true;
      randomSeed((uint32_t)esp_random());
      out.phVal = (_phMin + _phMax) * 0.5f; out.orpMv = (float)_orpMin + 50.0f; out.tempC = 25.0f;
      out.havePh = out.haveOrp = out.haveTemp = true; _last = 0;
    }
    if (now - _last < 1500) return false;
    _last = now;
    // pH oscillation
    switch (_phState) {
      case 0: out.phVal += 0.10f; if (out.phVal >= _phMax + 0.15f){ out.phVal = _phMax + 0.15f; _phState = 1; _phUntil = now + 6000; } break;
      case 1: if ((int32_t)(now - _phUntil) >= 0) _phState = 2; break;
      case 2: out.phVal -= 0.10f; if (out.phVal <= _phMin + 0.10f){ out.phVal = _phMin + 0.10f; _phState = 3; _phUntil = now + 4000; } break;
      default: if ((int32_t)(now - _phUntil) >= 0) _phState = 0; break;
    }
    out.phVal = constrain(out.phVal, 3.0f, 14.0f);
    // ORP oscillation
    switch (_orpState) {
      case 0: out.orpMv -= 4.0f; if (out.orpMv <= (float)_orpMin - 40.0f){ out.orpMv = (float)_orpMin - 40.0f; _orpState = 1; _orpUntil = now + 6000; } break;
      case 1: if ((int32_t)(now - _orpUntil) >= 0) _orpState = 2; break;
      case 2: out.orpMv += 4.0f; if (out.orpMv >= (float)_orpMin + 140.0f){ out.orpMv = (float)_orpMin + 140.0f; _orpState = 3; _orpUntil = now + 4000; } break;
      default: if ((int32_t)(now - _orpUntil) >= 0) _orpState = 0; break;
    }
    out.orpMv = constrain(out.orpMv, -2000.0f, 2000.0f);
    out.tempC += (float)random(-3,4) / 10.0f; out.tempC = constrain(out.tempC, 5.0f, 40.0f);
    // Ensure flags are asserted on every successful update
    out.havePh = true;
    out.haveOrp = true;
    out.haveTemp = true;
    return true;
  }
private:
  bool _inited=false; uint32_t _last=0;
  uint8_t _phState=0; uint32_t _phUntil=0;
  uint8_t _orpState=0; uint32_t _orpUntil=0;
  float _phMin,_phMax; int _orpMin;
};

} // namespace domain


