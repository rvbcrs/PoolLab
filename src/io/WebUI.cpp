#include "WebUI.h"
#include "MotorController.h"
#include <WiFi.h>
#include <SPIFFS.h>
#include <FS.h>

namespace io {

extern "C" void requestModeChange(int mode);
extern "C" void requestMqttReload();

void WebUI::begin(){
  if (_active) return;
  if (_storage) { _storage->begin(false); }
  _http.on("/", [this](){ handleIndex(); });
  _http.on("/settings", [this](){ handleSettings(); });
  _http.on("/safety", [this](){ handleSafety(); });
  _http.on("/console", [this](){ handleConsole(); }); // Console endpoint
  _http.on("/api/state", [this](){ handleApiState(); });
  _http.on("/api/save", HTTP_POST, [this](){ handleApiSave(); });
  _http.on("/api/test_alert", [this](){ 
    // Trigger test safety alert (pH sanity high) via ControlPolicy
    if (_motor) {
      ESP_LOGI("WebUI", "Test alert triggered from WebUI");
      _motor->triggerTestAlert(domain::SafetyAlert::PH_SANITY_HIGH);
    }
    _http.send(200, "text/plain", "Test alert sent!");
  });
  _http.begin();
  _ws.begin();
  _ws.onEvent([](uint8_t num, WStype_t type, uint8_t * payload, size_t length){ (void)num; (void)type; (void)payload; (void)length; });
  _active = true;
}

void WebUI::loop(){ if (_active) { _http.handleClient(); _ws.loop(); } }
void WebUI::stop(){ if (_active) { _http.stop(); _active=false; } }

static String fmtFloat(float v, int d){ char b[24]; dtostrf(v, 0, d, b); return String(b); }

void WebUI::sendStyleHeader(String &h){
  h  = F("<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>");
  h += F("<style>body{font-family:Arial,Helvetica,sans-serif;background:#111;color:#e3e3e3;margin:0;padding:16px;} .card{background:#1b1b1b;border-radius:10px;padding:16px;max-width:900px;margin:0 auto;box-shadow:0 2px 12px rgba(0,0,0,.4);} h2,h3{margin:0 0 12px 0;} label{display:block;margin:10px 0 6px;} input{width:100%;padding:10px;border-radius:8px;border:1px solid #333;background:#222;color:#fff;box-sizing:border-box;} button{width:100%;padding:12px;border-radius:8px;border:none;background:#4caf50;color:#fff;box-sizing:border-box;cursor:pointer;font-weight:600;font-size:15px;transition:background .2s;} button:hover{background:#66bb6a;} button:active{background:#388e3c;} .row{display:flex;gap:8px;} .row>*{flex:1;} .btn{cursor:pointer;border:none;} .btn.red{background:#b00020;} .btn.blue{background:#1976d2;} .btn.orange{background:#ff9800;} .muted{color:#aaa;font-size:12px;margin-top:8px;display:block;} .tiles{display:grid;grid-template-columns:repeat(3,1fr);gap:12px;margin:8px 0 14px;} .tile{position:relative;border-radius:12px;padding:12px;box-shadow:inset 0 1px 0 rgba(255,255,255,.03),0 1px 6px rgba(0,0,0,.35);border:1px solid rgba(255,255,255,.04);} .tile .title{color:#d6d6d6;font-size:13px;margin:6px 0 2px;display:block;letter-spacing:.3px} .tile .val{font-size:28px;font-weight:700;letter-spacing:.3px;margin:2px 0 6px} .icon{display:inline-flex;align-items:center;justify-content:center;width:32px;height:32px;border-radius:50%;margin-bottom:4px;font-size:18px} .tile .pump{position:absolute;left:10px;bottom:10px;opacity:.9;font-size:18px;display:none} .warn{color:#ffa000} .bad{color:#ff5252} .tile.blue{background:linear-gradient(180deg,#1d2938,#141a22);} .tile.blue .icon{background:#0c2a3f;color:#5ec8ff;} .tile.orange{background:linear-gradient(180deg,#2e2418,#1c1711);} .tile.orange .icon{background:#3a220c;color:#ffb74d;} .tile.teal{background:linear-gradient(180deg,#19312e,#121e1c);} .tile.teal .icon{background:#0c2f28;color:#7fe3cf;} .pump-stats{background:#222;border-radius:8px;padding:12px;margin:8px 0 14px;} .stat-row{display:flex;justify-content:space-between;align-items:center;padding:6px 0;border-bottom:1px solid #333;} .stat-row:last-child{border-bottom:none;} .stat-label{color:#aaa;font-size:14px;} .stat-val{color:#e3e3e3;font-size:14px;font-weight:600;text-align:right;}</style></head><body>");
}
void WebUI::sendFooter(String &h){ h += F("</body></html>"); }

void WebUI::handleIndex(){
  String html; sendStyleHeader(html);
  html += F("<div class='card'><h2>PoolLab</h2><div class='tiles'>");
  // pH tile
  html += F("<div class='tile blue'><div class='icon'>💧</div><span class='title'>pH</span><div class='val' id='phVal'>--.--</div><div class='muted'>Target: <span id='ph_range'>");
  html += (_phMin && _phMax) ? (fmtFloat(*_phMin,2)+String(" - ")+fmtFloat(*_phMax,2)) : String("--");
  html += F("</span></div><div class='pump' id='pump_ph'>🌀</div></div>");
  // ORP tile
  html += F("<div class='tile orange'><div class='icon'>⚡</div><span class='title'>ORP</span><div class='val' id='orpVal'>----</div><div class='muted'>Min/Max: <span id='orp_range'>");
  html += (_orpMin && _orpMax) ? (String(*_orpMin)+String(" / ")+String(*_orpMax)) : String("--");
  html += F("</span></div><div class='pump' id='pump_orp'>🌀</div></div>");
  // Temp tile
  html += F("<div class='tile teal'><div class='icon'>🌡️</div><span class='title'>Temp</span><div class='val' id='tempVal'>--.- °C</div></div>");
  html += F("</div>");
  // Pump stats display
  html += F("<div class='pump-stats'><div class='stat-row'><div class='stat-label'>pH Pump:</div><div class='stat-val' id='ph_stats'>--</div></div>");
  html += F("<div class='stat-row'><div class='stat-label'>ORP Pump:</div><div class='stat-val' id='orp_stats'>--</div></div></div>");
  html += F("<div class='row'><a href='/settings'><button class='btn blue'>Settings</button></a>");
  html += F("<a href='/safety'><button class='btn orange' style='margin-left:8px'>⚠️ Safety</button></a>");
  html += F("<a href='/console'><button class='btn teal' style='margin-left:8px;width:auto;padding:12px 16px'>🖥️</button></a></div>");
  html += F("<span class='muted'>IP: "); html += WiFi.localIP().toString(); html += F("</span></div>");
  // thresholds for client-side coloring
  float phMin = _phMin? *_phMin : 6.80f; float phMax = _phMax? *_phMax : 7.60f; int orpMin = _orpMin? *_orpMin : 250; int orpMax = _orpMax? *_orpMax : 850;
  html += F("<script>");
  html += "var PH_MIN="+String(phMin,2)+",PH_MAX="+String(phMax,2)+",ORP_MIN="+String(orpMin)+",ORP_MAX="+String(orpMax)+";";
  html += F("function cls(el,c){el.classList.remove('bad');el.classList.remove('warn'); if(c) el.classList.add(c);} ");
  html += F("function nearPh(v){return (v<=PH_MIN+0.05)||(v>=PH_MAX-0.05);} function nearOrp(v){return (v<=ORP_MIN+20)||(v>=ORP_MAX-20);} ");
  html += F("function fmtVol(ml){return ml>1000?(ml/1000).toFixed(2)+'L':ml.toFixed(1)+'ml';} var ws=new WebSocket('ws://'+location.host+':81/'); ws.onmessage=function(e){try{var d=JSON.parse(e.data); if(d.ph!==undefined){var el=document.getElementById('phVal'); if(d.ph===null){el.innerText='--.--'; cls(el,null);} else {el.innerText=d.ph; var v=parseFloat(d.ph); if(isFinite(v)){ if(v<PH_MIN||v>PH_MAX){cls(el,'bad');} else if(nearPh(v)){cls(el,'warn');} else {cls(el,null);} var pump=(v<PH_MIN||v>PH_MAX); document.getElementById('pump_ph').style.display=pump?'block':'none'; }}} if(d.orp!==undefined){var el2=document.getElementById('orpVal'); if(d.orp===null){el2.innerText='----'; cls(el2,null);} else {el2.innerText=d.orp+' mV'; var v2=parseInt(d.orp); if(isFinite(v2)){ if(v2<ORP_MIN||v2>ORP_MAX){cls(el2,'bad');} else if(nearOrp(v2)){cls(el2,'warn');} else {cls(el2,null);} var pump2=(v2<ORP_MIN||v2>ORP_MAX); document.getElementById('pump_orp').style.display=pump2?'block':'none'; }}} if(d.temp!==undefined){document.getElementById('tempVal').innerText=d.temp===null?'--.- °C':(d.temp+' °C');} if(d.pump_ph){var st=''; if(d.pump_ph.active){st+='🌀 '+d.pump_ph.session.toFixed(1)+'ml @ '+d.pump_ph.flow.toFixed(1)+'ml/min | Daily: '+fmtVol(d.pump_ph.daily)+' | Total: '+fmtVol(d.pump_ph.total);} else {st='Idle | Daily: '+fmtVol(d.pump_ph.daily)+' | Total: '+fmtVol(d.pump_ph.total);} document.getElementById('ph_stats').innerText=st;} if(d.pump_orp){var st2=''; if(d.pump_orp.active){st2+='🌀 '+d.pump_orp.session.toFixed(1)+'ml @ '+d.pump_orp.flow.toFixed(1)+'ml/min | Daily: '+fmtVol(d.pump_orp.daily)+' | Total: '+fmtVol(d.pump_orp.total);} else {st2='Idle | Daily: '+fmtVol(d.pump_orp.daily)+' | Total: '+fmtVol(d.pump_orp.total);} document.getElementById('orp_stats').innerText=st2;}}catch(_){} };</script>");
  sendFooter(html);
  _http.send(200, "text/html; charset=UTF-8", html);
}

void WebUI::handleSettings(){
  String html; sendStyleHeader(html);
  html += F("<div class='card'><h3>⚙️ Settings</h3>");
  html += F("<form method='POST' action='/api/save'>");
  
  // Network mode (only if Zigbee compiled in)
#if HAS_ZIGBEE
  html += F("<h4 style='margin-top:20px;color:#4caf50'>📡 Network Mode</h4>");
  core::Storage::Mode modeNow = _storage? _storage->getMode(core::Storage::MODE_WIFI_MQTT) : core::Storage::MODE_WIFI_MQTT;
  html += F("<label>Mode</label><div class='row'><select name='mode'>");
  html += F("<option value='wifi'"); if (modeNow==core::Storage::MODE_WIFI_MQTT) html += F(" selected"); html += F(">WiFi/MQTT</option>");
  html += F("<option value='zigbee'"); if (modeNow==core::Storage::MODE_ZIGBEE) html += F(" selected"); 
  #if defined(BOARD_ESP32P4_43)
  html += F(">Zigbee (via C6)</option>");
  #else
  html += F(">Zigbee</option>");
  #endif
  html += F("</select></div>");
#endif
  
  // pH Section
  html += F("<h4 style='margin-top:20px;color:#5ec8ff'>💧 pH Sensor & Pump</h4>");
  html += F("<label>Target pH Min</label><input name='ph_min' value='"); html += _phMin? fmtFloat(*_phMin,2) : String(6.80f); html += F("'>");
  html += F("<label>Target pH Max</label><input name='ph_max' value='"); html += _phMax? fmtFloat(*_phMax,2) : String(7.60f); html += F("'>");
  html += F("<label>pH Pump Speed (%)</label><input name='m1' value='"); html += _m1? String((int)*_m1) : String(60); html += F("'>");
  html += F("<label>pH Pump Flow Rate (ml/min @ 100%)</label><input name='m1_flow' value='"); html += _m1Flow? fmtFloat(*_m1Flow,1) : String(50.0f); html += F("'>");
  html += F("<small style='color:#888'>Calibrate: run pump for 60s @ 100%, measure volume, divide by 1 min</small>");
  
  // ORP Section
  html += F("<h4 style='margin-top:20px;color:#ffb74d'>⚡ ORP Sensor & Pump</h4>");
  html += F("<label>Target ORP Min (mV)</label><input name='orp_min' value='"); html += _orpMin? String(*_orpMin) : String(250); html += F("'>");
  html += F("<label>Target ORP Max (mV)</label><input name='orp_max' value='"); html += _orpMax? String(*_orpMax) : String(850); html += F("'>");
  html += F("<label>ORP Pump Speed (%)</label><input name='m2' value='"); html += _m2? String((int)*_m2) : String(60); html += F("'>");
  html += F("<label>ORP Pump Flow Rate (ml/min @ 100%)</label><input name='m2_flow' value='"); html += _m2Flow? fmtFloat(*_m2Flow,1) : String(50.0f); html += F("'>");
  html += F("<small style='color:#888'>Calibrate: run pump for 60s @ 100%, measure volume, divide by 1 min</small>");
  
  // Motor Control
  html += F("<h4 style='margin-top:20px;color:#7fe3cf'>🔌 Motor Control</h4>");
  {
    bool en = _storage ? _storage->getMotorsEnabled(true) : true;
    html += F("<div style='background:#222;padding:12px;border-radius:8px'><label style='display:flex;align-items:center;gap:8px;margin:0'><input type='checkbox' name='motors_en' value='1'");
    if (en) html += F(" checked");
    html += F("> <strong>Enable pH/ORP dosing pumps</strong></label></div>");
  }
  
  // MQTT settings
  String mh = _storage ? _storage->getMqttHost("") : String("");
  uint16_t mp = _storage ? _storage->getMqttPort(1883) : 1883;
  String mu = _storage ? _storage->getMqttUser("") : String("");
  String mw = _storage ? _storage->getMqttPass("") : String("");
  html += F("<h4 style='margin-top:20px;color:#ff9800'>📡 MQTT Broker</h4>");
  html += F("<label>Host</label><input name='mqtt_host' value='"); html += mh; html += F("'>");
  html += F("<label>Port</label><input name='mqtt_port' value='"); if (mp==0) html += F(""); else html += String((unsigned)mp); html += F("'>");
  html += F("<label>User</label><input name='mqtt_user' value='"); html += mu; html += F("'>");
  html += F("<label>Password</label><input type='password' name='mqtt_pass' value='"); html += mw; html += F("'>");
  html += F("<small style='color:#888'>For Home Assistant integration</small>");
  
  html += F("<div style='margin-top:24px' class='row'><button type='submit'>💾 Save Settings</button></div>");
  html += F("</form></div>");
  html += F("<div style='text-align:center;margin-top:20px'>");
  html += F("<a href='/' style='color:#4caf50'>← Back to Home</a> | ");
  html += F("<a href='/safety' style='color:#ff9800'>⚠️ Safety Settings</a>");
  html += F("</div>");
  sendFooter(html);
  _http.send(200, "text/html; charset=UTF-8", html);
}

void WebUI::handleApiState(){
  String j = "{";
  j += "\"ph\":"; j += domain::Metrics::instance().havePh ? fmtFloat(domain::Metrics::instance().phVal,2) : String("null");
  j += ",\"orp\":"; j += domain::Metrics::instance().haveOrp ? String((int)lrintf(domain::Metrics::instance().orpMv)) : String("null");
  j += ",\"temp\":"; j += domain::Metrics::instance().haveTemp ? fmtFloat(domain::Metrics::instance().tempC,1) : String("null");
  j += "}";
  _http.send(200, "application/json", j);
}

static float parseFloatOr(const String &s, float def){ char *end; float v = strtof(s.c_str(), &end); return (end==s.c_str()) ? def : v; }
static int parseIntOr(const String &s, int def){ char *end; long v = strtol(s.c_str(), &end, 10); return (end==s.c_str()) ? def : (int)v; }

void WebUI::handleApiSave(){
  if (_storage) { _storage->begin(false); }
  if (_http.hasArg("ph_max") && _phMax && _storage) {
    *_phMax = parseFloatOr(_http.arg("ph_max"), *_phMax); _storage->setPhMax(*_phMax);
  }
  if (_http.hasArg("ph_min") && _phMin && _storage) {
    *_phMin = parseFloatOr(_http.arg("ph_min"), *_phMin); _storage->setPhMin(*_phMin);
  }
  if (_http.hasArg("orp_min") && _orpMin && _storage) {
    *_orpMin = parseIntOr(_http.arg("orp_min"), *_orpMin); _storage->setOrpMin(*_orpMin);
  }
  if (_http.hasArg("orp_max") && _orpMax && _storage) {
    *_orpMax = parseIntOr(_http.arg("orp_max"), *_orpMax); _storage->setOrpMax(*_orpMax);
  }
  if (_http.hasArg("m1") && _m1 && _storage) {
    int v = parseIntOr(_http.arg("m1"), *_m1); v = constrain(v,0,100); *_m1 = (uint8_t)v; _storage->setM1Speed(*_m1);
  }
  if (_http.hasArg("m2") && _m2 && _storage) {
    int v = parseIntOr(_http.arg("m2"), *_m2); v = constrain(v,0,100); *_m2 = (uint8_t)v; _storage->setM2Speed(*_m2);
  }
  // Flow rates
  if (_http.hasArg("m1_flow") && _m1Flow && _storage) {
    float v = parseFloatOr(_http.arg("m1_flow"), *_m1Flow); v = constrain(v, 0.1f, 500.0f); *_m1Flow = v; _storage->setM1FlowRate(*_m1Flow);
  }
  if (_http.hasArg("m2_flow") && _m2Flow && _storage) {
    float v = parseFloatOr(_http.arg("m2_flow"), *_m2Flow); v = constrain(v, 0.1f, 500.0f); *_m2Flow = v; _storage->setM2FlowRate(*_m2Flow);
  }
  // Motors enabled checkbox (present only if checked)
  if (_storage) {
    bool en = _http.hasArg("motors_en");
    _storage->setMotorsEnabled(en);
    if (_motorsEnabled) *_motorsEnabled = en;
  }
  #if HAS_ZIGBEE
  if (_http.hasArg("mode") && _storage) {
    String m = _http.arg("mode"); m.toLowerCase();
    int modeInt = (m == "zigbee") ? 1 : 0;
    requestModeChange(modeInt);
  }
  #endif
  // MQTT settings save
  if (_storage) {
    String argHost = _http.arg("mqtt_host");
    String argPort = _http.arg("mqtt_port");
    String argUser = _http.arg("mqtt_user");
    String argPass = _http.arg("mqtt_pass");

    _storage->setMqttHost(argHost);
    
    argPort.trim();
    uint16_t port = argPort.length() ? (uint16_t)parseIntOr(argPort, 1883) : 1883;
    _storage->setMqttPort(port);
    
    _storage->setMqttUser(argUser);
    _storage->setMqttPass(argPass);
    
    requestMqttReload();
  }
  // WhatsApp notification settings
  if (_storage) {
    if (_http.hasArg("wa_phone")) _storage->setWhatsAppPhone(_http.arg("wa_phone"));
    if (_http.hasArg("wa_apikey")) _storage->setCallMeBotApiKey(_http.arg("wa_apikey"));
    bool waEnabled = _http.hasArg("wa_enabled");
    _storage->setWhatsAppEnabled(waEnabled);
  }
  // Redirect back to referer or settings
  String referer = _http.header("Referer");
  String redirect = referer.indexOf("/safety") >= 0 ? "/safety" : "/settings";
  _http.sendHeader("Location", redirect); 
  _http.send(302, "text/plain", "Saved");
}

void WebUI::broadcastMetrics(){
  if (!_active) return;
  String j = "{";
  j += "\"ph\":"; j += domain::Metrics::instance().havePh ? fmtFloat(domain::Metrics::instance().phVal,2) : String("null");
  j += ",\"orp\":"; j += domain::Metrics::instance().haveOrp ? String((int)lrintf(domain::Metrics::instance().orpMv)) : String("null");
  j += ",\"temp\":"; j += domain::Metrics::instance().haveTemp ? fmtFloat(domain::Metrics::instance().tempC,1) : String("null");
  
  // Pump stats (if motor controller available)
  if (_motor) {
    domain::PumpStats m1 = _motor->getM1Stats();
    domain::PumpStats m2 = _motor->getM2Stats();
    bool m1Running = _motor->isM1Running();
    bool m2Running = _motor->isM2Running();
    
    j += ",\"pump_ph\":{";
    j += "\"active\":"; j += m1Running ? "true" : "false";
    j += ",\"session\":"; j += fmtFloat(m1.sessionVolumeMl, 1);
    j += ",\"flow\":"; j += fmtFloat(m1.currentFlowMlMin, 1);
    j += ",\"daily\":"; j += fmtFloat(m1.dailyVolumeMl, 1);
    j += ",\"total\":"; j += fmtFloat(m1.totalVolumeMl, 1);
    j += "}";
    
    j += ",\"pump_orp\":{";
    j += "\"active\":"; j += m2Running ? "true" : "false";
    j += ",\"session\":"; j += fmtFloat(m2.sessionVolumeMl, 1);
    j += ",\"flow\":"; j += fmtFloat(m2.currentFlowMlMin, 1);
    j += ",\"daily\":"; j += fmtFloat(m2.dailyVolumeMl, 1);
    j += ",\"total\":"; j += fmtFloat(m2.totalVolumeMl, 1);
    j += "}";
  }
  
  j += "}";
  _ws.broadcastTXT(j);
}

void WebUI::handleSafety(){
  String html; sendStyleHeader(html);
  html += F("<div class='card'><h3>⚠️ Safety System</h3>");
  
  // Emergency stop status
  bool emergencyStop = _motor ? _motor->isEmergencyStop() : false;
  html += F("<div style='padding:12px;margin-bottom:16px;border-radius:8px;background:");
  html += emergencyStop ? F("rgba(244,67,54,0.2);border:2px solid #f44336'>") : F("rgba(76,175,80,0.2);border:2px solid #4caf50'>");
  html += F("<strong>Status:</strong> ");
  html += emergencyStop ? F("🛑 EMERGENCY STOP ACTIVE") : F("✅ Normal Operation");
  
  if (emergencyStop && _motor) {
    // Show last alert
    domain::SafetyAlert lastAlert = _motor->getLastAlert();
    const char* alertNames[] = {
      "None", "Daily limit M1", "Daily limit M2", 
      "Session volume M1", "Session volume M2",
      "Session duration M1", "Session duration M2",
      "pH sanity low", "pH sanity high",
      "ORP sanity low", "ORP sanity high",
      "pH sensor timeout", "ORP sensor timeout"
    };
    int alertIdx = (int)lastAlert;
    if (alertIdx > 0 && alertIdx < 13) {
      html += F("<br><strong>Last Alert:</strong> ");
      html += alertNames[alertIdx];
    }
  }
  html += F("</div>");
  
  // WhatsApp notifications config
  html += F("<form method='POST' action='/api/save'>");
  String waPhone = _storage ? _storage->getWhatsAppPhone("") : String("");
  String waApiKey = _storage ? _storage->getCallMeBotApiKey("") : String("");
  bool waEnabled = _storage ? _storage->getWhatsAppEnabled(false) : false;

  html += F("<h4 style='margin-top:20px'>📱 WhatsApp Notifications</h4>");
  html += F("<label>Phone Number</label>");
  html += F("<input name='wa_phone' placeholder='+31612345678' value='");
  html += waPhone;
  html += F("'><small>International format (e.g., +31612345678)</small>");
  html += F("<label>CallMeBot API Key</label>");
  html += F("<input name='wa_apikey' placeholder='Your API key' value='");
  html += waApiKey;
  html += F("'><small>Get your key at callmebot.com/blog/free-api-whatsapp-messages</small>");

  html += F("<div style='margin:12px 0'><label style='display:flex;align-items:center;gap:8px'>");
  html += F("<input type='checkbox' name='wa_enabled' value='1'");
  if (waEnabled) html += F(" checked");
  html += F("> Enable WhatsApp notifications</label></div>");

  html += F("<button type='submit'>Save WhatsApp Settings</button>");
  html += F("</form>");
  
  // Pump flow rates info
  html += F("<h4 style='margin-top:20px'>⚙️ Pump Flow Rates</h4>");
  html += F("<div style='font-size:14px;line-height:1.8'>");
  
  float m1Flow = _m1Flow ? *_m1Flow : (_storage ? _storage->getM1FlowRate(50.0f) : 50.0f);
  float m2Flow = _m2Flow ? *_m2Flow : (_storage ? _storage->getM2FlowRate(50.0f) : 50.0f);
  
  html += F("<strong>pH Pump (M1):</strong> "); html += fmtFloat(m1Flow, 1); html += F(" ml/min @ 100%<br>");
  html += F("<strong>ORP Pump (M2):</strong> "); html += fmtFloat(m2Flow, 1); html += F(" ml/min @ 100%<br>");
  html += F("<small style='color:#888'>Configure in <a href='/settings' style='color:#4caf50'>Settings</a></small>");
  html += F("</div>");
  
  // Safety limits info (read-only display for now)
  html += F("<h4 style='margin-top:20px'>🛡️ Safety Limits</h4>");
  html += F("<div style='font-size:14px;line-height:1.8'>");
  
  float maxDaily = _storage ? _storage->getMaxDailyVolume(500.0f) : 500.0f;
  float maxSession = _storage ? _storage->getMaxSessionVolume(50.0f) : 50.0f;
  int maxDuration = _storage ? _storage->getMaxSessionDuration(300) : 300;
  
  html += F("<strong>Max Daily Volume:</strong> "); html += fmtFloat(maxDaily, 0); html += F(" ml/day<br>");
  html += F("<strong>Max Session Volume:</strong> "); html += fmtFloat(maxSession, 0); html += F(" ml<br>");
  html += F("<strong>Max Session Duration:</strong> "); html += String(maxDuration); html += F(" seconds<br>");
  
  float phMin = _storage ? _storage->getPhSanityMin(4.0f) : 4.0f;
  float phMax = _storage ? _storage->getPhSanityMax(10.0f) : 10.0f;
  int orpMin = _storage ? _storage->getOrpSanityMin(-200) : -200;
  int orpMax = _storage ? _storage->getOrpSanityMax(1200) : 1200;
  
  html += F("<strong>pH Sanity Range:</strong> "); html += fmtFloat(phMin, 1); html += F(" - "); html += fmtFloat(phMax, 1); html += F("<br>");
  html += F("<strong>ORP Sanity Range:</strong> "); html += String(orpMin); html += F(" - "); html += String(orpMax); html += F(" mV<br>");
  html += F("</div>");
  
  // Test notification button
  html += F("<h4 style='margin-top:20px'>🧪 Test Notification</h4>");
  html += F("<button onclick=\"fetch('/api/test_alert').then(r=>alert('Test notification sent!'))\">Send Test Alert</button>");
  
  html += F("</div>");
  html += F("<div style='text-align:center;margin-top:20px'>");
  html += F("<a href='/' style='color:#4caf50'>← Back to Home</a>");
  html += F("</div>");
  sendFooter(html);
  _http.send(200, "text/html; charset=UTF-8", html);
}

void WebUI::log(const String &msg) {
  if (!_active) return;
  // Simple JSON log packet
  String safe = msg;
  safe.replace("\"", "\\\""); 
  safe.replace("\n", "\\n");
  safe.replace("\r", "");
  String json = "{\"log\":\"" + safe + "\"}";
  _ws.broadcastTXT(json);
}

void WebUI::handleConsole(){
  String html; sendStyleHeader(html);
  html += F("<div class='card'><h3>🖥️ Web Console</h3>");
  html += F("<div id='logs' style='background:#111;color:#0f0;font-family:monospace;padding:12px;height:400px;overflow-y:scroll;border:1px solid #333;border-radius:4px;font-size:12px;white-space:pre-wrap'></div>");
  html += F("<div class='row' style='margin-top:10px'><button onclick='document.getElementById(\"logs\").innerHTML=\"\"'>Clear</button>");
  html += F("<button onclick='ws.send(\"ping\")' class='btn blue'>Ping</button></div>");
  html += F("</div>");
  html += F("<div style='text-align:center;margin-top:20px'><a href='/' style='color:#4caf50'>← Back to Home</a></div>");
  
  // Script handles receiving logs
  html += F("<script>");
  html += F("var ws=new WebSocket('ws://'+location.host+':81/');");
  html += F("ws.onmessage=function(e){try{var d=JSON.parse(e.data); if(d.log){var l=document.getElementById('logs'); l.innerHTML+=d.log+'\\n'; l.scrollTop=l.scrollHeight;}}catch(_){}};");
  html += F("</script>");
  
  sendFooter(html);
  _http.send(200, "text/html; charset=UTF-8", html);
}

} // namespace io


