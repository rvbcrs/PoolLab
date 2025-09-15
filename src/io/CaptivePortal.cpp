#include "CaptivePortal.h"

namespace io {

static const byte DNS_PORT = 53;

CaptivePortal::CaptivePortal() {}

void CaptivePortal::beginAP(const String &apSsid){
  if (_active) return;
  // Keep STA available so we can connect after saving creds, but stay in AP if not set
  WiFi.mode(WIFI_AP_STA);
  String ssid = apSsid;
  uint64_t chipid = ESP.getEfuseMac();
  char suffix[16];
  snprintf(suffix, sizeof(suffix), "-%06llX", (unsigned long long)(chipid & 0xFFFFFFULL));
  ssid += suffix;
  WiFi.softAP(ssid.c_str());
  delay(100);
  _dns.start(DNS_PORT, "*", WiFi.softAPIP());
  setupRoutes();
  _http.begin();
  _active = true;
}

void CaptivePortal::setupRoutes(){
  auto indexHandler = [this](){
    String ssid = _storage ? _storage->getWifiSsid("") : "";
    String page;
    page  = F("<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>");
    page += F("<style>body{font-family:Arial,Helvetica,sans-serif;background:#111;color:#e3e3e3;margin:0;padding:16px;} .card{background:#1b1b1b;border-radius:10px;padding:16px;max-width:420px;margin:0 auto;box-shadow:0 2px 12px rgba(0,0,0,.4);} h3{margin:0 0 12px 0;} label{display:block;margin:10px 0 6px;} input,select,button{width:100%;padding:10px;border-radius:8px;border:1px solid #333;background:#222;color:#fff;box-sizing:border-box;} .row{display:flex;gap:8px;} .row>*{flex:1;} .btn{cursor:pointer;border:none;} .btn.red{background:#b00020;} .btn.blue{background:#1976d2;} .muted{color:#aaa;font-size:12px;margin-top:8px;display:block;} .right{float:right;}</style></head><body>");
    page += F("<div class='card'><h3>WiFi setup <span class='muted right' id='ip'></span></h3>");
    page += F("<label>SSID</label><div class='row'><select id='ssidSel'></select><button class='btn blue' onclick='scan()'>Scan</button></div>");
    page += F("<label>Custom SSID</label><input id='ssid' placeholder='Of kies uit de lijst' value='");
    page += ssid;
    page += F("'>");
    page += F("<label>Password</label><input id='pass' type='password' placeholder='WPA password'>");
    page += F("<div class='row'><button class='btn red' onclick='save()'>Save & Reboot</button><button class='btn' onclick='location.reload()'>Cancel</button></div>");
    page += F("<span class='muted'>Tip: kies SSID uit de lijst of vul handmatig.</span></div>");
    page += F("<script>function q(n){return document.getElementById(n)};function fill(list){let s=q('ssidSel');s.innerHTML='';list.forEach(o=>{let opt=document.createElement('option');opt.text=o.ssid+' ('+o.rssi+'dBm'+(o.sec?' 🔒':'')+')';opt.value=o.ssid;s.add(opt);});let typed=q('ssid').value;let idx=0;if(typed){for(let i=0;i<list.length;i++){if(list[i].ssid===typed){idx=i;break;}}}s.selectedIndex=idx;if(!typed && list.length>0){q('ssid').value=list[0].ssid;}}");
    page += F("function scan(){fetch('/scan').then(r=>r.json()).then(fill);} scan(); q('ssidSel').addEventListener('change',e=>{q('ssid').value=e.target.value}); function save(){let sVal=q('ssid').value||q('ssidSel').value; let p=q('pass').value; fetch('/save',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'s='+encodeURIComponent(sVal)+'&p='+encodeURIComponent(p)}).then(_=>{document.body.innerHTML='<div class=card><h3>Saving...</h3><p>Device rebooting.</p></div>';});}");
    page += F("</script></body></html>");
    _http.send(200, "text/html", page);
  };
  _http.on("/", HTTP_GET, indexHandler);
  _http.on("/generate_204", HTTP_GET, indexHandler); // Android captive portal
  _http.on("/hotspot-detect.html", HTTP_GET, indexHandler); // iOS captive portal
  _http.on("/scan", HTTP_GET, [this](){
    int n = WiFi.scanNetworks();
    String j = "[";
    for (int i=0;i<n;i++){
      if (i) j += ",";
      j += "{\"ssid\":\"" + String(WiFi.SSID(i)) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + ",\"sec\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? 1:0) + "}";
    }
    j += "]";
    _http.send(200, "application/json", j);
  });
  _http.on("/save", HTTP_POST, [this](){
    if (_storage) {
      if (_http.hasArg("s")) _storage->setWifiSsid(_http.arg("s"));
      if (_http.hasArg("p")) _storage->setWifiPass(_http.arg("p"));
    }
    _http.send(200, "text/html", F("<html>Saved. Rebooting...</html>"));
    delay(500);
    ESP.restart();
  });
  _http.onNotFound(indexHandler);
}

void CaptivePortal::loop(){
  if (!_active) return;
  _dns.processNextRequest();
  _http.handleClient();
}

void CaptivePortal::stop(){
  if (!_active) return;
  _http.stop();
  _dns.stop();
  WiFi.softAPdisconnect(true);
  _active = false;
}

} // namespace io


