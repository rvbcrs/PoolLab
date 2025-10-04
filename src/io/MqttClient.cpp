#include "MqttClient.h"
#include <Arduino.h>

namespace io {

MqttClient::MqttClient() : _client(_wifi) {}

void MqttClient::setStorage(core::Storage *s) { _storage = s; }

void MqttClient::setThresholdRefs(float *phMin, float *phMax, int *orpMin, int *orpMax) {
  _phMin = phMin; _phMax = phMax; _orpMin = orpMin; _orpMax = orpMax;
}

void MqttClient::begin(const char* host, uint16_t port, const char* user, const char* pass, const char* clientId) {
  _host = host; _port = port; _user = user; _pass = pass; _clientId = clientId;
  uint16_t effectivePort = (_port == 0) ? 1883 : _port;
  _client.setServer(_host, effectivePort);
  _client.setCallback([this](char* topic, uint8_t* payload, unsigned int length){
    if (_debug) ESP_LOGI("MQTT", "Message received on '%s' (%u bytes)", topic, length);
    this->onMessage(topic, payload, length);
  });
  if (_debug) {
    if (_port > 0) ESP_LOGI("MQTT", "Configured client: host=%s port=%u user=%s clientId=%s", _host?_host:"", _port, _user?_user:"", _clientId?_clientId:"");
    else ESP_LOGI("MQTT", "Configured client: host=%s (default port 1883) user=%s clientId=%s", _host?_host:"", _user?_user:"", _clientId?_clientId:"");
  }
}

void MqttClient::ensureConnected() {
  if (isConnected()) return;
  if (_host == nullptr || _host[0] == '\0') return; // skip if no broker configured
  uint32_t now = millis();
  if (_nextTryAtMs && (int32_t)(now - _nextTryAtMs) < 0) {
    return; // backoff window active
  }
  if (_debug) {
    ESP_LOGI("MQTT", "Connecting to broker %s%s%u...", _host, (_port>0?":":" (default 1883, port="), (_port>0?_port:1883));
    ESP_LOGI("MQTT", "Auth %s user='%s' passlen=%u clientId=%s",
             (_user && _user[0])?"ON":"OFF",
             _user ? _user : "",
             (unsigned)(_pass ? strlen(_pass) : 0),
             _clientId ? _clientId : "");
  }
  bool connected = false;
  if (_user && _user[0]) {
    if (_debug) ESP_LOGI("MQTT", "Using username/password for connect");
    connected = _client.connect(_clientId, _user, _pass);
  } else {
    if (_debug) ESP_LOGI("MQTT", "Connecting without auth");
    connected = _client.connect(_clientId);
  }
  if (connected) {
    if (_debug) ESP_LOGI("MQTT", "Connected. Subscribing to cmd topics...");
    _client.subscribe(TOPIC_CMD_PH_MIN);
    _client.subscribe(TOPIC_CMD_PH_MAX);
    _client.subscribe(TOPIC_CMD_ORP_MIN);
    _client.subscribe(TOPIC_CMD_ORP_MAX);
    _announced = false; // allow discovery publish again after reconnect
    _backoffMs = 5000;  // reset backoff on success
    _nextTryAtMs = 0;
  } else {
    if (_debug) ESP_LOGE("MQTT", "Connect failed. state=%d", _client.state());
    // Exponential backoff up to 5 minutes to keep UI responsive
    if (_backoffMs < 300000) _backoffMs = _backoffMs * 2;
    if (_backoffMs > 300000) _backoffMs = 300000;
    _nextTryAtMs = now + _backoffMs;
  }
}

bool MqttClient::isConnected() {
  return _client.connected();
}

void MqttClient::onMessage(char* topic, uint8_t* payload, unsigned int length) {
  String t(topic);
  String v;
  for (unsigned int i=0;i<length;i++) v += (char)payload[i];
  v.trim();
  if (_debug) ESP_LOGI("MQTT", "Handling msg topic=%s payload='%s'", t.c_str(), v.c_str());
  float f = v.toFloat();
  if (t == TOPIC_CMD_PH_MIN)  {
    if (!isnan(f) && _phMin && _storage) { *_phMin = f; _storage->setPhMin(f); _client.publish(TOPIC_CFG_PH_MIN, v.c_str(), true); if (_debug) ESP_LOGI("MQTT","Set PH_MIN=%.2f", f);} else if (_debug) ESP_LOGE("MQTT","Invalid PH_MIN payload");
  } else if (t == TOPIC_CMD_PH_MAX) {
    if (!isnan(f) && _phMax && _storage) { *_phMax = f; _storage->setPhMax(f); _client.publish(TOPIC_CFG_PH_MAX, v.c_str(), true); if (_debug) ESP_LOGI("MQTT","Set PH_MAX=%.2f", f);} else if (_debug) ESP_LOGE("MQTT","Invalid PH_MAX payload");
  } else if (t == TOPIC_CMD_ORP_MIN){
    if (!isnan(f) && _orpMin && _storage) { *_orpMin = (int)lrintf(f); _storage->setOrpMin(*_orpMin); _client.publish(TOPIC_CFG_ORP_MIN, String(*_orpMin).c_str(), true); if (_debug) ESP_LOGI("MQTT","Set ORP_MIN=%d", *_orpMin);} else if (_debug) ESP_LOGE("MQTT","Invalid ORP_MIN payload");
  } else if (t == TOPIC_CMD_ORP_MAX){
    if (!isnan(f) && _orpMax && _storage) { *_orpMax = (int)lrintf(f); _storage->setOrpMax(*_orpMax); _client.publish(TOPIC_CFG_ORP_MAX, String(*_orpMax).c_str(), true); if (_debug) ESP_LOGI("MQTT","Set ORP_MAX=%d", *_orpMax);} else if (_debug) ESP_LOGE("MQTT","Invalid ORP_MAX payload");
  }
}

void MqttClient::publishDiscoveryOnce() {
  if (_announced || !_client.connected()) return;
  if (_debug) ESP_LOGI("MQTT", "Publishing HA discovery...");
  String ph;   ph.reserve(160);
  ph   = F("{\"name\":\"Pool pH\",\"state_topic\":\"");
  ph  += TOPIC_STATE_PH;
  ph  += F("\",\"unit_of_measurement\":\"pH\",\"unique_id\":\"pool_ph\",\"icon\":\"mdi:beaker-outline\"}");

  String orp;  orp.reserve(160);
  orp  = F("{\"name\":\"Pool ORP\",\"state_topic\":\"");
  orp += TOPIC_STATE_ORP;
  orp += F("\",\"unit_of_measurement\":\"mV\",\"unique_id\":\"pool_orp\",\"icon\":\"mdi:flash\"}");

  String temp; temp.reserve(200);
  temp = F("{\"name\":\"Pool Temp\",\"state_topic\":\"");
  temp+= TOPIC_STATE_TEMP;
  temp+= F("\",\"unit_of_measurement\":\"°C\",\"device_class\":\"temperature\",\"unique_id\":\"pool_temp\"}");
  
  _client.publish(DISCOVERY_PH, ph.c_str(), true);
  _client.publish(DISCOVERY_ORP, orp.c_str(), true);
  _client.publish(DISCOVERY_TEMP, temp.c_str(), true);
  _announced = true;
}

void MqttClient::publishStatesIfReady(const domain::Metrics &m) {
  if (!_client.connected()) return;
  static uint32_t lastPub=0; uint32_t now=millis();
  if (now - lastPub < 1000) return; // rate limit
  lastPub = now;
  if (_debug) ESP_LOGI("MQTT", "Publish states (ph=%s, orp=%s, temp=%s)", m.havePh?"yes":"no", m.haveOrp?"yes":"no", m.haveTemp?"yes":"no");
  if (m.havePh)   { char b[16]; snprintf(b,sizeof(b),"%.2f", m.phVal); _client.publish(TOPIC_STATE_PH, b, true); if (_debug) ESP_LOGI("MQTT","Published %s=%s", TOPIC_STATE_PH, b); }
  if (m.haveOrp)  { char b[16]; snprintf(b,sizeof(b),"%d", (int)lrintf(m.orpMv)); _client.publish(TOPIC_STATE_ORP, b, true); if (_debug) ESP_LOGI("MQTT","Published %s=%s", TOPIC_STATE_ORP, b); }
  if (m.haveTemp) { char b[16]; snprintf(b,sizeof(b),"%.1f", m.tempC); _client.publish(TOPIC_STATE_TEMP, b, true); if (_debug) ESP_LOGI("MQTT","Published %s=%s", TOPIC_STATE_TEMP, b); }
}

void MqttClient::publishAlert(const char* alertType, const char* message) {
  if (!_client.connected()) return;
  
  // Publish to pool/alert/<alert_type> topic
  String topic = "pool/alert/";
  topic += alertType;
  
  _client.publish(topic.c_str(), message, true);  // retained for persistence
  ESP_LOGI("MQTT", "Published alert: %s = %s", topic.c_str(), message);
}

void MqttClient::loop() {
  _client.loop();
}

} // namespace io


