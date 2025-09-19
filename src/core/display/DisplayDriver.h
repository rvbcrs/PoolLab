#pragma once

#include <Arduino.h>

namespace core {

class DisplayDriver {
public:
  virtual ~DisplayDriver() {}
  virtual void begin() = 0;
  virtual void setRotation(uint8_t r) = 0;
};

} // namespace core


