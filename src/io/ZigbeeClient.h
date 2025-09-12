#pragma once

#include <Arduino.h>
#if __has_include(<Zigbee.h>)
#include <Zigbee.h>
#define HAVE_ARDUINO_ZIGBEE 1
#else
#define HAVE_ARDUINO_ZIGBEE 0
#endif
// Prefer Arduino Zigbee if present; avoid hard dependency on ESP-IDF Zigbee.
// Only enable IDF Zigbee if explicitly requested via ENABLE_IDF_ZIGBEE.
#if defined(ENABLE_IDF_ZIGBEE) && __has_include(<esp_zigbee_core.h>)
#include <esp_zigbee_core.h>
#define HAVE_IDF_ZIGBEE 1
#else
#define HAVE_IDF_ZIGBEE 0
#endif

namespace io {

struct ZigbeeConfig {
  // reserved for future: network/channel/pan id, device info
};

class ZigbeeClient {
public:
  void begin(const ZigbeeConfig &cfg) {
    (void)cfg;
  #if HAVE_ARDUINO_ZIGBEE
    // Do not start Zigbee automatically; wait for explicit commissioning request
    _ready = false;
  #else
    _ready = false; // Zigbee stack not available on this target
  #endif
  }

  void loop() {
    // placeholder for future Zigbee event pump
  }

  bool startCommissioning(uint32_t durationSeconds = 60) {
  #if HAVE_ARDUINO_ZIGBEE
    // Set a window; upper layer will open network and start stack
    _commissioningUntil = millis() + (uint32_t)min<uint32_t>(durationSeconds, 60) * 1000UL;
    return true;
  #else
    _commissioningUntil = millis() + durationSeconds * 1000UL;
    return false;
  #endif
  }

  bool isCommissioning() const {
    return millis() < _commissioningUntil;
  }

  bool isReady() const {
    return _ready;
  }

  // Stubs for reporting; in real stack map to ZCL attributes and reports
  void reportPh(float ph) {
  #if HAVE_ARDUINO_ZIGBEE
    (void)ph; // map to Analog Input cluster attribute in user code later
  #else
    (void)ph;
  #endif
  }
  void reportOrp(int orpMv) {
  #if HAVE_ARDUINO_ZIGBEE
    (void)orpMv;
  #else
    (void)orpMv;
  #endif
  }
  void reportTemp(float tempC) {
  #if HAVE_ARDUINO_ZIGBEE
    if (_ready) {
      // Use default endpoint 10 temp sensor if present; else no-op
      // This is a placeholder; the real report is handled in main via zbTempSensor
    }
  #else
    (void)tempC;
  #endif
  }

private:
  bool _ready = false;
  uint32_t _commissioningUntil = 0;
};

} // namespace io


