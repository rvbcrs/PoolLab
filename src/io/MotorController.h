#pragma once

#include <Arduino.h>
#include "domain/ControlPolicy.h"
#include "driver/ledc.h"

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
    _pwmFreq = pwmFreq;
    _pwmBits = pwmBits;
    
    // Setup GPIO pins
    pinMode(_pins.stby, OUTPUT);
    pinMode(_pins.m1In1, OUTPUT);
    pinMode(_pins.m1In2, OUTPUT);
    pinMode(_pins.m2In1, OUTPUT);
    pinMode(_pins.m2In2, OUTPUT);
    
    // Use ESP-IDF LEDC API directly to avoid timer conflicts with backlight (timer 1)
    // Configure Timer 2 for M1
    ledc_timer_config_t timer2_cfg = {
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .duty_resolution = (ledc_timer_bit_t)pwmBits,
      .timer_num = LEDC_TIMER_2,
      .freq_hz = (uint32_t)pwmFreq,
      .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer2_cfg);
    
    // Configure Timer 3 for M2
    ledc_timer_config_t timer3_cfg = {
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .duty_resolution = (ledc_timer_bit_t)pwmBits,
      .timer_num = LEDC_TIMER_3,
      .freq_hz = (uint32_t)pwmFreq,
      .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer3_cfg);
    
    // Configure Channel 4 for M1 (using Timer 2)
    ledc_channel_config_t m1_channel_cfg = {
      .gpio_num = _pins.m1Pwm,
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .channel = LEDC_CHANNEL_4,
      .intr_type = LEDC_INTR_DISABLE,
      .timer_sel = LEDC_TIMER_2,
      .duty = 0,
      .hpoint = 0,
      .flags = {.output_invert = 0}
    };
    ledc_channel_config(&m1_channel_cfg);
    
    // Configure Channel 5 for M2 (using Timer 3)
    ledc_channel_config_t m2_channel_cfg = {
      .gpio_num = _pins.m2Pwm,
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .channel = LEDC_CHANNEL_5,
      .intr_type = LEDC_INTR_DISABLE,
      .timer_sel = LEDC_TIMER_3,
      .duty = 0,
      .hpoint = 0,
      .flags = {.output_invert = 0}
    };
    ledc_channel_config(&m2_channel_cfg);
    
    // Initialize to safe state
    digitalWrite(_pins.stby, HIGH);  // Enable driver
    digitalWrite(_pins.m1In1, LOW);
    digitalWrite(_pins.m1In2, LOW);
    digitalWrite(_pins.m2In1, LOW);
    digitalWrite(_pins.m2In2, LOW);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_4, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_4);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_5, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_5);
    
    _policy = new domain::ControlPolicy(_pins.stby, _pins.m1In1, _pins.m1In2, _pins.m1Pwm, _pins.m2In1, _pins.m2In2, _pins.m2Pwm);
  }

  void tick(const domain::ControlConfig &cfg, bool havePh, float phVal, bool haveOrp, float orpMv, bool forceAOn) {
    if (!_policy) return;
    digitalWrite(_pins.stby, HIGH);  // Ensure TB6612 is enabled
    _policy->update(cfg, havePh, phVal, haveOrp, orpMv, forceAOn, _m1Running, _m2Running);
  }

  void stopAll() {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_4, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_4);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_5, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_5);
    // Ensure H-bridges are coasting
    digitalWrite(_pins.m1In1, LOW);
    digitalWrite(_pins.m1In2, LOW);
    digitalWrite(_pins.m2In1, LOW);
    digitalWrite(_pins.m2In2, LOW);
    digitalWrite(_pins.stby, LOW);  // Disable driver for safety
    _m1Running = _m2Running = false;
  }

         bool isM1Running() const { return _m1Running; }
         bool isM2Running() const { return _m2Running; }
         
         domain::PumpStats getM1Stats() const { return _policy ? _policy->getM1Stats() : domain::PumpStats{}; }
         domain::PumpStats getM2Stats() const { return _policy ? _policy->getM2Stats() : domain::PumpStats{}; }
         void resetM1Total() { if (_policy) _policy->resetM1Total(); }
         void resetM2Total() { if (_policy) _policy->resetM2Total(); }
         void resetM1Daily() { if (_policy) _policy->resetM1Daily(); }
         void resetM2Daily() { if (_policy) _policy->resetM2Daily(); }
         void resetAllDaily() { if (_policy) _policy->resetAllDaily(); }

private:
  MotorPins _pins{};
  int _pwmFreq = 0;
  int _pwmBits = 0;
  domain::ControlPolicy *_policy = nullptr;
  bool _m1Running = false;
  bool _m2Running = false;
};

} // namespace io


