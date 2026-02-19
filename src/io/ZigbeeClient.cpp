#include "ZigbeeClient.h"
#include <Preferences.h>
#include <esp_log.h>
#include <math.h>

#if HAVE_ARDUINO_ZIGBEE
#include <lvgl.h>
#include "domain/Metrics.h"
#include "core/Storage.h"
#endif

namespace io {

ZigbeeClient zigbee;

// ---- Stub implementations (non-Zigbee builds) -------------------------------

#if !HAVE_ARDUINO_ZIGBEE

bool     ZigbeeClient::begin(const ZigbeeConfig& cfg) { (void)cfg; return false; }
void     ZigbeeClient::loop()                          {}
void     ZigbeeClient::startCommission(uint8_t s)      { (void)s; }
void     ZigbeeClient::startJoined()                   {}
void     ZigbeeClient::factoryReset()                  { ESP.restart(); }
bool     ZigbeeClient::isStarted()   const             { return false; }
bool     ZigbeeClient::isJoined()    const             { return false; }
bool     ZigbeeClient::isConnected() const             { return false; }
bool     ZigbeeClient::everJoined()  const             { return false; }
uint32_t ZigbeeClient::commissionUntilMs() const       { return 0; }
void     ZigbeeClient::clearCommissionTimer()          {}

#else // HAVE_ARDUINO_ZIGBEE ---------------------------------------------------

static inline bool zb_is_joined() { return esp_zb_bdb_dev_joined(); }

// ---------- begin() ----------------------------------------------------------

bool ZigbeeClient::begin(const ZigbeeConfig& cfg) {
  _cfg = cfg;

  Preferences prefs;
  prefs.begin(PREF_NS, /*readOnly=*/true);
  bool doPair = prefs.getBool(PREF_PAIR,  false);
  _everJoined = prefs.getBool(PREF_BOUND, false);
  prefs.end();

  ESP_LOGI("ZB", "Commissioning flag: %d", doPair ? 1 : 0);
  ESP_LOGI("ZB", "Zigbee bound flag:  %d", _everJoined ? 1 : 0);

  if (doPair) {
    Preferences prefs2;
    prefs2.begin(PREF_NS, /*readOnly=*/false);
    prefs2.putBool(PREF_PAIR, false);
    prefs2.end();
  }

  return doPair; // caller should call startCommission() if true
}

// ---------- endpoint setup (shared by commission + joined) -------------------

void ZigbeeClient::_setupEndpoints(bool eraseNvs) {
  if (_started) return;

  _tempSensor.setManufacturerAndModel("PoolLab", "Pool Temperature");
  _tempSensor.setMinMaxValue(0, 60);
  _ph.setManufacturerAndModel("PoolLab", "Pool pH");
  _ph.setMinMaxValue(0.0f, 14.0f);
  _orp.setManufacturerAndModel("PoolLab", "Pool ORP");
  _orp.setMinMaxValue(-2000, 2000);

  _phMinEp.addAnalogOutput();
  _phMaxEp.addAnalogOutput();
  _orpMinEp.addAnalogOutput();
  _orpMaxEp.addAnalogOutput();

  _phMinEp.onAnalogOutputChange( [](float v){ zigbee.onPhMinWrite(v);  });
  _phMaxEp.onAnalogOutputChange( [](float v){ zigbee.onPhMaxWrite(v);  });
  _orpMinEp.onAnalogOutputChange([](float v){ zigbee.onOrpMinWrite(v); });
  _orpMaxEp.onAnalogOutputChange([](float v){ zigbee.onOrpMaxWrite(v); });

  Zigbee.setRxOnWhenIdle(true);
  Zigbee.setScanDuration(eraseNvs ? 4 : 3);
  Zigbee.setTimeout(120000);
  Zigbee.addEndpoint(&_tempSensor);
  Zigbee.addEndpoint(&_ph);
  Zigbee.addEndpoint(&_orp);
  Zigbee.addEndpoint(&_phMinEp);
  Zigbee.addEndpoint(&_phMaxEp);
  Zigbee.addEndpoint(&_orpMinEp);
  Zigbee.addEndpoint(&_orpMaxEp);
  ZigbeeEP::allowMultipleBinding(true);
}

// ---------- startCommission() ------------------------------------------------

void ZigbeeClient::startCommission(uint8_t seconds) {
  if (!_started) {
    _maskExpanded = false;
    _setupEndpoints(/*eraseNvs=*/true);

    Zigbee.setPrimaryChannelMask(1u << 11);
    bool ok = Zigbee.begin(ZIGBEE_ROUTER, /*eraseNvs=*/true);
    esp_zb_set_tx_power(20);
    ESP_LOGI("ZB", "begin(commission) -> %s", ok ? "OK" : "FAIL");
    (void)ok;

    _tempSensor.setReporting(1, 0, 1);
    _ph.setReporting(0, 30, 0.01f);
    _orp.setReporting(0, 30, 5);

    auto& M = domain::Metrics::instance();
    _tempSensor.setTemperature(M.haveTemp ? M.tempC : 25.0f);
    _tempSensor.reportTemperature();
    _ph.setFlow(M.havePh ? M.phVal : 7.00f);
    _ph.report();
    _orp.setPressure(M.haveOrp ? (int16_t)lrintf(M.orpMv) : (int16_t)300);
    _orp.report();

    // Delayed push after ZHA interview completes.
    lv_timer_t *t = lv_timer_create([](lv_timer_t *tm) {
      (void)tm;
      auto& M = domain::Metrics::instance();
      zigbee._ph.setFlow(M.havePh ? M.phVal : 7.00f);
      zigbee._ph.report();
      zigbee._orp.setPressure(M.haveOrp ? (int16_t)lrintf(M.orpMv) : (int16_t)300);
      zigbee._orp.report();
    }, 2000, NULL);
    lv_timer_set_repeat_count(t, 1);

    _started = true;
  }

  _commissionUntilMs = millis() + (uint32_t)seconds * 1000UL;
}

// ---------- startJoined() ----------------------------------------------------

void ZigbeeClient::startJoined() {
  if (_started) return;
  ESP_LOGI("ZB", "Start Zigbee (joined mode, no commissioning)");

  _setupEndpoints(/*eraseNvs=*/false);
  Zigbee.setPrimaryChannelMask(0x07FFF800);
  bool ok = Zigbee.begin(ZIGBEE_ROUTER, /*eraseNvs=*/false);
  esp_zb_set_tx_power(20);
  ESP_LOGI("ZB", "begin(joined) -> %s", ok ? "OK" : "FAIL");
  (void)ok;

  _tempSensor.setReporting(1, 0, 1);
  _ph.setReporting(0, 30, 0.01f);
  _orp.setReporting(0, 30, 5);

  _started = true;
}

// ---------- factoryReset() ---------------------------------------------------

void ZigbeeClient::factoryReset() {
  Zigbee.factoryReset(/*restart=*/true);
}

// ---------- accessors --------------------------------------------------------

bool     ZigbeeClient::isStarted()   const { return _started; }
bool     ZigbeeClient::isJoined()    const { return zb_is_joined(); }
bool     ZigbeeClient::isConnected() const { return Zigbee.connected(); }
bool     ZigbeeClient::everJoined()  const { return _everJoined; }
uint32_t ZigbeeClient::commissionUntilMs() const { return _commissionUntilMs; }
void     ZigbeeClient::clearCommissionTimer()     { _commissionUntilMs = 0; }

// ---------- loop() helpers ---------------------------------------------------

void ZigbeeClient::_applyDeferredWrites() {
  if (_phMinPending)  { _phMinPending  = false; *_cfg.phMin  = _phMinValue;               _cfg.storage->setPhMin(*_cfg.phMin);  }
  if (_phMaxPending)  { _phMaxPending  = false; *_cfg.phMax  = _phMaxValue;               _cfg.storage->setPhMax(*_cfg.phMax);  }
  if (_orpMinPending) { _orpMinPending = false; *_cfg.orpMin = (int)lrintf(_orpMinValue); _cfg.storage->setOrpMin(*_cfg.orpMin); }
  if (_orpMaxPending) { _orpMaxPending = false; *_cfg.orpMax = (int)lrintf(_orpMaxValue); _cfg.storage->setOrpMax(*_cfg.orpMax); }
}

void ZigbeeClient::_reportSensors() {
  auto& M = domain::Metrics::instance();
  if (M.havePh)   _ph.setFlow(M.phVal);
  if (M.haveOrp)  _orp.setPressure((int16_t)lrintf(M.orpMv));
  if (M.haveTemp) _tempSensor.setTemperature(M.tempC);
}

void ZigbeeClient::_saveBoundFlagIfNeeded() {
  if (!_everJoined && Zigbee.connected()) {
    _everJoined = true;
    Preferences prefs;
    prefs.begin(PREF_NS, /*readOnly=*/false);
    prefs.putBool(PREF_BOUND, true);
    prefs.end();
    ESP_LOGI("ZB", "Bound flag saved to NVS");
  }
}

void ZigbeeClient::loop() {
  if (!_started) return;
  _applyDeferredWrites();
  _reportSensors();
  _saveBoundFlagIfNeeded();
}

#endif // HAVE_ARDUINO_ZIGBEE

} // namespace io
