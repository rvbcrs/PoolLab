#pragma once

#include "core/display/DisplayDriver.h"

#if !defined(USE_JC3248W535)
#include <Arduino_GFX_Library.h>

namespace core {

class St7789C6 : public DisplayDriver {
public:
  St7789C6(Arduino_GFX* gfx, Arduino_DataBus* bus) : _gfx(gfx), _bus(bus) {}
  void begin() override;
  void setRotation(uint8_t r) override { if (_gfx) _gfx->setRotation(r); }
private:
  Arduino_GFX* _gfx;
  Arduino_DataBus* _bus;
};

} // namespace core

#else

namespace core {

class St7789C6 : public DisplayDriver {
public:
  St7789C6(void*, void*){}
  void begin() override {}
  void setRotation(uint8_t) override {}
};

} // namespace core

#endif

