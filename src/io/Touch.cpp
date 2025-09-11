#include "Touch.h"
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
  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  pinMode(TOUCH_RST, OUTPUT);
  digitalWrite(TOUCH_RST, LOW); 
  delay(50); 
  digitalWrite(TOUCH_RST, HIGH); 
  delay(150);
  
  pinMode(TOUCH_INT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TOUCH_INT), onTouchInt, FALLING);
  
  uint8_t id[3] = {0};
  i2cRead(AXS5106L_ADDR, AXS5106L_ID_REG, id, 3);
  Serial.printf("[Touch] Init complete, ID: %02X %02X %02X\n", id[0], id[1], id[2]);
}

bool readTouchOnce(TouchPoint &p){
  p.pressed = false;
  // Try without interrupt check - always poll
  // if (!touchIrq) return false; // no new data
  // touchIrq = false;
  uint8_t data[14] = {0};
  if (!i2cRead(AXS5106L_ADDR, AXS5106L_TOUCH_DATA_REG, data, sizeof(data))) return false;
  uint8_t n = data[1];
  if (n == 0) return false;
  uint16_t rx = (((uint16_t)(data[2] & 0x0F)) << 8) | data[3];
  uint16_t ry = (((uint16_t)(data[4] & 0x0F)) << 8) | data[5];
  // Our display uses rotation(1): swap x/y compared to raw, per vendor demo
  int16_t x = ry; // swapped
  int16_t y = rx;
  // Clamp to bounds - use actual display dimensions for rotation(1)
  x = constrain(x, 0, 319);  // width in landscape
  y = constrain(y, 0, 171);  // height in landscape
  p.x = x; p.y = y; p.pressed = true; return true;
}

// Legacy compatibility
void touchPoll() {
  // No-op - not needed anymore
}

bool getTouchPoint(TouchPoint &p) {
  return readTouchOnce(p);
}

} // namespace io


