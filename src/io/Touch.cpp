#if defined(BOARD_ESP32S3_35) && defined(USE_JC3248W535)
#include "Touch.h"
namespace io {
bool i2cRead(uint8_t, uint8_t, uint8_t*, uint32_t){ return false; }
void touchBegin() {}
bool readTouchOnce(TouchPoint &p){ p.pressed=false; return false; }
bool getTouchPoint(TouchPoint &p){ p.pressed=false; return false; }
}
#else
#include "Touch.h"
#include <Arduino.h>
#include <Wire.h>

namespace io {

const int TOUCH_SDA = 18;
const int TOUCH_SCL = 19;
const int TOUCH_RST = 20;
const int TOUCH_INT = 21;
const uint8_t AXS5106L_ADDR = 0x63;
const uint8_t AXS5106L_ID_REG = 0x08;
const uint8_t AXS5106L_TOUCH_DATA_REG = 0x01;

// Interrupt flag for compatibility (not used in polling)
static volatile bool touchIrq = false;
static void IRAM_ATTR onTouchInt(){ touchIrq = true; }

bool i2cWrite8(uint8_t addr, uint8_t reg, const uint8_t* data, uint32_t len){
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (len) Wire.write(data, len);
  return Wire.endTransmission() == 0;
}

bool i2cRead(uint8_t addr, uint8_t reg, uint8_t* buf, uint32_t len){
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) return false;
  uint32_t got = Wire.requestFrom(addr, (uint8_t)len);
  if (got != len) return false;
  for (uint32_t i=0;i<len;i++) buf[i] = Wire.read();
  return true;
}

void touchBegin(){
  // Use Arduino Wire only on non-S3 boards; S3 BSP sets up I2C
  #if !defined(BOARD_ESP32S3_35)
  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  #else
  (void)TOUCH_SDA; (void)TOUCH_SCL;
  #endif
  pinMode(TOUCH_RST, OUTPUT);
  digitalWrite(TOUCH_RST, LOW); 
  delay(50); 
  digitalWrite(TOUCH_RST, HIGH); 
  delay(150);
  
  pinMode(TOUCH_INT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TOUCH_INT), onTouchInt, FALLING);
  
  uint8_t id[3] = {0};
  i2cRead(AXS5106L_ADDR, AXS5106L_ID_REG, id, 3);
  ESP_LOGI("TOUCH", "Init complete, ID: %02X %02X %02X", id[0], id[1], id[2]);
}

bool readTouchOnce(TouchPoint &out) {
#if defined(BOARD_ESP32C6_TOUCH_1_47)
  uint8_t buf[14] = {0};
  // Defensive: ensure I2C read succeeds and buffer has expected structure
  if (!i2cRead(AXS5106L_ADDR, AXS5106L_TOUCH_DATA_REG, buf, sizeof(buf))) {
    out.pressed = false; return false;
  }
  uint8_t n = buf[1];
  if (n == 0) { out.pressed = false; return false; }
  // Validate minimal coordinates payload presence
  // buf[2..5] should exist
  uint16_t rx = (uint16_t)((buf[2] & 0x0F) << 8) | buf[3];
  uint16_t ry = (uint16_t)((buf[4] & 0x0F) << 8) | buf[5];
  if (rx == 0 && ry == 0) { out.pressed = false; return false; }
  // Mapping as on master for rotation(1): x=ry; y=rx; clamp to 320x172
  int16_t x = (int16_t)ry;
  int16_t y = (int16_t)rx;
  if (x < 0) x = 0; if (x > 319) x = 319;
  if (y < 0) y = 0; if (y > 171) y = 171;
  out.x = x;
  out.y = y;
  out.pressed = true;
  return true;
#else
  // existing implementations for other boards
  return false;
#endif
}

// Legacy compatibility
void touchPoll() {
  // No-op - not needed anymore
}

bool getTouchPoint(TouchPoint &p) {
  return readTouchOnce(p);
}

} // namespace io


#endif
