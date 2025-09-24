#pragma once

#include <Arduino.h>
#include "domain/ControlPolicy.h"

namespace io {

struct MotorPins {
  int stby;
  int m1In1; int m1In2; int m1Pwm;
  int m2In1; int m2In2; int m2Pwm;
};

class MotorController {
public:
  void begin(const MotorPins &pins, int pwmFreq, int pwmBits) {
    _pins = pins;
    ledcAttach(_pins.m1Pwm, pwmFreq, pwmBits);
    ledcAttach(_pins.m2Pwm, pwmFreq, pwmBits);
    ledcWrite(_pins.m1Pwm, 0);
    ledcWrite(_pins.m2Pwm, 0);
    _policy = new domain::ControlPolicy(_pins.stby, _pins.m1In1, _pins.m1In2, _pins.m1Pwm, _pins.m2In1, _pins.m2In2, _pins.m2Pwm);
  }

  void tick(const domain::ControlConfig &cfg, bool havePh, float phVal, bool haveOrp, float orpMv, bool forceAOn) {
    if (!_policy) return;
    _policy->update(cfg, havePh, phVal, haveOrp, orpMv, forceAOn, _m1Running, _m2Running);
  }

  void stopAll() {
    if (!_policy) return;
    ledcWrite(_pins.m1Pwm, 0);
    ledcWrite(_pins.m2Pwm, 0);
    _m1Running = _m2Running = false;
  }

  bool isM1Running() const { return _m1Running; }
  bool isM2Running() const { return _m2Running; }

private:
  MotorPins _pins{};
  domain::ControlPolicy *_policy = nullptr;
  bool _m1Running = false;
  bool _m2Running = false;
};

} // namespace io


