#pragma once

#include <Arduino.h>

// Feature detection — same logic as ZB_ENABLED in main.cpp.
// P4 has HAS_ZIGBEE=0 in platformio.ini, so HAVE_ARDUINO_ZIGBEE will be 0 there.
#if defined(HAS_ZIGBEE) && (HAS_ZIGBEE == 0)
  #define HAVE_ARDUINO_ZIGBEE 0
#elif __has_include(<Zigbee.h>)
  #include <Zigbee.h>
  #include <ZigbeeEP.h>
  #include <ep/ZigbeeTempSensor.h>
  #include <ep/ZigbeeAnalog.h>
  #include <ep/ZigbeeFlowSensor.h>
  #include <ep/ZigbeePressureSensor.h>
  #include <esp_zigbee_secur.h>
  #include <esp_zigbee_core.h>
  #define HAVE_ARDUINO_ZIGBEE 1
#else
  #define HAVE_ARDUINO_ZIGBEE 0
#endif

// Forward declarations to avoid circular includes.
namespace core { class Storage; }

namespace io {

struct ZigbeeConfig {
  float*         phMin   = nullptr;  // pointer to application PH_MIN float
  float*         phMax   = nullptr;  // pointer to application PH_MAX float
  int*           orpMin  = nullptr;  // pointer to application ORP_MIN int
  int*           orpMax  = nullptr;  // pointer to application ORP_MAX int
  core::Storage* storage = nullptr;  // for persisting threshold changes
};

class ZigbeeClient {
public:
  // Initialise client, load NVS pairing state.
  // Returns true if a deferred commission was pending (caller should call startCommission).
  bool begin(const ZigbeeConfig& cfg);

  // Periodic maintenance: apply deferred threshold writes, push sensor values.
  // Call every ~2 s from the main loop when isStarted().
  void loop();

  // Start Zigbee in commissioning mode (factory-new, opens join window).
  void startCommission(uint8_t seconds);

  // Start Zigbee without commissioning (uses stored network params).
  void startJoined();

  // Perform Zigbee factory reset (erases NVS network, then reboots).
  void factoryReset();

  bool     isStarted()          const;
  bool     isJoined()           const;
  bool     isConnected()        const;
  bool     everJoined()         const;
  uint32_t commissionUntilMs()  const;
  void     clearCommissionTimer();

  // Called from ZCL AO callbacks (public so plain lambdas can reach them).
#if HAVE_ARDUINO_ZIGBEE
  void onPhMinWrite(float v)   { _phMinValue  = v; _phMinPending  = true; }
  void onPhMaxWrite(float v)   { _phMaxValue  = v; _phMaxPending  = true; }
  void onOrpMinWrite(float v)  { _orpMinValue = v; _orpMinPending = true; }
  void onOrpMaxWrite(float v)  { _orpMaxValue = v; _orpMaxPending = true; }
#else
  void onPhMinWrite(float)  {}
  void onPhMaxWrite(float)  {}
  void onOrpMinWrite(float) {}
  void onOrpMaxWrite(float) {}
#endif

private:
  static constexpr const char* PREF_NS    = "pura";
  static constexpr const char* PREF_PAIR  = "zb_pair";
  static constexpr const char* PREF_BOUND = "zb_bound";

#if HAVE_ARDUINO_ZIGBEE
  ZigbeeConfig _cfg;
  bool         _started           = false;
  bool         _everJoined        = false;
  uint32_t     _commissionUntilMs = 0;
  bool         _maskExpanded      = false;

  // Deferred threshold writes (set from ZCL context, applied in loop()).
  volatile bool  _phMinPending  = false, _phMaxPending  = false;
  volatile bool  _orpMinPending = false, _orpMaxPending = false;
  volatile float _phMinValue    = 0,     _phMaxValue    = 0;
  volatile float _orpMinValue   = 0,     _orpMaxValue   = 0;

  // Zigbee endpoints (IDs match original implementation).
  ZigbeeTempSensor     _tempSensor{10};
  ZigbeeFlowSensor     _ph{11};
  ZigbeePressureSensor _orp{12};
  ZigbeeAnalog         _phMinEp{13};
  ZigbeeAnalog         _phMaxEp{14};
  ZigbeeAnalog         _orpMinEp{15};
  ZigbeeAnalog         _orpMaxEp{16};

  void _setupEndpoints(bool eraseNvs);
  void _applyDeferredWrites();
  void _reportSensors();
  void _saveBoundFlagIfNeeded();
#endif
};

extern ZigbeeClient zigbee;

} // namespace io
