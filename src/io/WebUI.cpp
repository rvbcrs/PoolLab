#include "WebUI.h"
#include <WiFi.h>

namespace io {

extern "C" void requestModeChange(int mode);

void WebUI::begin(){
  if (_active) return;
  _http.on("/", [this](){ handleIndex(); });
  _http.on("/settings", [this](){ handleSettings(); });
  _http.on("/api/state", [this](){ handleApiState(); });
  _http.on("/api/save", HTTP_POST, [this](){ handleApiSave(); });
  _http.begin();
  _ws.begin();
  _ws.onEvent([](uint8_t num, WStype_t type, uint8_t * payload, size_t length){ (void)num; (void)type; (void)payload; (void)length; });
  _active = true;
}

void WebUI::loop(){ if (_active) { _http.handleClient(); _ws.loop(); } }
void WebUI::stop(){ if (_active) { _http.stop(); _active=false; } }

static String fmtFloat(float v, int d){ char b[24]; dtostrf(v, 0, d, b); return String(b); }

void WebUI::sendStyleHeader(String &h){
  h  = F("<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>");
  h += F("<style>body{font-family:Arial,Helvetica,sans-serif;background:#111;color:#e3e3e3;margin:0;padding:16px;} .card{background:#1b1b1b;border-radius:10px;padding:16px;max-width:900px;margin:0 auto;box-shadow:0 2px 12px rgba(0,0,0,.4);} h2,h3{margin:0 0 12px 0;} label{display:block;margin:10px 0 6px;} input,button{width:100%;padding:10px;border-radius:8px;border:1px solid #333;background:#222;color:#fff;box-sizing:border-box;} .row{display:flex;gap:8px;} .row>*{flex:1;} .btn{cursor:pointer;border:none;} .btn.red{background:#b00020;} .btn.blue{background:#1976d2;} .muted{color:#aaa;font-size:12px;margin-top:8px;display:block;} .tiles{display:grid;grid-template-columns:repeat(3,1fr);gap:12px;margin:8px 0 14px;} .tile{border-radius:12px;padding:12px;box-shadow:inset 0 1px 0 rgba(255,255,255,.03),0 1px 6px rgba(0,0,0,.35);border:1px solid rgba(255,255,255,.04);} .tile .title{color:#d6d6d6;font-size:13px;margin:6px 0 2px;display:block;letter-spacing:.3px} .tile .val{font-size:28px;font-weight:700;letter-spacing:.3px;margin:2px 0 6px} .icon{display:inline-flex;align-items:center;justify-content:center;width:32px;height:32px;border-radius:50%;margin-bottom:4px;font-size:18px} .tile.blue{background:linear-gradient(180deg,#1d2938,#141a22);} .tile.blue .icon{background:#0c2a3f;color:#5ec8ff;} .tile.orange{background:linear-gradient(180deg,#2e2418,#1c1711);} .tile.orange .icon{background:#3a220c;color:#ffb74d;} .tile.teal{background:linear-gradient(180deg,#19312e,#121e1c);} .tile.teal .icon{background:#0c2f28;color:#7fe3cf;}</style></head><body>");
}
void WebUI::sendFooter(String &h){ h += F("</body></html>"); }

void WebUI::handleIndex(){
  String html; sendStyleHeader(html);
  html += F("<div class='card'><h2>PoolLab</h2><div class='tiles'>");
  // pH tile
  html += F("<div class='tile blue'><div class='icon'>💧</div><span class='title'>pH</span><div class='val' id='phVal'>--.--</div><div class='muted'>Target: <span id='ph_range'>");
  html += (_phMin && _phMax) ? (fmtFloat(*_phMin,2)+String(" - ")+fmtFloat(*_phMax,2)) : String("--");
  html += F("</span></div></div>");
  // ORP tile
  html += F("<div class='tile orange'><div class='icon'>⚡</div><span class='title'>ORP</span><div class='val' id='orpVal'>----</div><div class='muted'>Min/Max: <span id='orp_range'>");
  html += (_orpMin && _orpMax) ? (String(*_orpMin)+String(" / ")+String(*_orpMax)) : String("--");
  html += F("</span></div></div>");
  // Temp tile
  html += F("<div class='tile teal'><div class='icon'>🌡️</div><span class='title'>Temp</span><div class='val' id='tempVal'>--.- °C</div></div>");
  html += F("</div><div class='row'><a href='/settings'><button class='btn blue'>Settings</button></a></div>");
  html += F("<span class='muted'>IP: "); html += WiFi.localIP().toString(); html += F("</span></div>");
  html += F("<script>var ws=new WebSocket('ws://'+location.host+':81/'); ws.onmessage=function(e){try{var d=JSON.parse(e.data); if(d.ph!==undefined) document.getElementById('phVal').innerText=d.ph===null?'--.--':d.ph; if(d.orp!==undefined) document.getElementById('orpVal').innerText=d.orp===null?'----':(d.orp+' mV'); if(d.temp!==undefined) document.getElementById('tempVal').innerText=d.temp===null?'--.- °C':(d.temp+' °C');}catch(_){} };</script>");
  sendFooter(html);
  _http.send(200, "text/html; charset=UTF-8", html);
}

void WebUI::handleSettings(){
  String html; sendStyleHeader(html);
  html += F("<div class='card'><h3>Settings</h3>");
  html += F("<form method='POST' action='/api/save'>");
  // Network mode
  core::Storage::Mode modeNow = _storage? _storage->getMode(core::Storage::MODE_ZIGBEE) : core::Storage::MODE_ZIGBEE;
  html += F("<label>Mode</label><div class='row'><select name='mode'><option value='zigbee'"); if (modeNow==core::Storage::MODE_ZIGBEE) html += F(" selected"); html += F(">Zigbee</option><option value='wifi'"); if (modeNow==core::Storage::MODE_WIFI_MQTT) html += F(" selected"); html += F(">WiFi/MQTT</option></select></div>");
  // Thresholds
  html += F("<label>pH Min</label><input name='ph_min' value='"); html += _phMin? fmtFloat(*_phMin,2) : String(6.80f); html += F("'>");
  html += F("<label>pH Max</label><input name='ph_max' value='"); html += _phMax? fmtFloat(*_phMax,2) : String(7.60f); html += F("'>");
  html += F("<label>ORP Min (mV)</label><input name='orp_min' value='"); html += _orpMin? String(*_orpMin) : String(250); html += F("'>");
  html += F("<label>ORP Max (mV)</label><input name='orp_max' value='"); html += _orpMax? String(*_orpMax) : String(850); html += F("'>");
  html += F("<label>pH Motor %</label><input name='m1' value='"); html += _m1? String((int)*_m1) : String(60); html += F("'>");
  html += F("<label>ORP Motor %</label><input name='m2' value='"); html += _m2? String((int)*_m2) : String(60); html += F("'>");
  html += F("<div class='row'><button class='btn red' type='submit'>Save</button><a href='/'><button class='btn' type='button'>Cancel</button></a></div>");
  html += F("</form></div>");
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
  if (_http.hasArg("mode") && _storage) {
    String m = _http.arg("mode"); m.toLowerCase();
    int modeInt = (m == "zigbee") ? 1 : 0;
    requestModeChange(modeInt);
  }
  _http.sendHeader("Location", "/settings"); _http.send(302, "text/plain", "Saved");
}

void WebUI::broadcastMetrics(){
  if (!_active) return;
  String j = "{";
  j += "\"ph\":"; j += domain::Metrics::instance().havePh ? fmtFloat(domain::Metrics::instance().phVal,2) : String("null");
  j += ",\"orp\":"; j += domain::Metrics::instance().haveOrp ? String((int)lrintf(domain::Metrics::instance().orpMv)) : String("null");
  j += ",\"temp\":"; j += domain::Metrics::instance().haveTemp ? fmtFloat(domain::Metrics::instance().tempC,1) : String("null");
  j += "}";
  _ws.broadcastTXT(j);
}

} // namespace io


