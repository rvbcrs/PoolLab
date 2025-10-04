#pragma once

#include "core/touch/TouchDriver.h"
#include <driver/i2c_master.h>
#include "core/I2CBus.h"

namespace core {

class Axs5106L : public TouchDriver {
public:
  Axs5106L(int sda, int scl, int rst, int irq, uint8_t addr)
  : _sda(sda), _scl(scl), _rst(rst), _irq(irq), _addr(addr) {}

  void begin() override {
    _bus = core::i2c_bus_init(_sda, _scl);
    if (_dev == nullptr) {
      i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = _addr,
        .scl_speed_hz = 400000,
        .scl_wait_us = 0,
        .flags = { 0 }
      };
      i2c_master_dev_handle_t dev = nullptr;
      ESP_ERROR_CHECK(i2c_master_bus_add_device((i2c_master_bus_handle_t)_bus, &dev_cfg, &dev));
      _dev = dev;
    }
    if (_rst >= 0) {
      pinMode(_rst, OUTPUT);
      digitalWrite(_rst, LOW); delay(50); digitalWrite(_rst, HIGH); delay(150);
    }
    if (_irq >= 0) {
      pinMode(_irq, INPUT_PULLUP);
    }
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
    if (_dev == nullptr) return false;
    return ESP_OK == i2c_master_transmit_receive((i2c_master_dev_handle_t)_dev, &reg, 1, buf, len, -1);
  }
  int _sda, _scl, _rst, _irq; uint8_t _addr; void* _bus = nullptr; void* _dev = nullptr;
};

} // namespace core


