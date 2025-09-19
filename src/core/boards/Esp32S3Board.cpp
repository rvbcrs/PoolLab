#include "Esp32S3Board.h"

namespace core {

void Esp32S3Board::earlyInit() {
  // Placeholder (QSPI display handled by BSP elsewhere)
}

void Esp32S3Board::initPeripherals() {
  // Default SPI/I2C init left to libraries/BSP
}

} // namespace core


