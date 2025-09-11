#pragma once

#include <Arduino.h>

namespace io {

// Touch controller (AXS5106L) constants
extern const int TOUCH_SDA;
extern const int TOUCH_SCL;
extern const int TOUCH_RST;
extern const int TOUCH_INT;
extern const uint8_t AXS5106L_ADDR;
extern const uint8_t AXS5106L_ID_REG;
extern const uint8_t AXS5106L_TOUCH_DATA_REG;

struct TouchPoint { int16_t x; int16_t y; bool pressed; };

bool i2cWrite8(uint8_t addr, uint8_t reg, const uint8_t* data, uint32_t len);
bool i2cRead(uint8_t addr, uint8_t reg, uint8_t* buf, uint32_t len);

void touchBegin();
void touchPoll();  // Poll touch hardware and update cached data
bool getTouchPoint(TouchPoint &p);  // Get cached touch data
bool readTouchOnce(TouchPoint &p);  // Legacy function - polls and returns data

} // namespace io


