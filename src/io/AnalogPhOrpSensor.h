#pragma once

#include <Arduino.h>
#include "domain/SensorSource.h"
#include "domain/Telemetry.h"

namespace io {

// Simple analog reader for PH4502C-style boards using two ADC pins.
// Conversion uses basic linear calibration. Tune constants or update at runtime later.
class AnalogPhOrpSensor : public domain::SensorSource {
public:
  struct PhCal {
    // Two-point calibration around pH 7.0. Provide volts measured at pH4 and pH10.
    // Defaults assume MCP6002 / LM358 on 3.3 V supply (mid-supply = 1.65 V at pH 7,
    // gain ≈ 5.2×: pH 4 ≈ +0.93 V offset → 2.58 V, pH 10 ≈ −0.93 V offset → 0.72 V).
    // Calibrate with pH 4 and pH 7 buffer solutions for accurate readings.
    PhCal() : voltsAtPh4(2.58f), voltsAtPh10(0.72f) {}
    float voltsAtPh4;
    float voltsAtPh10;
  };
  struct OrpCal {
    // Analog output is assumed centered near voltsAt0mV for 0 mV ORP with linear scale.
    // Default assumes 3.3 V supply: mid-supply = 1.65 V at 0 mV ORP.
    OrpCal() : voltsAt0mV(1.65f), mVPerVolt(1000.0f) {}
    float voltsAt0mV;
    float mVPerVolt; // scale factor (mV per Volt delta)
  };

  AnalogPhOrpSensor(int phAdcPin, int orpAdcPin,
                    uint16_t sampleCount = 16, uint32_t updatePeriodMs = 1000)
  : _phPin(phAdcPin), _orpPin(orpAdcPin),
    _samples(sampleCount), _period(updatePeriodMs) {}

  AnalogPhOrpSensor(int phAdcPin, int orpAdcPin,
                    const PhCal &phCal, const OrpCal &orpCal,
                    uint16_t sampleCount, uint32_t updatePeriodMs)
  : _phPin(phAdcPin), _orpPin(orpAdcPin), _phCal(phCal), _orpCal(orpCal),
    _samples(sampleCount), _period(updatePeriodMs) {}

  void begin() override {
    _lastMs = 0;
    if (isValidPin(_phPin)) pinMode(_phPin, INPUT);
    if (isValidPin(_orpPin)) pinMode(_orpPin, INPUT);
    // Configure ADC resolution/attenuation (C6 supports 12-bit; 11dB up to ~3.5V)
    #ifdef analogReadResolution
    analogReadResolution(12);
    #endif
    #ifdef ADC_11db
    if (isValidPin(_phPin)) analogSetPinAttenuation(_phPin, ADC_11db);
    if (isValidPin(_orpPin)) analogSetPinAttenuation(_orpPin, ADC_11db);
    #endif
  }

  bool read(domain::Telemetry &out) override {
    const uint32_t now = millis();
    if (now - _lastMs < _period) return false;
    _lastMs = now;

    bool any = false;
    if (isValidPin(_phPin)) {
      float v = readAveragedVolts(_phPin);
      float ph = convertVoltsToPh(v);
      out.phVal = constrain(ph, 0.0f, 14.0f);
      out.havePh = true;
      any = true;
    }
    if (isValidPin(_orpPin)) {
      float v = readAveragedVolts(_orpPin);
      float orp = convertVoltsToOrpMv(v);
      out.orpMv = orp;
      out.haveOrp = true;
      any = true;
    }
    return any;
  }

  void setPhCalibration(const PhCal &c) { _phCal = c; }
  void setOrpCalibration(const OrpCal &c) { _orpCal = c; }
  void setUpdatePeriod(uint32_t ms) { _period = ms; }
  // Expose averaged voltage sampling for UI-guided calibration
  float sampleVoltsPh() const { return isValidPin(_phPin) ? readAveragedVolts(_phPin) : 0.0f; }
  float sampleVoltsOrp() const { return isValidPin(_orpPin) ? readAveragedVolts(_orpPin) : 0.0f; }
  PhCal getPhCalibration() const { return _phCal; }
  OrpCal getOrpCalibration() const { return _orpCal; }

private:
  static bool isValidPin(int p) { return p >= 0; }

  float readAveragedVolts(int pin) const {
    if (!isValidPin(pin) || _samples == 0) return 0.0f;
    uint32_t acc_mv = 0;
    for (uint16_t i = 0; i < _samples; ++i) {
      acc_mv += analogReadMilliVolts(pin);  // uses eFuse calibration → real mV
    }
    return (acc_mv / (float)_samples) / 1000.0f;
  }

  // Linearize using two-point calibration around pH 7
  float convertVoltsToPh(float v) const {
    const float v4 = _phCal.voltsAtPh4;
    const float v10 = _phCal.voltsAtPh10;
    // slope (pH per Volt)
    const float slope = (10.0f - 4.0f) / (v10 - v4 + 1e-6f);
    // intercept: pH = slope * v + b; use pH7 lies midway between 4 and 10 by volts mid
    const float v7 = (v10 + v4) * 0.5f;
    const float b = 7.0f - slope * v7;
    return slope * v + b;
  }

  float convertVoltsToOrpMv(float v) const {
    // Centered around voltsAt0mV with linear scale
    return (v - _orpCal.voltsAt0mV) * _orpCal.mVPerVolt;
  }

  int _phPin;
  int _orpPin;
  PhCal _phCal;
  OrpCal _orpCal;
  uint16_t _samples;
  uint32_t _period;
  uint32_t _lastMs = 0;
  const float _vref = 3.30f; // ADC reference (approx); adjust if needed
};

} // namespace io



