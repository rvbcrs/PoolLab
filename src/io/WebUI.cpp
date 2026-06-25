#include "WebUI.h"
#include "MotorController.h"
#include <WiFi.h>
#include <SPIFFS.h>
#include <FS.h>

namespace io {

extern "C" void requestModeChange(int mode);
extern "C" void requestMqttReload();
extern "C" void requestOrpCalReload();

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
  _ws.enableHeartbeat(15000, 3000, 2); // ping clients; prune dead slots so broadcasts don't stall on stale sockets
  _ws.onEvent([](uint8_t num, WStype_t type, uint8_t * payload, size_t length){ (void)num; (void)type; (void)payload; (void)length; });
  _active = true;
}

void WebUI::loop(){ if (_active) { _http.handleClient(); _ws.loop(); } }
void WebUI::stop(){ if (_active) { _http.stop(); _active=false; } }

static String fmtFloat(float v, int d){ char b[24]; dtostrf(v, 0, d, b); return String(b); }

void WebUI::sendStyleHeader(String &h){
  h  = F("<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>");
  h += F("<script>(function(){try{var t=localStorage.getItem('pl-theme')||'dark';document.documentElement.setAttribute('data-theme',t);}catch(_){}})();</script>");
  h += F("<style>"
    /* dark (default) */
    ":root{--bg:#0a0418;--ink:#fff;--mute:#9a85b8;--line:rgba(255,95,166,.18);--linesoft:rgba(255,95,166,.08);--card:linear-gradient(160deg,rgba(255,255,255,.06),rgba(255,255,255,.01));--cardborder:rgba(255,95,166,.25);--ph:#ff5fa6;--orp:#5fe0ff;--temp:#c87fff;--bad:#ff3960;--glow:1;--bgFx:radial-gradient(ellipse 70% 50% at 20% 10%,rgba(255,95,166,.22),transparent 60%),radial-gradient(ellipse 70% 50% at 80% 20%,rgba(95,224,255,.16),transparent 60%),radial-gradient(ellipse 60% 40% at 50% 100%,rgba(200,127,255,.14),transparent 70%),linear-gradient(180deg,#0a0418 0%,#15082a 100%);--scan:repeating-linear-gradient(0deg,transparent 0 3px,rgba(255,255,255,.025) 3px 4px)}"
    /* light */
    "[data-theme=light]{--bg:#fdf9f3;--ink:#1a0e2a;--mute:#7a6a90;--line:rgba(26,14,42,.1);--linesoft:rgba(26,14,42,.05);--card:rgba(255,255,255,.7);--cardborder:rgba(26,14,42,.08);--ph:#e6377c;--orp:#1f9ed4;--temp:#9a4ed4;--bad:#d62a4a;--glow:0;--bgFx:radial-gradient(ellipse 60% 50% at 15% 5%,rgba(230,55,124,.14),transparent 60%),radial-gradient(ellipse 70% 50% at 85% 20%,rgba(31,158,212,.1),transparent 60%),radial-gradient(ellipse 60% 40% at 50% 100%,rgba(154,78,212,.08),transparent 70%);--scan:none}"
    "*{box-sizing:border-box}"
    "body{margin:0;background:var(--bg);color:var(--ink);font:14px/1.5 'SF Pro Display',-apple-system,Inter,system-ui,sans-serif;min-height:100vh;overflow-x:hidden;font-weight:400}"
    "body::before{content:'';position:fixed;inset:0;z-index:0;pointer-events:none;background:var(--bgFx)}"
    "body::after{content:'';position:fixed;inset:0;z-index:0;pointer-events:none;background:var(--scan);opacity:.5}"
    ".shell{max-width:920px;margin:0 auto;padding:32px 28px 60px;position:relative;z-index:1}"
    /* alert banner */
    ".alerts{display:none;flex-direction:column;gap:8px;margin-bottom:18px}"
    ".alerts.show{display:flex}"
    ".alert{display:flex;align-items:flex-start;gap:14px;padding:14px 18px;border:1px solid var(--bad);border-left:4px solid var(--bad);border-radius:6px;background:var(--card);backdrop-filter:blur(18px);font-size:13px;color:var(--ink)}"
    "[data-theme=dark] .alert{box-shadow:0 0 24px rgba(255,57,96,.15)}"
    ".alert.warn{border-color:var(--orp);border-left-color:var(--orp)}"
    "[data-theme=dark] .alert.warn{box-shadow:0 0 24px rgba(95,224,255,.12)}"
    ".alert .ic{font-family:'SF Mono',Menlo,monospace;font-size:11px;letter-spacing:.3em;text-transform:uppercase;color:var(--bad);font-weight:700;flex:0 0 60px;padding-top:1px}"
    "[data-theme=dark] .alert .ic{text-shadow:0 0 6px var(--bad)}"
    ".alert.warn .ic{color:var(--orp)}"
    "[data-theme=dark] .alert.warn .ic{text-shadow:0 0 6px var(--orp)}"
    ".alert .body{flex:1;line-height:1.5}"
    ".alert .body b{color:var(--ink);font-weight:600;font-family:'SF Mono',Menlo,monospace;font-size:12px}"
    ".alert .body em{font-style:italic;color:var(--mute);display:block;margin-top:3px;font-size:12px}"
    /* topbar */
    ".topbar{display:flex;justify-content:space-between;align-items:center;padding:13px 22px;border:1px solid var(--line);border-radius:99px;background:var(--card);backdrop-filter:blur(18px);gap:14px;flex-wrap:wrap}"
    ".topbar .left{display:flex;align-items:center;gap:14px;font-family:'SF Mono',Menlo,monospace;font-size:11px;letter-spacing:.32em;text-transform:uppercase}"
    ".topbar .logo{color:var(--ph);font-weight:700;letter-spacing:.3em}"
    ".topbar .sep{color:var(--mute);opacity:.4}"
    ".topbar .meta{color:var(--mute)}"
    ".topbar .right{display:flex;align-items:center;gap:14px}"
    ".topbar .live{color:var(--orp);font-family:'SF Mono',Menlo,monospace;font-size:11px;letter-spacing:.32em;text-transform:uppercase;font-weight:600}"
    "[data-theme=dark] .topbar .live,[data-theme=dark] .topbar .logo{text-shadow:0 0 8px currentColor}"
    ".topbar .live::before{content:'';display:inline-block;width:7px;height:7px;border-radius:50%;background:var(--orp);margin-right:8px;animation:p 1.4s ease-in-out infinite;vertical-align:1px}"
    "[data-theme=dark] .topbar .live::before{box-shadow:0 0 10px var(--orp)}"
    "@keyframes p{50%{opacity:.3}}"
    /* theme toggle */
    ".themesw{background:transparent;border:1px solid var(--line);color:var(--mute);font-family:'SF Mono',Menlo,monospace;font-size:11px;letter-spacing:.2em;text-transform:uppercase;cursor:pointer;padding:6px 12px;border-radius:99px;transition:all .15s;width:auto}"
    ".themesw:hover{color:var(--ink);border-color:var(--ink)}"
    /* heading */
    ".heading{margin:36px 0 32px}"
    ".heading h1{margin:0;font-weight:300;font-size:32px;letter-spacing:-.02em;color:var(--ink)}"
    ".heading h1 em{font-style:italic;font-weight:300;background:linear-gradient(135deg,var(--ph),var(--orp));-webkit-background-clip:text;background-clip:text;color:transparent}"
    "[data-theme=dark] .heading h1 em{filter:drop-shadow(0 0 12px rgba(255,95,166,.25))}"
    ".heading .desc{color:var(--mute);font-size:13px;letter-spacing:.06em;margin-top:8px}"
    /* metrics */
    ".metrics{display:grid;grid-template-columns:repeat(3,1fr);gap:16px;margin-bottom:22px}"
    ".metric{position:relative;background:var(--card);border:1px solid var(--cardborder);border-radius:6px;padding:22px;backdrop-filter:blur(18px);overflow:hidden}"
    "[data-theme=dark] .metric{box-shadow:0 0 0 1px rgba(255,255,255,.03) inset,0 8px 32px rgba(0,0,0,.4),0 0 40px var(--clr,#ff5fa6)15}"
    "[data-theme=light] .metric{box-shadow:0 8px 32px var(--clr,#e6377c)10,0 0 0 1px rgba(255,255,255,.6) inset}"
    "[data-theme=dark] .metric::before{content:'';position:absolute;inset:0;background:linear-gradient(135deg,transparent 40%,var(--clr,#ff5fa6)08 50%,transparent 60%);pointer-events:none}"
    ".metric::after{content:'';position:absolute;top:0;left:0;right:0;height:1px;background:linear-gradient(90deg,transparent,var(--clr,#ff5fa6),transparent)}"
    "[data-theme=dark] .metric::after{box-shadow:0 0 12px var(--clr,#ff5fa6);height:1px}"
    "[data-theme=light] .metric::after{height:2px}"
    ".metric.ph{--clr:var(--ph)}.metric.orp{--clr:var(--orp)}.metric.temp{--clr:var(--temp)}"
    ".label{display:flex;justify-content:space-between;align-items:center;font-size:10px;letter-spacing:.4em;text-transform:uppercase;color:var(--mute);font-family:'SF Mono',Menlo,monospace;margin-bottom:18px}"
    ".label .id{padding:2px 10px;border:1px solid var(--clr,var(--ph));border-radius:99px;color:var(--clr,var(--ph));font-weight:600;font-size:9px}"
    "[data-theme=dark] .label .id{text-shadow:0 0 6px var(--clr,var(--ph))}"
    ".val{display:flex;align-items:baseline;gap:8px;font-family:'SF Mono',Menlo,monospace;font-size:54px;font-weight:600;letter-spacing:-.04em;font-variant-numeric:tabular-nums;color:var(--clr,var(--ph));font-style:italic;margin:0}"
    "[data-theme=dark] .val{text-shadow:0 0 18px var(--clr,var(--ph)),0 0 50px color-mix(in srgb,var(--clr,var(--ph)) 35%,transparent),0 4px 0 rgba(0,0,0,.4)}"
    "[data-theme=light] .val{text-shadow:0 2px 24px color-mix(in srgb,var(--clr,var(--ph)) 25%,transparent)}"
    ".val.bad{color:var(--bad)}"
    ".val .unit{color:var(--mute);font-size:14px;letter-spacing:.1em;font-family:'SF Mono',Menlo,monospace;text-transform:uppercase;font-style:normal;font-weight:400;text-shadow:none}"
    ".range{margin-top:16px;display:flex;justify-content:space-between;font-size:10px;letter-spacing:.25em;text-transform:uppercase;color:var(--mute);font-family:'SF Mono',Menlo,monospace}"
    ".range b{color:var(--clr,var(--ph));font-weight:600}"
    "[data-theme=dark] .range b{text-shadow:0 0 6px var(--clr,var(--ph))}"
    /* dosers */
    ".dosers{margin-top:8px;background:var(--card);border:1px solid var(--cardborder);border-radius:6px;backdrop-filter:blur(18px);overflow:hidden;position:relative}"
    "[data-theme=dark] .dosers{box-shadow:0 0 0 1px rgba(255,255,255,.03) inset}"
    ".dosers::before{content:'';position:absolute;top:0;left:0;right:0;height:1px;background:linear-gradient(90deg,transparent,var(--ph),var(--orp),transparent)}"
    "[data-theme=dark] .dosers::before{box-shadow:0 0 12px var(--ph)}"
    ".doser{display:grid;grid-template-columns:160px 1fr auto;align-items:center;gap:18px;padding:18px 26px;border-bottom:1px solid var(--linesoft)}"
    ".doser:last-child{border-bottom:none}"
    ".doser .n{display:flex;align-items:center;gap:12px;font-size:12px;letter-spacing:.18em;text-transform:uppercase;color:var(--ink);font-family:'SF Mono',Menlo,monospace;font-weight:600}"
    ".doser .dot{width:10px;height:10px;border-radius:50%;background:var(--linesoft)}"
    ".doser .dot.on{background:var(--ph)}"
    "[data-theme=dark] .doser .dot.on{box-shadow:0 0 16px var(--ph),0 0 32px var(--ph)}"
    "[data-theme=light] .doser .dot.on{box-shadow:0 0 14px var(--ph)}"
    ".doser .st{font-size:12px;color:var(--mute);text-transform:uppercase;letter-spacing:.18em;font-style:italic}"
    ".doser .nums{font-family:'SF Mono',Menlo,monospace;font-size:11px;color:var(--mute);text-align:right;letter-spacing:.15em;text-transform:uppercase}"
    ".doser .nums b{color:var(--ph);font-weight:600}"
    "[data-theme=dark] .doser .nums b{text-shadow:0 0 6px var(--ph)}"
    ".doser:nth-child(2) .nums b{color:var(--orp)}"
    "[data-theme=dark] .doser:nth-child(2) .nums b{text-shadow:0 0 6px var(--orp)}"
    /* actions */
    ".actions{margin-top:24px;display:grid;grid-template-columns:1fr 1fr 64px;gap:12px}"
    ".btn{padding:14px 18px;background:transparent;border:1px solid var(--ph);border-radius:6px;text-decoration:none;color:var(--ph);text-align:center;font-size:10px;letter-spacing:.4em;text-transform:uppercase;font-weight:700;font-family:'SF Mono',Menlo,monospace;transition:all .2s;cursor:pointer;display:inline-flex;align-items:center;justify-content:center}"
    "[data-theme=dark] .btn{background:linear-gradient(180deg,rgba(255,95,166,.08),rgba(255,95,166,.02));border-color:rgba(255,95,166,.5);text-shadow:0 0 6px var(--ph)}"
    "[data-theme=dark] .btn:hover{box-shadow:0 0 28px var(--ph),0 0 0 1px var(--ph) inset;background:linear-gradient(180deg,rgba(255,95,166,.15),rgba(255,95,166,.05))}"
    "[data-theme=light] .btn{background:rgba(255,255,255,.5);border-color:rgba(230,55,124,.4);backdrop-filter:blur(20px)}"
    "[data-theme=light] .btn:hover{background:var(--ph);color:#fff;box-shadow:0 8px 24px rgba(230,55,124,.4)}"
    ".btn.warn{color:var(--bad);border-color:var(--bad)}"
    "[data-theme=dark] .btn.warn{border-color:rgba(255,57,96,.4);text-shadow:0 0 6px var(--bad)}"
    "[data-theme=dark] .btn.warn:hover{box-shadow:0 0 28px var(--bad)}"
    "[data-theme=light] .btn.warn{border-color:rgba(214,42,74,.4)}"
    "[data-theme=light] .btn.warn:hover{background:var(--bad);color:#fff;box-shadow:0 8px 24px rgba(214,42,74,.4)}"
    ".btn.icon{color:var(--orp);border-color:var(--orp)}"
    "[data-theme=dark] .btn.icon{border-color:rgba(95,224,255,.4);text-shadow:0 0 6px var(--orp)}"
    "[data-theme=dark] .btn.icon:hover{box-shadow:0 0 28px var(--orp)}"
    "[data-theme=light] .btn.icon{border-color:rgba(31,158,212,.4)}"
    "[data-theme=light] .btn.icon:hover{background:var(--orp);color:#fff;box-shadow:0 8px 24px rgba(31,158,212,.4)}"
    /* foot */
    ".foot{margin-top:28px;text-align:center;font-size:10px;letter-spacing:.4em;text-transform:uppercase;color:var(--mute);font-family:'SF Mono',Menlo,monospace}"
    ".foot b{color:var(--orp);font-weight:600}"
    "[data-theme=dark] .foot b{text-shadow:0 0 6px var(--orp)}"
    /* settings/form (shared style for /settings, /safety pages) */
    ".card{background:var(--card);border:1px solid var(--cardborder);padding:30px;max-width:760px;margin:28px auto;border-radius:8px;backdrop-filter:blur(18px);position:relative;z-index:1}"
    "h2,h3{margin:0 0 16px 0;font-family:inherit;font-weight:300;font-style:italic;letter-spacing:-.01em}"
    "h2{font-size:28px}h3{font-size:22px}"
    "h4{font-size:11px;text-transform:uppercase;font-style:normal;letter-spacing:.4em;color:var(--ph);font-family:'SF Mono',Menlo,monospace;margin:28px 0 12px;font-weight:700}"
    "[data-theme=dark] h4{text-shadow:0 0 6px var(--ph)}"
    "label{display:block;margin:14px 0 5px;font-size:10px;letter-spacing:.3em;color:var(--mute);text-transform:uppercase;font-family:'SF Mono',Menlo,monospace;font-weight:600}"
    "input,select{width:100%;padding:11px 14px;background:var(--bg);color:var(--ink);border:1px solid var(--line);border-radius:4px;font-family:'SF Mono',Menlo,monospace;font-size:13px;font-variant-numeric:tabular-nums;outline:none;transition:all .15s}"
    "input:focus,select:focus{border-color:var(--ph)}"
    "[data-theme=dark] input:focus,[data-theme=dark] select:focus{box-shadow:0 0 12px rgba(255,95,166,.3)}"
    "button[type=submit],button:not(.themesw){width:100%;padding:14px;background:var(--ph);color:#fff;border:none;cursor:pointer;font-size:11px;letter-spacing:.4em;text-transform:uppercase;font-family:'SF Mono',Menlo,monospace;font-weight:700;transition:all .15s;border-radius:4px}"
    "[data-theme=dark] button[type=submit]{box-shadow:0 0 20px rgba(255,95,166,.4)}"
    "button[type=submit]:hover{filter:brightness(1.1);box-shadow:0 0 28px rgba(255,95,166,.6)}"
    ".row{display:flex;gap:8px}.row>*{flex:1}"
    ".muted{color:var(--mute);font-size:11px;font-family:'SF Mono',Menlo,monospace;display:block;margin-top:6px;letter-spacing:.08em}"
    "small{color:var(--mute);font-family:'SF Mono',Menlo,monospace;font-size:10px;letter-spacing:.08em}"
    "a{color:var(--ph);text-decoration:none}"
    /* console modal */
    ".modal-bd{position:fixed;inset:0;background:rgba(10,4,24,.6);backdrop-filter:blur(8px);z-index:100;display:none;align-items:center;justify-content:center;padding:24px}"
    ".modal-bd.open{display:flex}"
    ".modal{width:100%;max-width:820px;height:560px;max-height:85vh;background:var(--card);border:1px solid var(--cardborder);border-radius:8px;backdrop-filter:blur(24px);overflow:hidden;display:flex;flex-direction:column;box-shadow:0 24px 60px rgba(0,0,0,.5)}"
    "[data-theme=dark] .modal{box-shadow:0 0 60px rgba(255,95,166,.25),0 24px 60px rgba(0,0,0,.5)}"
    ".modal-head{flex:0 0 auto;display:flex;justify-content:space-between;align-items:center;padding:14px 20px;border-bottom:1px solid var(--linesoft);font-family:'SF Mono',Menlo,monospace;font-size:11px;letter-spacing:.32em;text-transform:uppercase;color:var(--mute)}"
    ".modal-head .ttl{color:var(--ph);font-weight:600}"
    "[data-theme=dark] .modal-head .ttl{text-shadow:0 0 8px var(--ph)}"
    ".modal-head .x{background:none;border:none;color:var(--mute);font-size:18px;cursor:pointer;padding:4px 10px;line-height:1;width:auto;transition:color .15s;font-weight:400}"
    ".modal-head .x:hover{color:var(--bad)}"
    /* terminal-style body: always black, always scrollable */
    ".modal-body{flex:1 1 0;min-height:0;overflow-y:auto;padding:16px 20px;font-family:'SF Mono',Menlo,monospace;font-size:11px;line-height:1.55;background:#05060a;color:#9fe;text-shadow:0 0 4px rgba(95,224,255,.4);white-space:pre-wrap;word-break:break-all}"
    ".modal-body .empty{color:#5a6470;font-style:italic;letter-spacing:.04em;text-shadow:none}"
    ".modal-foot{flex:0 0 auto;padding:12px 20px;border-top:1px solid var(--linesoft);display:flex;gap:10px;justify-content:flex-end}"
    ".modal-foot button{width:auto;padding:8px 16px;font-size:10px;letter-spacing:.3em;background:transparent;color:var(--mute);border:1px solid var(--line);border-radius:4px}"
    ".modal-foot button:hover{color:var(--ink);border-color:var(--ink);background:transparent}"
    "</style></head><body>");
}
void WebUI::sendFooter(String &h){ h += F("</body></html>"); }

