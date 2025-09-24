#include "Esp32C6Board.h"
#include <SPI.h>

namespace core {

void Esp32C6Board::earlyInit() {
  // Nothing specific yet; keep as placeholder
}

void Esp32C6Board::initPeripherals() {
  // Match working pins: SCK=1, MOSI=2, SS=14
  SPI.begin(1 /* SCK */, -1 /* MISO */, 2 /* MOSI */, 14 /* SS */);
}

} // namespace core


