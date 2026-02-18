#pragma once

#include "core/Board.h"

namespace core {

class Esp32P4Board : public Board {
public:
  const char* name()       const override { return "ESP32-P4"; }
  void earlyInit()               override;
  void initPeripherals()         override;
  BoardPins pins()         const override;
  lv_disp_t* initDisplay()       override;  // MIPI-DSI + GT911 I2C + LVGL setup
  void initTouch()               override;  // GT911 read-side setup (after initDisplay)
  bool lvglLock()                override { return true; }
  void lvglUnlock()              override {}
  bool hasZigbee()         const override { return true; } // via C6 bridge
};

} // namespace core

