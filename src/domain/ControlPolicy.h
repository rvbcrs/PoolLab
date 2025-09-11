#pragma once

#include <Arduino.h>

namespace domain {

struct ControlConfig {
  float phMax;
  float phHyst;
  int   orpMin;
  int   orpHyst;
  uint8_t m1SpeedPc;
  uint8_t m2SpeedPc;
};

class ControlPolicy {
public:
  ControlPolicy(int stby, int m1_in1, int m1_in2, int m1_pwm, 
                int m2_in1, int m2_in2, int m2_pwm);

  void update(const ControlConfig& cfg, bool havePh, float phVal,
              bool haveOrp, float orpMv, bool forceMotorAOn,
              bool &m1Running, bool &m2Running);

private:
  int _stby;
  int _m1_in1, _m1_in2, _m1_pwm;
  int _m2_in1, _m2_in2, _m2_pwm;
};

} // namespace domain


