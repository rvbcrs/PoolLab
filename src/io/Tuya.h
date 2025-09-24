#pragma once

#include <Arduino.h>

namespace io {

// Configure DP ids used by the Tuya parser
void tuyaConfigure(uint8_t dpTemp, uint8_t dpOrp, uint8_t dpPh, uint8_t dpOrpAlt1, uint8_t dpPhAlt1);

// Feed one byte to parsers (A/B directions)
void tuyaFeedA(uint8_t b);
void tuyaFeedB(uint8_t b);

// Frame helpers (move from main)
uint8_t tuyaChecksum(const uint8_t* p, size_t n);
void tuyaSendFrame(HardwareSerial &port, uint8_t cmd, const uint8_t* data, uint16_t len);
void tuyaSendDpQuery(HardwareSerial &port);           // cmd 0x10
void tuyaSendQueryProductInfo(HardwareSerial &port);  // cmd 0x01
void tuyaSendSetWifiStatus(HardwareSerial &port, uint8_t status); // cmd 0x03

} // namespace io


