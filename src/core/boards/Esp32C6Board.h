#pragma once

#include "core/Board.h"

namespace core {

class Esp32C6Board : public Board {
public:
  const char* name()       const override { return "ESP32-C6"; }
  void earlyInit()               override;
  void initPeripherals()         override;
  BoardPins pins()         const override;
  lv_disp_t* initDisplay()       override;
  void initTouch()               override;
  bool lvglLock()                override { return true; }
  void lvglUnlock()              override {}
  bool hasZigbee()         const override;
};

} // namespace core


