#pragma once

#include <Arduino.h>

namespace domain {

#ifndef PH_ON_MOTOR_A
#define PH_ON_MOTOR_A 1   // 1: pH uses Motor A (M1), 0: pH uses Motor B (M2)
#endif
#ifndef M1_DIR_A
#define M1_DIR_A 0        // 1: M1 forward = AIN1=HIGH/AIN2=LOW, 0: forward = AIN1=LOW/AIN2=HIGH
#endif
#ifndef M2_DIR_A
#define M2_DIR_A 1        // 1: M2 forward = BIN1=HIGH/BIN2=LOW, 0: forward = BIN1=LOW/BIN2=HIGH
#endif

struct ControlConfig {
  float phMin;
  float phMax;
  float phHyst;
  int   orpMin;
  int   orpHyst;
  uint8_t m1SpeedPc;
  uint8_t m2SpeedPc;
  float m1FlowRateMlPerMin;  // Flow rate at 100% speed for Motor 1 (ml/min)
  float m2FlowRateMlPerMin;  // Flow rate at 100% speed for Motor 2 (ml/min)
};

struct PumpStats {
  float totalVolumeMl;      // Total volume pumped since manual reset (ml) - for tank changes
  float dailyVolumeMl;      // Volume pumped today (resets at midnight)
  float sessionVolumeMl;    // Volume pumped in current run session (since motor started)
  float currentFlowMlMin;   // Current flow rate (ml/min)
  uint32_t totalRuntimeMs;  // Total runtime in milliseconds
  uint32_t sessionStartMs;  // When current session started (0 if not running)
};

class ControlPolicy {
public:
  ControlPolicy(int stby, int m1_in1, int m1_in2, int m1_pwm, 
                int m2_in1, int m2_in2, int m2_pwm);

  void update(const ControlConfig& cfg, bool havePh, float phVal,
              bool haveOrp, float orpMv, bool forceMotorAOn,
              bool &m1Running, bool &m2Running);

  // Get pump statistics
  PumpStats getM1Stats() const { return _m1Stats; }
  PumpStats getM2Stats() const { return _m2Stats; }
  
  // Reset pump statistics
  void resetM1Total() { _m1Stats.totalVolumeMl = 0.0f; }  // For tank changes
  void resetM2Total() { _m2Stats.totalVolumeMl = 0.0f; }
  void resetM1Daily() { _m1Stats.dailyVolumeMl = 0.0f; }  // For midnight reset
  void resetM2Daily() { _m2Stats.dailyVolumeMl = 0.0f; }
  void resetAllDaily() { _m1Stats.dailyVolumeMl = 0.0f; _m2Stats.dailyVolumeMl = 0.0f; }

private:
  int _stby;
  int _m1_in1, _m1_in2, _m1_pwm;
  int _m2_in1, _m2_in2, _m2_pwm;
  
  // Pump statistics tracking
  PumpStats _m1Stats = {};
  PumpStats _m2Stats = {};
  uint32_t _m1LastStartMs = 0;
  uint32_t _m2LastStartMs = 0;
  uint8_t _m1LastSpeedPc = 0;
  uint8_t _m2LastSpeedPc = 0;
  
  void updatePumpStats(PumpStats &stats, uint32_t &lastStartMs, uint8_t &lastSpeedPc,
                       bool isRunning, uint8_t speedPc, float flowRateMlPerMin);
};

} // namespace domain


