#include "WiFiManager.h"
#include <esp_log.h>

namespace io {

void WiFiManager::begin(const String &ssid, const String &pass, const char *clientPrefix, IpCallback onIpUpdate){
  _ssid = ssid; _pass = pass; _clientPrefix = clientPrefix ? clientPrefix : "poollab"; _onIp = onIpUpdate;
  setupEvents();
  uint64_t chipid = ESP.getEfuseMac(); char host[32];
  snprintf(host, sizeof(host), "%s-%06llX", _clientPrefix.c_str(), (unsigned long long)(chipid & 0xFFFFFFULL));
  ESP_LOGI("WiFi", "Boot: hostname=%s", host);
  if (_ssid.length() == 0) {
    ESP_LOGI("WiFi", "Boot: no SSID -> starting captive portal");
    startPortal("PoolLab-Setup");
    if (_onIp) _onIp(WiFi.softAPIP().toString());
    return;
  }
  ESP_LOGI("WiFi", "Boot: STA starting (ssid='%s')", _ssid.c_str());
  ensureSta();
}

void WiFiManager::setupEvents(){
  WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info){
    switch(event){
      case ARDUINO_EVENT_WIFI_STA_CONNECTED:
        ESP_LOGI("WiFi","STA connected to AP");
        _connecting = false; break;
      case ARDUINO_EVENT_WIFI_STA_GOT_IP: {
        _connecting = false; _failCount = 0;
        String ip = WiFi.localIP().toString();
        uint64_t chipid = ESP.getEfuseMac(); char host[32];
        snprintf(host, sizeof(host), "%s-%06llX", _clientPrefix.c_str(), (unsigned long long)(chipid & 0xFFFFFFULL));
        ArduinoOTA.setHostname(host);
        ArduinoOTA.begin();
        ESP_LOGI("WiFi","GOT_IP=%s hostname=%s", ip.c_str(), host);
        if (_onIp) _onIp(ip);
        break; }
      case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        ESP_LOGI("WiFi","Disconnected reason=%d", (int)info.wifi_sta_disconnected.reason);
        _connecting = false; _failCount++;
        if (_onIp) _onIp(String("--"));
        if (_failCount > 3) {
          if (_ssid.length() == 0) {
            ESP_LOGI("WiFi","Too many fails and no SSID -> Captive Portal");
            startPortal("PoolLab-Setup");
            if (_onIp) _onIp(WiFi.softAPIP().toString());
          } else {
            ESP_LOGI("WiFi","Too many fails but SSID is set -> keep STA retries, no portal");
          }
        }
        break;
      default: break;
    }
  });
}

void WiFiManager::ensureSta(){
  WiFi.setHostname("poollab");
  WiFi.persistent(false);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  // Avoid switching out of AP mode when no credentials
  if (_ssid.length() == 0) return;
  WiFi.mode(WIFI_STA);
  if (WiFi.status() == WL_CONNECTED || _connecting) return;
  hardRestart();
}

void WiFiManager::hardRestart(){
  WiFi.disconnect(true, true);
  delay(150);
  WiFi.mode(WIFI_STA);
  ESP_LOGI("WiFi","Connecting to '%s'", _ssid.c_str());
  WiFi.begin(_ssid.c_str(), _pass.c_str());
  _connecting = true; _lastAttemptMs = millis();
}

void WiFiManager::startPortal(const String &apPrefix){
  // Stay in AP+STA so we can switch to STA after saving credentials
  WiFi.mode(WIFI_AP_STA);
  // Disable auto reconnect to prevent STA from killing AP immediately
  WiFi.setAutoReconnect(false);
  String ssid = apPrefix; uint64_t chipid = ESP.getEfuseMac(); char suffix[16];
  snprintf(suffix, sizeof(suffix), "-%06llX", (unsigned long long)(chipid & 0xFFFFFFULL));
  ssid += suffix;
  WiFi.softAP(ssid.c_str());
  ESP_LOGI("WiFi","Portal SSID=%s IP=%s", ssid.c_str(), WiFi.softAPIP().toString().c_str());
}

void WiFiManager::loop(){
  uint32_t now = millis();
  // When portal is active (SSID empty), keep AP alive and do not attempt STA reconnects
  if (_ssid.length() == 0) return;
  if (WiFi.getMode() == WIFI_MODE_STA || WiFi.getMode() == WIFI_MODE_APSTA) {
    if (WiFi.status() != WL_CONNECTED) {
      if (!_connecting && now - _lastAttemptMs > 5000) { _lastAttemptMs = now; ensureSta(); }
      if (_connecting && now - _lastAttemptMs > 12000) { ESP_LOGI("WiFi","Retry hard restart"); hardRestart(); }
    }
  }
}

} // namespace io


