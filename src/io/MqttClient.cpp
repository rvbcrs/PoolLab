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
  _client.setServer(_host, _port);
  _client.setCallback([this](char* topic, uint8_t* payload, unsigned int length){
    this->onMessage(topic, payload, length);
  });
}

void MqttClient::ensureConnected() {
  if (isConnected()) return;
  
  if (_client.connect(_clientId, _user, _pass)) {
    _client.subscribe(TOPIC_CMD_PH_MIN);
    _client.subscribe(TOPIC_CMD_PH_MAX);
    _client.subscribe(TOPIC_CMD_ORP_MIN);
    _client.subscribe(TOPIC_CMD_ORP_MAX);
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
  float f = v.toFloat();
  if (t == TOPIC_CMD_PH_MIN)  {
    if (!isnan(f) && _phMin && _storage) { *_phMin = f; _storage->setPhMin(f); _client.publish(TOPIC_CFG_PH_MIN, v.c_str(), true); }
  } else if (t == TOPIC_CMD_PH_MAX) {
    if (!isnan(f) && _phMax && _storage) { *_phMax = f; _storage->setPhMax(f); _client.publish(TOPIC_CFG_PH_MAX, v.c_str(), true); }
  } else if (t == TOPIC_CMD_ORP_MIN){
    if (!isnan(f) && _orpMin && _storage) { *_orpMin = (int)lrintf(f); _storage->setOrpMin(*_orpMin); _client.publish(TOPIC_CFG_ORP_MIN, String(*_orpMin).c_str(), true); }
  } else if (t == TOPIC_CMD_ORP_MAX){
    if (!isnan(f) && _orpMax && _storage) { *_orpMax = (int)lrintf(f); _storage->setOrpMax(*_orpMax); _client.publish(TOPIC_CFG_ORP_MAX, String(*_orpMax).c_str(), true); }
  }
}

void MqttClient::publishDiscoveryOnce() {
  if (_announced || !_client.connected()) return;
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
  if (m.havePh)   { char b[16]; snprintf(b,sizeof(b),"%.2f", m.phVal); _client.publish(TOPIC_STATE_PH, b, true); }
  if (m.haveOrp)  { char b[16]; snprintf(b,sizeof(b),"%d", (int)lrintf(m.orpMv)); _client.publish(TOPIC_STATE_ORP, b, true); }
  if (m.haveTemp) { char b[16]; snprintf(b,sizeof(b),"%.1f", m.tempC); _client.publish(TOPIC_STATE_TEMP, b, true); }
}

void MqttClient::loop() {
  _client.loop();
}

} // namespace io


