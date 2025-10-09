#include "Esp32P4Board.h"

namespace core {

void Esp32P4Board::earlyInit() {
  // ESP32-P4 specific early initialization
  // Note: Display and touch init is done separately via initMipiDisplay() and initTouch()
}

void Esp32P4Board::initPeripherals() {
  // ESP32-P4: I2C for touch is initialized in main.cpp using ESP-IDF i2c_master API
  // Touch I2C bus (I2C_NUM_1): SDA=GPIO7, SCL=GPIO8
  // DO NOT use Wire.begin() here - it conflicts with ESP-IDF i2c_master!
  
  // Note: ESP32-C6 communication options:
  // Option 1 - UART: GPIO16 (RX), GPIO17 (TX)
  // Option 2 - I2C: GPIO9 (SDA), GPIO10 (SCL)  
  // Option 3 - SDIO: GPIO16-21 (for ESP-HOSTED high-speed)
}

void Esp32P4Board::initMipiDisplay() {
  // MIPI-DSI display initialization is handled by ST7701 driver
  // This is called from main.cpp after LVGL init
  // Display uses dedicated MIPI-DSI peripheral, not SPI
}

void Esp32P4Board::initTouch() {
  // Touch initialization is handled by GT911 driver
  // This is called from main.cpp after display init
  // Touch uses I2C initialized in initPeripherals()
}

} // namespace core

