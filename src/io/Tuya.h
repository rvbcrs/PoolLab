#pragma once

#include <Arduino.h>

namespace io {

// Configure DP ids used by the Tuya parser
void tuyaConfigure(uint8_t dpTemp, uint8_t dpOrp, uint8_t dpPh, uint8_t dpOrpAlt1, uint8_t dpPhAlt1);

// Feed one byte to parsers (A/B directions)
void tuyaFeedA(uint8_t b);
void tuyaFeedB(uint8_t b);

} // namespace io


