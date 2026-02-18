#pragma once

#include <Arduino.h>
#include <lvgl.h>

namespace core {

// All board GPIO assignments in one place.
// -1 means "not present on this board".
struct BoardPins {
  int lcdBl    = -1;            // backlight
  int btn1     = -1, btn2 = -1; // commissioning / boot buttons
  int motorStby = -1;           // TB6612 STBY
  int m1in1    = -1, m1in2 = -1, m1pwm = -1;
  int m2in1    = -1, m2in2 = -1, m2pwm = -1;
  int i2sBclk  = -1, i2sLrc = -1, i2sDin = -1; // speaker (MAX98357A)
};

class Board {
public:
  virtual ~Board() {}
  virtual const char* name()           const = 0;
  virtual void earlyInit()                   = 0; // pins, clocks, etc.
  virtual void initPeripherals()             = 0; // SPI/I2C/UART per board

  // Pin configuration
  virtual BoardPins pins()             const = 0;

  // Display + LVGL setup (board-specific driver init, flush/input callbacks).
  // Returns the registered lv_disp_t* so main.cpp can pass it to ui::init().
  virtual lv_disp_t* initDisplay()          = 0;

  // Touch driver init (no-op when BSP manages touch)
  virtual void initTouch()                   = 0;

  // LVGL thread safety (no-op on single-core boards; BSP mutex on S3)
  virtual bool lvglLock()                    = 0; // returns true on success
  virtual void lvglUnlock()                  = 0;

  // Runtime capability query (reflects build-time HAS_ZIGBEE flag)
  virtual bool hasZigbee()            const = 0;
};

} // namespace core


