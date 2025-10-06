#pragma once

#include "core/Board.h"

namespace core {

class Esp32P4Board : public Board {
public:
  const char* name() const override { return "ESP32-P4"; }
  void earlyInit() override;
  void initPeripherals() override;
  
  // P4-specific methods for MIPI-DSI display
  void initMipiDisplay();
  void initTouch();
};

} // namespace core

