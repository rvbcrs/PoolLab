#include "ControlPolicy.h"
#include <Arduino.h>
#include "driver/ledc.h"

namespace domain {

ControlPolicy::ControlPolicy(int stby, int m1_in1, int m1_in2, int m1_pwm, int m2_in1, int m2_in2, int m2_pwm)
  : _stby(stby), 
    _m1_in1(m1_in1), _m1_in2(m1_in2), _m1_pwm(m1_pwm),
    _m2_in1(m2_in1), _m2_in2(m2_in2), _m2_pwm(m2_pwm) {
  // Pin initialization is now handled by MotorController
}

void ControlPolicy::updatePumpStats(PumpStats &stats, uint32_t &lastStartMs, uint8_t &lastSpeedPc,
                                    bool isRunning, uint8_t speedPc, float flowRateMlPerMin) {
  uint32_t now = millis();
  
  if (isRunning) {
    // Motor is running
    if (lastStartMs == 0) {
      // Motor just started - begin new session
      lastStartMs = now;
      lastSpeedPc = speedPc;
      stats.sessionStartMs = now;
      stats.sessionVolumeMl = 0.0f;  // Reset session volume
    } else {
      // Motor was already running - accumulate volume
      uint32_t elapsedMs = now - lastStartMs;
      float elapsedMin = elapsedMs / 60000.0f;
      float volumeMl = (lastSpeedPc / 100.0f) * flowRateMlPerMin * elapsedMin;
      
      stats.totalVolumeMl += volumeMl;
      stats.dailyVolumeMl += volumeMl;
      stats.sessionVolumeMl += volumeMl;
      stats.totalRuntimeMs += elapsedMs;
      lastStartMs = now;
      lastSpeedPc = speedPc;
    }
    // Update current flow rate
    stats.currentFlowMlMin = (speedPc / 100.0f) * flowRateMlPerMin;
  } else {
    // Motor stopped
    if (lastStartMs != 0) {
      // Motor just stopped - accumulate final period
      uint32_t elapsedMs = now - lastStartMs;
      float elapsedMin = elapsedMs / 60000.0f;
      float volumeMl = (lastSpeedPc / 100.0f) * flowRateMlPerMin * elapsedMin;
      
      stats.totalVolumeMl += volumeMl;
      stats.dailyVolumeMl += volumeMl;
      stats.sessionVolumeMl += volumeMl;
      stats.totalRuntimeMs += elapsedMs;
      lastStartMs = 0;
      lastSpeedPc = 0;
      stats.sessionStartMs = 0;
    }
    stats.currentFlowMlMin = 0.0f;
  }
}

void ControlPolicy::update(const ControlConfig &cfg, 
                          bool havePh, float phVal,
                          bool haveOrp, float orpMv,
                          bool forceMotorAOn,
                          bool &m1Running, bool &m2Running) {
  if (forceMotorAOn) {
    bool dirA = (M1_DIR_A != 0);
    digitalWrite(_m1_in1, dirA ? HIGH : LOW);
    digitalWrite(_m1_in2, dirA ? LOW  : HIGH);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_4, 1023);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_4);
    m1Running = true;
    updatePumpStats(_m1Stats, _m1LastStartMs, _m1LastSpeedPc, m1Running, 100, cfg.m1FlowRateMlPerMin);
    return;
  }

  // pH control (assignable to M1 or M2)
  // Symmetric policy: dose when pH < min or pH > max; stop when back within band with hysteresis
  bool phOut = havePh && (phVal < cfg.phMin || phVal > cfg.phMax);
  bool phBack = havePh && (phVal >= (cfg.phMin + cfg.phHyst) && phVal <= (cfg.phMax - cfg.phHyst));
  if (PH_ON_MOTOR_A) {
    if (phOut) {
      if (!m1Running) {
        bool dirA = (M1_DIR_A != 0);
        digitalWrite(_m1_in1, dirA ? HIGH : LOW);
        digitalWrite(_m1_in2, dirA ? LOW  : HIGH);
        uint32_t duty = (uint32_t)(cfg.m1SpeedPc * 1023 / 100);  // 10-bit: 0-1023
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_4, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_4);
        m1Running = true;
      }
    } else if (phBack) {
      if (m1Running) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_4, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_4);
        digitalWrite(_m1_in1, LOW);
        digitalWrite(_m1_in2, LOW);
        m1Running = false;
      }
    }
  }

  // ORP control goes to the other motor automatically
  int orpIntNow = (int)lrintf(orpMv);
  bool orpLow = haveOrp && (orpIntNow < cfg.orpMin);
  bool orpBack = haveOrp && (orpIntNow > (cfg.orpMin + cfg.orpHyst));
  if (PH_ON_MOTOR_A) {
    // ORP on Motor B
    if (orpLow) {
      if (!m2Running) {
        bool dirA = (M2_DIR_A != 0);
        digitalWrite(_m2_in1, dirA ? HIGH : LOW);
        digitalWrite(_m2_in2, dirA ? LOW  : HIGH);
        uint32_t duty = (uint32_t)(cfg.m2SpeedPc * 1023 / 100);  // 10-bit: 0-1023
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_5, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_5);
        m2Running = true;
      }
    } else if (orpBack) {
      if (m2Running) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_5, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_5);
        digitalWrite(_m2_in1, LOW);
        digitalWrite(_m2_in2, LOW);
        m2Running = false;
      }
    }
  } else {
    // ORP on Motor A
    if (orpLow) {
      if (!m1Running) {
        bool dirA = (M1_DIR_A != 0);
        digitalWrite(_m1_in1, dirA ? HIGH : LOW);
        digitalWrite(_m1_in2, dirA ? LOW  : HIGH);
        uint32_t duty = (uint32_t)(cfg.m1SpeedPc * 1023 / 100);  // 10-bit: 0-1023
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_4, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_4);
        m1Running = true;
      }
    } else if (orpBack) {
      if (m1Running) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_4, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_4);
        digitalWrite(_m1_in1, LOW);
        digitalWrite(_m1_in2, LOW);
        m1Running = false;
      }
    }
  }
  
  // Update pump statistics for both motors
  updatePumpStats(_m1Stats, _m1LastStartMs, _m1LastSpeedPc, m1Running, cfg.m1SpeedPc, cfg.m1FlowRateMlPerMin);
  updatePumpStats(_m2Stats, _m2LastStartMs, _m2LastSpeedPc, m2Running, cfg.m2SpeedPc, cfg.m2FlowRateMlPerMin);
}

} // namespace domain


