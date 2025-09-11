#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include <Arduino_GFX_Library.h>

namespace core {

// Minimal display bridge: initialize LVGL draw buffer and flush using Arduino_GFX
class DisplayBridge {
public:
  DisplayBridge(Arduino_GFX* gfx);
  void initLvgl(uint16_t bufHeight = 20);
  lv_disp_t* registerDisplay();

private:
  Arduino_GFX* _gfx;
  lv_color_t* _buf1;
  lv_disp_draw_buf_t _drawBuf;
  lv_disp_drv_t _dispDrv;
};

} // namespace core


