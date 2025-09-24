#pragma once

#include "core/touch/TouchDriver.h"
#include <Wire.h>

namespace core {

class Axs5106L : public TouchDriver {
public:
  Axs5106L(int sda, int scl, int rst, int irq, uint8_t addr)
  : _sda(sda), _scl(scl), _rst(rst), _irq(irq), _addr(addr) {}

  void begin() override {
    Wire.begin(_sda, _scl);
    pinMode(_rst, OUTPUT);
    digitalWrite(_rst, LOW); delay(50); digitalWrite(_rst, HIGH); delay(150);
    pinMode(_irq, INPUT_PULLUP);
  }

  bool read(TouchPoint &out) override {
    out.pressed = false;
    uint8_t buf[14] = {0};
    if (!i2cRead(0x01, buf, sizeof(buf))) return false;
    uint8_t n = buf[1]; if (n == 0) return false;
    uint16_t rx = (uint16_t)((buf[2] & 0x0F) << 8) | buf[3];
    uint16_t ry = (uint16_t)((buf[4] & 0x0F) << 8) | buf[5];
    // Master mapping for rotation(1): x=ry; y=rx; clamp to 320x172
    int16_t x = (int16_t)ry;
    int16_t y = (int16_t)rx;
    if (x < 0) x = 0; if (x > 319) x = 319;
    if (y < 0) y = 0; if (y > 171) y = 171;
    out.x = x; out.y = y; out.pressed = true; return true;
  }

private:
  bool i2cRead(uint8_t reg, uint8_t* buf, uint32_t len){
    Wire.beginTransmission(_addr);
    Wire.write(reg);
    if (Wire.endTransmission() != 0) return false;
    uint32_t got = Wire.requestFrom(_addr, (uint8_t)len);
    if (got != len) return false;
    for (uint32_t i=0;i<len;i++) buf[i] = Wire.read();
    return true;
  }
  int _sda, _scl, _rst, _irq; uint8_t _addr;
};

} // namespace core