void WebUI::handleIndex(){
  String html; sendStyleHeader(html);
  String phRangeMin = _phMin ? fmtFloat(*_phMin,2) : String("--");
  String phRangeMax = _phMax ? fmtFloat(*_phMax,2) : String("--");
  String orpRangeMin = _orpMin ? String(*_orpMin) : String("--");
  String orpRangeMax = _orpMax ? String(*_orpMax) : String("--");

  html += F("<div class='shell'>");
  html += F("<div class='topbar'>"
            "<div class='left'>"
              "<span class='logo'>◆ Pura</span>"
              "<span class='sep'>/</span><span class='meta'>node ");
  html += WiFi.localIP().toString();
  html += F("</span>"
            "</div>"
            "<div class='right'>"
              "<button class='themesw' onclick='tgTheme()' id='themesw'>◐ light</button>"
              "<span class='live'>live · 2hz</span>"
            "</div>"
          "</div>");

  html += F("<div class='heading'>"
            "<h1>Pura <em>readings</em>.</h1>"
            "<div class='desc'>Real-time telemetry from the pool sensors.</div>"
          "</div>");

  html += F("<div class='alerts' id='alerts'></div>");

  html += F("<div class='metrics'>"
            "<div class='metric ph'>"
              "<div class='label'><span>pH</span><span class='id'>A01</span></div>"
              "<div class='val' id='phVal'>--.--</div>"
              "<div class='range'><span>tgt min <b>"); html += phRangeMin;
  html += F("</b></span><span>max <b>"); html += phRangeMax; html += F("</b></span></div>"
            "</div>"
            "<div class='metric orp'>"
              "<div class='label'><span>ORP</span><span class='id'>B01</span></div>"
              "<div class='val' id='orpVal'>—<span class='unit'>mV</span></div>"
              "<div class='range'><span>win min <b>"); html += orpRangeMin;
  html += F("</b></span><span>max <b>"); html += orpRangeMax; html += F("</b></span></div>"
            "</div>"
            "<div class='metric temp'>"
              "<div class='label'><span>Temp</span><span class='id'>NTC</span></div>"
              "<div class='val' id='tempVal'>—<span class='unit'>°C</span></div>"
              "<div class='range'><span><b>stable</b></span><span>ambient</span></div>"
            "</div>"
          "</div>");

  html += F("<div class='dosers'>"
            "<div class='doser'><div class='n'><span class='dot' id='dot_ph'></span>pH Dose</div>"
              "<div class='st'><em>idle</em></div>"
              "<div class='nums' id='ph_stats'>today <b>0.0</b>mL · total <b>0.0</b>mL</div></div>"
            "<div class='doser'><div class='n'><span class='dot' id='dot_orp'></span>ORP Dose</div>"
              "<div class='st'><em>idle</em></div>"
              "<div class='nums' id='orp_stats'>today <b>0.0</b>mL · total <b>0.0</b>mL</div></div>"
          "</div>");

  html += F("<div class='actions'>"
            "<a class='btn' href='/settings'>Settings</a>"
            "<a class='btn warn' href='/safety'>Safety</a>"
            "<button class='btn icon' onclick='openCon()' title='Console'>&gt;_</button>"
          "</div>");

  html += F("<div class='modal-bd' id='conmd' onclick='if(event.target==this)closeCon()'>"
            "<div class='modal'>"
              "<div class='modal-head'>"
                "<span class='ttl'>&gt; Web Console</span>"
                "<button class='x' onclick='closeCon()' title='Close'>×</button>"
              "</div>"
              "<div class='modal-body' id='logs'><span class='empty'>waiting for log output…</span></div>"
              "<div class='modal-foot'>"
                "<button onclick='clearLogs()'>Clear</button>"
                "<button onclick='closeCon()'>Close</button>"
              "</div>"
            "</div>"
          "</div>");

  html += F("<div class='foot'>node · <b>"); html += WiFi.localIP().toString(); html += F("</b> · v0.7</div>");
  html += F("</div>");

  float phMin = _phMin? *_phMin : 6.80f; float phMax = _phMax? *_phMax : 7.60f;
  int orpMin = _orpMin? *_orpMin : 250; int orpMax = _orpMax? *_orpMax : 850;
  html += F("<script>");
  html += "var PH_MIN="+String(phMin,2)+",PH_MAX="+String(phMax,2)+",ORP_MIN="+String(orpMin)+",ORP_MAX="+String(orpMax)+";";
  html += F(
    "function syncThemeBtn(){var t=document.documentElement.getAttribute('data-theme')||'dark';"
      "document.getElementById('themesw').innerHTML=(t==='dark'?'◐ light':'◑ dark');}"
    "function tgTheme(){var t=document.documentElement.getAttribute('data-theme')||'dark';"
      "var nt=(t==='dark'?'light':'dark');document.documentElement.setAttribute('data-theme',nt);"
      "try{localStorage.setItem('pl-theme',nt);}catch(_){}syncThemeBtn();}"
    "syncThemeBtn();"
    "function openCon(){document.getElementById('conmd').classList.add('open');}"
    "function closeCon(){document.getElementById('conmd').classList.remove('open');}"
    "function clearLogs(){var l=document.getElementById('logs');l.innerHTML='<span class=\"empty\">cleared</span>';}"
    "var _logEmpty=true;"
    "function addLog(line){var l=document.getElementById('logs');if(_logEmpty){l.innerHTML='';_logEmpty=false;}"
      "var ln=document.createElement('div');ln.textContent=line;l.appendChild(ln);"
      "while(l.childNodes.length>500)l.removeChild(l.firstChild);"
      "l.scrollTop=l.scrollHeight;}"
    "document.addEventListener('keydown',function(e){if(e.key==='Escape')closeCon();});"
    "function cls(el,c){el.classList.remove('bad');el.classList.remove('warn');if(c)el.classList.add(c);}"
    "function nearPh(v){return v<=PH_MIN+0.05||v>=PH_MAX-0.05;}"
    "function nearOrp(v){return v<=ORP_MIN+20||v>=ORP_MAX-20;}"
    "var _curPh=null,_curOrp=null;"
    "function renderAlerts(){var a=document.getElementById('alerts'),items=[];"
      "if(_curPh!==null){"
        "if(_curPh<PH_MIN)items.push({k:'bad',ic:'pH ↓',t:'<b>pH te laag ('+_curPh.toFixed(2)+').</b> Water is te zuur — er is geen base-dosering beschikbaar, voeg handmatig pH-plus toe om weer binnen '+PH_MIN.toFixed(2)+'–'+PH_MAX.toFixed(2)+' te komen.<em>Een lage pH veroorzaakt corrosie en irritatie van de huid/ogen.</em>'});"
        "else if(_curPh>PH_MAX)items.push({k:'bad',ic:'pH ↑',t:'<b>pH te hoog ('+_curPh.toFixed(2)+').</b> Water is te basisch — pH-minus wordt automatisch gedoseerd om het terug binnen '+PH_MIN.toFixed(2)+'–'+PH_MAX.toFixed(2)+' te brengen.<em>Hoge pH vermindert de werking van chloor.</em>'});"
      "}"
      "if(_curOrp!==null){"
        "if(_curOrp<ORP_MIN)items.push({k:'bad',ic:'ORP ↓',t:'<b>ORP te laag ('+_curOrp+' mV).</b> Te weinig chloor in het zwembad — chloor wordt automatisch gedoseerd om boven '+ORP_MIN+' mV te komen.<em>Lage ORP betekent onvoldoende desinfectie.</em>'});"
        "else if(_curOrp>ORP_MAX)items.push({k:'warn',ic:'ORP ↑',t:'<b>ORP te hoog ('+_curOrp+' mV).</b> Te veel chloor in het zwembad — geen automatische dosering (anders wordt het erger).<em>Wacht tot het chloorgehalte natuurlijk daalt of dun het water aan.</em>'});"
      "}"
      "if(items.length===0){a.classList.remove('show');a.innerHTML='';return;}"
      "a.innerHTML=items.map(function(i){return '<div class=\"alert '+i.k+'\"><div class=\"ic\">'+i.ic+'</div><div class=\"body\">'+i.t+'</div></div>';}).join('');"
      "a.classList.add('show');}"
    "function fmtVol(ml){return ml>=1000?(ml/1000).toFixed(2)+'L':ml.toFixed(1)+'mL';}"
    "function pumpHtml(p){"
      "var prefix=p.active?'● '+p.session.toFixed(1)+'mL @ '+p.flow.toFixed(1)+'mL/min · ':'';"
      "return prefix+'today <b>'+fmtVol(p.daily)+'</b> · total <b>'+fmtVol(p.total)+'</b>';}"
    "function setStateHtml(rowDot,active){var d=document.getElementById(rowDot);if(d)d.classList.toggle('on',!!active);"
      "var st=d&&d.parentNode&&d.parentNode.parentNode&&d.parentNode.parentNode.querySelector('.st');"
      "if(st)st.innerHTML=active?'<em>active</em>':'<em>idle</em>';}"
    "var ws;function wsConnect(){ws=new WebSocket('ws://'+location.host+':81/');"
    "ws.onclose=function(){setTimeout(wsConnect,2000);};"  // ponytail: bare reconnect, no backoff needed on a LAN
    "ws.onmessage=function(e){try{var d=JSON.parse(e.data);"
      "if(d.log!==undefined){addLog(d.log);}"
      "if(d.ph!==undefined){var el=document.getElementById('phVal');"
        "if(d.ph===null){el.innerText='--.--';cls(el,null);_curPh=null;}"
        "else{el.innerText=d.ph;var v=parseFloat(d.ph);_curPh=isFinite(v)?v:null;if(isFinite(v)){"
          "if(v<PH_MIN||v>PH_MAX)cls(el,'bad');else if(nearPh(v))cls(el,'warn');else cls(el,null);}}renderAlerts();}"
      "if(d.orp!==undefined){var el2=document.getElementById('orpVal');"
        "if(d.orp===null){el2.innerHTML='—<span class=\"unit\">mV</span>';cls(el2,null);_curOrp=null;}"
        "else{el2.innerHTML=d.orp+'<span class=\"unit\">mV</span>';var v2=parseInt(d.orp);_curOrp=isFinite(v2)?v2:null;if(isFinite(v2)){"
          "if(v2<ORP_MIN||v2>ORP_MAX)cls(el2,'bad');else if(nearOrp(v2))cls(el2,'warn');else cls(el2,null);}}renderAlerts();}"
      "if(d.temp!==undefined){var et=document.getElementById('tempVal');"
        "if(d.temp===null)et.innerHTML='—<span class=\"unit\">°C</span>';"
        "else et.innerHTML=d.temp+'<span class=\"unit\">°C</span>';}"
      "if(d.pump_ph){document.getElementById('ph_stats').innerHTML=pumpHtml(d.pump_ph);"
        "setStateHtml('dot_ph',d.pump_ph.active);}"
      "if(d.pump_orp){document.getElementById('orp_stats').innerHTML=pumpHtml(d.pump_orp);"
        "setStateHtml('dot_orp',d.pump_orp.active);}"
    "}catch(_){}};}wsConnect();"
  "</script>");
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

  // ORP slope calibration (board op-amp gain compensation)
  if (_storage) {
    float orpMvPerV = _storage->getOrpMvPerV(1000.0f);
    html += F("<label>ORP Slope (mV per Volt)</label><input name='orp_mv_per_v' value='"); html += fmtFloat(orpMvPerV,1); html += F("'>");
    html += F("<small style='color:#888'>Put probe in known buffer (e.g. 256 mV @ 25°C), read live ADC voltage in /console, set slope = buffer_mV / (V_adc − V_zero). Default 1000.</small>");
  }

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
  if (_http.hasArg("orp_mv_per_v") && _storage) {
    float v = parseFloatOr(_http.arg("orp_mv_per_v"), 1000.0f);
    v = constrain(v, 50.0f, 5000.0f);
    _storage->setOrpMvPerV(v);
    requestOrpCalReload();
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
  html += F("var ws;function wsConnect(){ws=new WebSocket('ws://'+location.host+':81/');");
  html += F("ws.onclose=function(){setTimeout(wsConnect,2000);};");
  html += F("ws.onmessage=function(e){try{var d=JSON.parse(e.data); if(d.log){var l=document.getElementById('logs'); l.innerHTML+=d.log+'\\n'; l.scrollTop=l.scrollHeight;}}catch(_){}};}wsConnect();");
  html += F("</script>");
  
  sendFooter(html);
  _http.send(200, "text/html; charset=UTF-8", html);
}

} // namespace io


