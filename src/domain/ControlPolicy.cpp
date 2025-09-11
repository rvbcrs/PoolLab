#include "ControlPolicy.h"
#include <Arduino.h>

namespace domain {

ControlPolicy::ControlPolicy(int stby, int m1_in1, int m1_in2, int m1_pwm, int m2_in1, int m2_in2, int m2_pwm)
  : _stby(stby), 
    _m1_in1(m1_in1), _m1_in2(m1_in2), _m1_pwm(m1_pwm),
    _m2_in1(m2_in1), _m2_in2(m2_in2), _m2_pwm(m2_pwm) {
  
  pinMode(_stby, OUTPUT);
  pinMode(_m1_in1, OUTPUT);
  pinMode(_m1_in2, OUTPUT);
  pinMode(_m2_in1, OUTPUT);
  pinMode(_m2_in2, OUTPUT);
  digitalWrite(_stby, HIGH);
}

void ControlPolicy::update(const ControlConfig &cfg, 
                          bool havePh, float phVal,
                          bool haveOrp, float orpMv,
                          bool forceMotorAOn,
                          bool &m1Running, bool &m2Running) {
  if (forceMotorAOn) {
    digitalWrite(_m1_in1, LOW);
    digitalWrite(_m1_in2, HIGH);
    ledcWrite(_m1_pwm, 255);
    m1Running = true;
    return;
  }

  // pH control: run when above max; stop when below (max - hyst)
  bool phHigh = havePh && phVal > cfg.phMax;
  bool phBack = havePh && phVal < (cfg.phMax - cfg.phHyst);
  if (phHigh) {
    if (!m1Running) {
      bool dirA = false;
      digitalWrite(_m1_in1, dirA ? HIGH : LOW);
      digitalWrite(_m1_in2, dirA ? LOW  : HIGH);
      uint8_t duty = (uint8_t)(cfg.m1SpeedPc * 255 / 100);
      ledcWrite(_m1_pwm, duty);
      m1Running = true;
    }
  } else if (phBack) {
    if (m1Running) {
      ledcWrite(_m1_pwm, 0);
      m1Running = false;
    }
  }

  // Motor 2: ORP low -> dose (direction A)
  int orpIntNow = (int)lrintf(orpMv);
  bool orpLow = haveOrp && (orpIntNow < cfg.orpMin);
  bool orpBack = haveOrp && (orpIntNow > (cfg.orpMin + cfg.orpHyst));
  if (orpLow) {
    if (!m2Running) {
      bool dirA = true;
      digitalWrite(_m2_in1, dirA ? HIGH : LOW);
      digitalWrite(_m2_in2, dirA ? LOW  : HIGH);
      uint8_t duty = (uint8_t)(cfg.m2SpeedPc * 255 / 100);
      ledcWrite(_m2_pwm, duty);
      m2Running = true;
    }
  } else if (orpBack) {
    if (m2Running) {
      ledcWrite(_m2_pwm, 0);
      m2Running = false;
    }
  }
}

} // namespace domain


