#pragma once

#include <Arduino.h>
#include "domain/SensorSource.h"
#include "domain/Telemetry.h"

namespace io {

// Minimal ADS1115 single-shot reader for two single-ended channels
class AdsPhOrpSensor : public domain::SensorSource {
public:
  enum Gain {
    GAIN_2_3 = 0, // ±6.144 V
    GAIN_1   = 1, // ±4.096 V
    GAIN_2   = 2, // ±2.048 V
    GAIN_4   = 3, // ±1.024 V
    GAIN_8   = 4, // ±0.512 V
    GAIN_16  = 5  // ±0.256 V
  };

  struct PhCal { PhCal(): voltsAtPh4(3.00f), voltsAtPh10(2.00f){} float voltsAtPh4; float voltsAtPh10; };
  struct OrpCal { OrpCal(): voltsAt0mV(2.50f), mVPerVolt(1000.0f){} float voltsAt0mV; float mVPerVolt; };

  AdsPhOrpSensor(uint8_t i2cAddr, int sdaPin, int sclPin, uint8_t chPh, uint8_t chOrp,
                 Gain gain = GAIN_1, uint8_t samples = 8, uint32_t periodMs = 1000)
  : _addr(i2cAddr), _sda(sdaPin), _scl(sclPin), _chPh(chPh), _chOrp(chOrp), _gain(gain), _samples(samples), _period(periodMs) {}

  void begin() override {
    beginBus();
    _lastMs = 0;
  }

  bool read(domain::Telemetry &out) override {
    uint32_t now = millis(); if (now - _lastMs < _period) return false; _lastMs = now;
    bool any=false;
    if (_chPh <= 3) { float v = readChannelVolts(_chPh); out.phVal = constrain(convertVoltsToPh(v), 0.0f, 14.0f); out.havePh=true; any=true; }
    if (_chOrp <= 3) { float v = readChannelVolts(_chOrp); out.orpMv = convertVoltsToOrpMv(v); out.haveOrp=true; any=true; }
    return any;
  }

  void setPhCalibration(const PhCal &c){ _phCal=c; }
  void setOrpCalibration(const OrpCal &c){ _orpCal=c; }
  PhCal getPhCalibration() const { return _phCal; }
  OrpCal getOrpCalibration() const { return _orpCal; }
  float sampleVoltsPh(){ return (_chPh<=3)?readChannelVolts(_chPh):0.0f; }
  float sampleVoltsOrp(){ return (_chOrp<=3)?readChannelVolts(_chOrp):0.0f; }

private:
  float fsVolts() const {
    switch(_gain){
      case GAIN_2_3: return 6.144f; case GAIN_1: return 4.096f; case GAIN_2: return 2.048f;
      case GAIN_4: return 1.024f; case GAIN_8: return 0.512f; default: return 0.256f;
    }
  }

  float readChannelVolts(uint8_t ch){
    const uint16_t mux = 0x4000 | (ch << 12); // single-ended AINx
    const uint16_t pga = (uint16_t)_gain << 9;
    const uint16_t mode = 0x0100; // single-shot
    const uint16_t dr = 0x0080;   // 1600 SPS
    uint16_t cfg = 0x8000 | mux | pga | mode | dr | 0x0003; // COMP disabled
    float acc=0.0f;
    for (uint8_t i=0;i<_samples;i++){
      writeReg16(0x01, cfg);
      // wait conversion (~1ms at 860sps; use 3ms)
      delay(3);
      int16_t code = (int16_t)readReg16(0x00);
      float v = (float)code / 32767.0f * fsVolts();
      acc += v;
    }
    return acc / (float)_samples;
  }

  void writeReg16(uint8_t reg, uint16_t val){ writeBytes(reg, (uint8_t[]){ (uint8_t)(val>>8), (uint8_t)(val&0xFF) }, 2); }
  uint16_t readReg16(uint8_t reg){ uint8_t b[2]={0}; readBytes(reg, b, 2); return (uint16_t)((((uint16_t)b[0])<<8)|b[1]); }

  void beginBus();
  void writeBytes(uint8_t reg, const uint8_t *data, size_t len);
  void readBytes(uint8_t reg, uint8_t *data, size_t len);

  float convertVoltsToPh(float v) const {
    float v4=_phCal.voltsAtPh4, v10=_phCal.voltsAtPh10; float slope = (10.0f-4.0f)/((v10 - v4) + 1e-6f); float v7=(v10+v4)*0.5f; float b = 7.0f - slope*v7; return slope*v + b;
  }
  float convertVoltsToOrpMv(float v) const { return (v - _orpCal.voltsAt0mV) * _orpCal.mVPerVolt; }

  uint8_t _addr; int _sda, _scl; uint8_t _chPh, _chOrp; Gain _gain; uint8_t _samples; uint32_t _period; uint32_t _lastMs=0;
  PhCal _phCal; OrpCal _orpCal;
  void * _bus = nullptr; // i2c_master_bus_handle_t
  void * _dev = nullptr; // i2c_master_dev_handle_t
};

} // namespace io


