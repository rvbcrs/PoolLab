#pragma once

#include <Arduino.h>

namespace core {

class Board {
public:
  virtual ~Board() {}
  virtual const char* name() const = 0;
  virtual void earlyInit() = 0;     // pins, clocks, etc.
  virtual void initPeripherals() = 0; // SPI/I2C/UART per board
};

} // namespace core


