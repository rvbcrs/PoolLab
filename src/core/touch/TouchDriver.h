#pragma once

#include <Arduino.h>

namespace core {

struct TouchPoint { int16_t x; int16_t y; bool pressed; };

class TouchDriver {
public:
  virtual ~TouchDriver() {}
  virtual void begin() = 0;
  virtual bool read(TouchPoint &p) = 0;
};

} // namespace core


