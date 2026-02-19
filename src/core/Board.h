#pragma once

#include <Arduino.h>
#include <lvgl.h>

namespace core {

// UI layout parameters per board (card sizes, font sizes, y-positions).
// Default values target the S3/C6 displays.
struct UiConfig {
  int  cardHeight      = 195;   // px, module-UI card height
  int  valueFontSize   = 28;    // 28 → montserrat_28 | 48 → montserrat_48
  int  statsFontSize   = 12;    // 12 → montserrat_12 | 14 → montserrat_14
  int  valueY          = 44;    // y-offset of value label from card top
  int  subLblY         = 78;    // y-offset of sub-label (e.g. "Target")
  int  subValsY        = 96;    // y-offset of sub-values
  int  pumpStatsY      = 120;   // y-offset of pump-stats label
  int  unitSpacing     = 6;     // px between value and its unit label
  int  pumpIconY       = 0;     // y-offset of pump icon from card bottom
  bool forceThreeCards = false; // if true, skip small-layout auto-detect
  bool showHostnameInIp = false; // include mDNS hostname in IP label
  const char* zigbeeLabelText = "Zigbee Mode"; // settings/mode label text
};

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

  // UI layout parameters (override in boards with different screen sizes/fonts)
  virtual UiConfig uiConfig()         const { return {}; }

  // Called every main-loop tick to check for stalled LVGL task and rebuild UI if needed.
  // Default: no-op. Override on boards that run LVGL on a dedicated task (S3 BSP path).
  virtual void lvglWatchdogTick() {}
};

} // namespace core


