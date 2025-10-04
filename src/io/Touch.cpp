#include "Touch.h"
#include <Arduino.h>
#if !defined(BOARD_ESP32S3_35)
#include <driver/i2c_master.h>
#include "core/I2CBus.h"
#endif

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

static void* s_i2c_bus = nullptr;   // i2c_master_bus_handle_t (non-S3)
static void* s_i2c_dev = nullptr;   // i2c_master_dev_handle_t (non-S3)

bool i2cWrite8(uint8_t addr, uint8_t reg, const uint8_t* data, uint32_t len){
#if defined(BOARD_ESP32S3_35)
  (void)addr; (void)reg; (void)data; (void)len; return false;
#else
  if (s_i2c_dev == nullptr) return false;
  uint8_t tmp[5];
  tmp[0] = reg;
  if (len && data){
    uint32_t n = len > 4 ? 4 : len; // defensive bound
    memcpy(&tmp[1], data, n);
    return ESP_OK == i2c_master_transmit((i2c_master_dev_handle_t)s_i2c_dev, tmp, n+1, -1);
  }
  return ESP_OK == i2c_master_transmit((i2c_master_dev_handle_t)s_i2c_dev, tmp, 1, -1);
#endif
}

bool i2cRead(uint8_t addr, uint8_t reg, uint8_t* buf, uint32_t len){
#if defined(BOARD_ESP32S3_35)
  (void)addr; (void)reg; (void)buf; (void)len; return false;
#else
  (void)addr; // device address fixed in handle
  if (s_i2c_dev == nullptr) return false;
  return ESP_OK == i2c_master_transmit_receive((i2c_master_dev_handle_t)s_i2c_dev, &reg, 1, buf, len, -1);
#endif
}

void touchBegin(){
  // Initialize ESP-IDF I2C master bus and device
#if defined(BOARD_ESP32S3_35)
  // S3: no Touch I2C to avoid mixing drivers; rely on BSP or disabled touch
  (void)TOUCH_SDA; (void)TOUCH_SCL;
#else
  s_i2c_bus = core::i2c_bus_init(TOUCH_SDA, TOUCH_SCL);
  if (s_i2c_dev == nullptr && s_i2c_bus){
    i2c_device_config_t dev_cfg = { .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = AXS5106L_ADDR, .scl_speed_hz = 400000, .scl_wait_us = 0, .flags = { 0 } };
    i2c_master_dev_handle_t dev = nullptr;
    if (ESP_OK == i2c_master_bus_add_device((i2c_master_bus_handle_t)s_i2c_bus, &dev_cfg, &dev)){
      s_i2c_dev = dev;
    }
  }
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
  // Reduce verbosity to avoid UI stalls when no serial monitor is attached
  //ESP_LOGI("TOUCH", "Init complete, ID: %02X %02X %02X", id[0], id[1], id[2]);
}

bool readTouchOnce(TouchPoint &out) {
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
}

// Legacy compatibility
void touchPoll() {
  // No-op - not needed anymore
}

bool getTouchPoint(TouchPoint &p) {
  return readTouchOnce(p);
}

} // namespace io


