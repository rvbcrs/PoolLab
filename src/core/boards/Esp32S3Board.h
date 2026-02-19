#pragma once

#include "core/Board.h"

namespace core {

class Esp32S3Board : public Board {
public:
  const char* name()       const override { return "ESP32-S3"; }
  void earlyInit()               override;
  void initPeripherals()         override;
  BoardPins pins()         const override;
  lv_disp_t* initDisplay()       override;
  void initTouch()               override;   // registers LVGL indev via io::readTouchOnce()
  bool lvglLock()                override;
  void lvglUnlock()              override;
  bool hasZigbee()         const override { return false; }
  void lvglWatchdogTick()        override;

private:
  uint32_t _watchdogNext  = 0;
  uint32_t _heartbeatMs   = 0;
};

} // namespace core


