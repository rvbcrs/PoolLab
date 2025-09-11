#pragma once

#include <WiFiClient.h>
#include <PubSubClient.h>
#include "core/Storage.h"
#include "domain/Metrics.h"

namespace io {

// ---- MQTT Topics (centralized here) ----
static const char* TOPIC_STATE_PH   = "pool/sensor/ph";
static const char* TOPIC_STATE_ORP  = "pool/sensor/orp";
static const char* TOPIC_STATE_TEMP = "pool/sensor/temp";
static const char* DISCOVERY_PH     = "homeassistant/sensor/pool_ph/config";
static const char* DISCOVERY_ORP    = "homeassistant/sensor/pool_orp/config";
static const char* DISCOVERY_TEMP   = "homeassistant/sensor/pool_temp/config";

static const char* TOPIC_CFG_PH_MIN   = "pool/cfg/ph_min";
static const char* TOPIC_CFG_PH_MAX   = "pool/cfg/ph_max";
static const char* TOPIC_CFG_ORP_MIN  = "pool/cfg/orp_min";
static const char* TOPIC_CFG_ORP_MAX  = "pool/cfg/orp_max";
static const char* TOPIC_CMD_PH_MIN   = "pool/cmd/ph_min";
static const char* TOPIC_CMD_PH_MAX   = "pool/cmd/ph_max";
static const char* TOPIC_CMD_ORP_MIN  = "pool/cmd/orp_min";
static const char* TOPIC_CMD_ORP_MAX  = "pool/cmd/orp_max";

class MqttClient {
public:
  MqttClient();
  void setStorage(core::Storage *s);
  void setThresholdRefs(float *phMin, float *phMax, int *orpMin, int *orpMax);
  void begin(const char* host, uint16_t port, const char* user, const char* pass, const char* clientId);
  void ensureConnected();
  bool isConnected();
  void publishDiscoveryOnce();
  void publishStatesIfReady(const domain::Metrics &m);
  void loop();

private:
  void onMessage(char* topic, uint8_t* payload, unsigned int length);

  WiFiClient _wifi;
  PubSubClient _client;
  core::Storage *_storage = nullptr;
  const char* _host;
  uint16_t _port;
  const char* _user;
  const char* _pass;
  const char* _clientId;
  bool _announced = false;
  float *_phMin, *_phMax;
  int *_orpMin, *_orpMax;
};

} // namespace io


