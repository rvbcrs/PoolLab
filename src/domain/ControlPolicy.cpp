#include "ControlPolicy.h"
#include <Arduino.h>
#include "driver/ledc.h"
#include <esp_log.h>

namespace domain {

ControlPolicy::ControlPolicy(int stby, int m1_in1, int m1_in2, int m1_pwm, int m2_in1, int m2_in2, int m2_pwm)
  : _stby(stby), 
    _m1_in1(m1_in1), _m1_in2(m1_in2), _m1_pwm(m1_pwm),
    _m2_in1(m2_in1), _m2_in2(m2_in2), _m2_pwm(m2_pwm) {
  // Pin initialization is now handled by MotorController
  _lastPhSensorMs = millis();
  _lastOrpSensorMs = millis();
}

void ControlPolicy::triggerAlert(SafetyAlert alert) {
  _lastAlert = alert;
  _emergencyStop = true;
  
  // Log the alert
  const char* alertNames[] = {
    "NONE", "DAILY_LIMIT_M1", "DAILY_LIMIT_M2", 
    "SESSION_VOLUME_M1", "SESSION_VOLUME_M2",
    "SESSION_DURATION_M1", "SESSION_DURATION_M2",
    "PH_SANITY_LOW", "PH_SANITY_HIGH",
    "ORP_SANITY_LOW", "ORP_SANITY_HIGH",
    "PH_SENSOR_TIMEOUT", "ORP_SENSOR_TIMEOUT"
  };
  ESP_LOGE("SAFETY", "ALERT: %s - Emergency stop activated!", alertNames[(int)alert]);
  
  // Call callback if registered
  if (_alertCallback) {
    _alertCallback(alert);
  }
}

bool ControlPolicy::checkSafety(const ControlConfig& cfg, bool havePh, float phVal, bool haveOrp, float orpMv) {
  if (_emergencyStop) return false;  // Already in emergency stop
  
  uint32_t now = millis();
  
  // 1. Check sensor timeouts
  if (cfg.sensorTimeoutSec > 0) {
    if (!havePh && (now - _lastPhSensorMs) > (cfg.sensorTimeoutSec * 1000)) {
      triggerAlert(SafetyAlert::PH_SENSOR_TIMEOUT);
      return false;
    }
    if (!haveOrp && (now - _lastOrpSensorMs) > (cfg.sensorTimeoutSec * 1000)) {
      triggerAlert(SafetyAlert::ORP_SENSOR_TIMEOUT);
      return false;
    }
  }
  
  // Update last valid sensor times
  if (havePh) {
    _lastPhSensorMs = now;
    
    // 2. Check pH sanity
    if (phVal < cfg.phSanityMin) {
      triggerAlert(SafetyAlert::PH_SANITY_LOW);
      return false;
    }
    if (phVal > cfg.phSanityMax) {
      triggerAlert(SafetyAlert::PH_SANITY_HIGH);
      return false;
    }
    
    // Check for sudden jumps (> 1.0 pH in 1 sec) - indicates sensor glitch
    if (abs(phVal - _lastValidPh) > 1.0f) {
      ESP_LOGW("SAFETY", "pH sudden jump detected: %.2f -> %.2f (ignoring)", _lastValidPh, phVal);
      return false;  // Ignore this reading but don't trigger emergency
    }
    _lastValidPh = phVal;
  }
  
  if (haveOrp) {
    _lastOrpSensorMs = now;
    
    // 3. Check ORP sanity
    if (orpMv < cfg.orpSanityMin) {
      triggerAlert(SafetyAlert::ORP_SANITY_LOW);
      return false;
    }
    if (orpMv > cfg.orpSanityMax) {
      triggerAlert(SafetyAlert::ORP_SANITY_HIGH);
      return false;
    }
    
    // Check for sudden jumps (> 300 mV in 1 sec)
    if (abs(orpMv - _lastValidOrp) > 300.0f) {
      ESP_LOGW("SAFETY", "ORP sudden jump detected: %.0f -> %.0f (ignoring)", _lastValidOrp, orpMv);
      return false;
    }
    _lastValidOrp = orpMv;
  }
  
  // 4. Check daily volume limits
  if (cfg.maxDailyVolumeMl > 0) {
    if (_m1Stats.dailyVolumeMl >= cfg.maxDailyVolumeMl) {
      triggerAlert(SafetyAlert::DAILY_LIMIT_M1);
      return false;
    }
    if (_m2Stats.dailyVolumeMl >= cfg.maxDailyVolumeMl) {
      triggerAlert(SafetyAlert::DAILY_LIMIT_M2);
      return false;
    }
  }
  
  // 5. Check session volume limits (only for running pumps)
  if (cfg.maxSessionVolumeMl > 0) {
    if (_m1Stats.sessionStartMs > 0 && _m1Stats.sessionVolumeMl >= cfg.maxSessionVolumeMl) {
      triggerAlert(SafetyAlert::SESSION_VOLUME_M1);
      return false;
    }
    if (_m2Stats.sessionStartMs > 0 && _m2Stats.sessionVolumeMl >= cfg.maxSessionVolumeMl) {
      triggerAlert(SafetyAlert::SESSION_VOLUME_M2);
      return false;
    }
  }
  
  // 6. Check session duration limits (only for running pumps)
  if (cfg.maxSessionDurationSec > 0) {
    if (_m1Stats.sessionStartMs > 0) {
      uint32_t m1Duration = (now - _m1Stats.sessionStartMs) / 1000;
      if (m1Duration >= cfg.maxSessionDurationSec) {
        triggerAlert(SafetyAlert::SESSION_DURATION_M1);
        return false;
      }
    }
    if (_m2Stats.sessionStartMs > 0) {
      uint32_t m2Duration = (now - _m2Stats.sessionStartMs) / 1000;
      if (m2Duration >= cfg.maxSessionDurationSec) {
        triggerAlert(SafetyAlert::SESSION_DURATION_M2);
        return false;
      }
    }
  }
  
  return true;  // All safety checks passed
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
  // SAFETY FIRST: Check all safety conditions
  if (!checkSafety(cfg, havePh, phVal, haveOrp, orpMv)) {
    // Safety check failed - emergency stop all motors
    if (m1Running || m2Running) {
      ESP_LOGE("SAFETY", "Emergency stop - shutting down all motors");
      ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_4, 0);
      ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_4);
      ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_5, 0);
      ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_5);
      digitalWrite(_m1_in1, LOW);
      digitalWrite(_m1_in2, LOW);
      digitalWrite(_m2_in1, LOW);
      digitalWrite(_m2_in2, LOW);
      m1Running = false;
      m2Running = false;
    }
    // Update stats to finalize any running sessions
    updatePumpStats(_m1Stats, _m1LastStartMs, _m1LastSpeedPc, false, 0, cfg.m1FlowRateMlPerMin);
    updatePumpStats(_m2Stats, _m2LastStartMs, _m2LastSpeedPc, false, 0, cfg.m2FlowRateMlPerMin);
    return;
  }
  
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


