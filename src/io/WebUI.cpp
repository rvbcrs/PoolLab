#include "WebUI.h"
#include "MotorController.h"
#include "domain/History.h"
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
  _http.on("/api/history", [this](){ handleApiHistory(); });
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
    /* mobile responsive */
    "@media (max-width:640px){"
      "body{padding:12px 14px}"
      ".topbar{padding:10px 14px;flex-wrap:wrap;gap:8px;border-radius:24px}"
      ".topbar .brand{flex:1 1 auto;font-size:11px;letter-spacing:.15em}"
      ".topbar .brand .node{display:none}"
      ".topbar .status{gap:8px;font-size:9px;letter-spacing:.12em}"
      ".themesw{padding:6px 10px;font-size:9px;letter-spacing:.15em}"
      "h1{font-size:32px;line-height:1.05}"
      ".sub{font-size:13px;margin-bottom:16px}"
      ".metrics{grid-template-columns:1fr;gap:10px;margin-bottom:16px}"
      ".metric{padding:16px}"
      ".label{font-size:9px;letter-spacing:.2em;margin-bottom:10px}"
      ".label .id{font-size:8px;padding:2px 8px}"
      ".val{font-size:44px;letter-spacing:-.03em}"
      ".val .unit{font-size:11px;letter-spacing:.08em}"
      ".range{font-size:9px;letter-spacing:.15em;margin-top:10px;gap:6px;flex-wrap:wrap}"
      ".dosers{margin-top:10px}"
      ".doser{grid-template-columns:1fr;gap:6px;padding:14px 16px}"
      ".doser .nums{text-align:left;font-size:10px;letter-spacing:.1em}"
      ".actions{grid-template-columns:1fr 1fr;gap:10px;margin-top:18px}"
      ".actions .btn.icon{grid-column:1/-1}"
      ".btn{padding:12px 14px;font-size:10px;letter-spacing:.2em}"
      ".alert{padding:12px 14px;gap:10px;font-size:12px}"
      ".alert .ic{flex:0 0 42px;font-size:10px;letter-spacing:.18em}"
      ".foot{font-size:9px;letter-spacing:.2em;margin-top:22px}"
      ".card{padding:20px 16px;margin:14px 8px;border-radius:6px}"
      "h2{font-size:22px}h3{font-size:18px}"
      "input,select{padding:10px 12px;font-size:14px}"  // 14px prevents iOS auto-zoom
      "button[type=submit]{padding:12px;font-size:10px;letter-spacing:.25em}"
      ".modal{padding:8px}.modal-card{max-height:90vh}"
      ".modal-body{font-size:10px;padding:12px 14px}"
    "}"
    "</style></head><body>");
}
void WebUI::sendFooter(String &h){ h += F("</body></html>"); }

void WebUI::handleIndex(){
  // Branch on UI style — 0 = Vapor, 1 = Poolside, 2 = Helder
  int uiStyle = _storage ? _storage->getUiStyle(0) : 0;
  if (uiStyle == 1) {
    String html; renderPoolsideIndex(html); _http.send(200, "text/html", html);
    return;
  }
  if (uiStyle == 2) {
    String html; renderHelderIndex(html); _http.send(200, "text/html", html);
    return;
  }
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
  float pool_m3 = 0.0f;
  if (_storage) pool_m3 = ((float)_storage->getPoolLengthCm() * _storage->getPoolWidthCm() * _storage->getPoolHeightCm()) / 1000000.0f;
  html += "var PH_MIN="+String(phMin,2)+",PH_MAX="+String(phMax,2)+",ORP_MIN="+String(orpMin)+",ORP_MAX="+String(orpMax)+",POOL_M3="+String(pool_m3,1)+";";
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
    "function sodaGrams(dph){return POOL_M3>0?Math.round(75*Math.abs(dph)*POOL_M3):0;}"
    "function fmtDose(g){return g>=1000?(g/1000).toFixed(2)+' kg':g+' g';}"
    "function renderAlerts(){var a=document.getElementById('alerts'),items=[];"
      "if(_curPh!==null){"
        "if(_curPh<PH_MIN){"
          "var target=(PH_MIN+PH_MAX)/2,g=sodaGrams(target-_curPh);"
          "var tip=POOL_M3>0?' <b>Voeg ~'+fmtDose(g)+' natriumcarbonaat (soda) toe</b> voor '+POOL_M3.toFixed(1)+' m³ om naar pH '+target.toFixed(1)+' te gaan.':' Stel je pool-afmetingen in bij <a href=\"/settings\">Settings</a> voor een dosis-tip.';"
          "items.push({k:'bad',ic:'pH ↓',t:'<b>pH te laag ('+_curPh.toFixed(2)+').</b> Water is te zuur.'+tip+'<em>Lage pH veroorzaakt corrosie en irritatie van huid/ogen.</em>'});"
        "}else if(_curPh>PH_MAX){"
          "items.push({k:'bad',ic:'pH ↑',t:'<b>pH te hoog ('+_curPh.toFixed(2)+').</b> Water is te basisch — pH-minus pomp doseert automatisch tot '+PH_MIN.toFixed(2)+'–'+PH_MAX.toFixed(2)+'.<em>Hoge pH vermindert de werking van chloor.</em>'});"
        "}"
      "}"
      "if(_curOrp!==null){"
        "if(_curOrp<ORP_MIN)items.push({k:'bad',ic:'ORP ↓',t:'<b>ORP te laag ('+_curOrp+' mV).</b> Te weinig chloor — chloor-pomp doseert automatisch tot boven '+ORP_MIN+' mV.<em>Lage ORP betekent onvoldoende desinfectie.</em>'});"
        "else if(_curOrp>ORP_MAX)items.push({k:'warn',ic:'ORP ↑',t:'<b>ORP te hoog ('+_curOrp+' mV).</b> Te veel chloor — geen automatische dosering.<em>Wacht tot het gehalte daalt of dun het water aan.</em>'});"
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

// Settings — grouped, style-independent light page (proposal D)
void WebUI::handleSettings(){
  float phMin = _phMin ? *_phMin : (_storage ? _storage->getPhMin(6.80f) : 6.80f);
  float phMax = _phMax ? *_phMax : (_storage ? _storage->getPhMax(7.60f) : 7.60f);
  int orpMin = _orpMin ? *_orpMin : (_storage ? _storage->getOrpMin(250) : 250);
  int orpMax = _orpMax ? *_orpMax : (_storage ? _storage->getOrpMax(850) : 850);
  int m1 = _m1 ? (int)*_m1 : 60; int m2 = _m2 ? (int)*_m2 : 60;
  float m1Flow = _m1Flow ? *_m1Flow : 50.0f; float m2Flow = _m2Flow ? *_m2Flow : 50.0f;
  bool motorsEn = _storage ? _storage->getMotorsEnabled(true) : true;
  float orpMvPerV = _storage ? _storage->getOrpMvPerV(1000.0f) : 1000.0f;
  int uiStyle = _storage ? _storage->getUiStyle(0) : 0;
  int psTheme = _storage ? _storage->getPoolsideTheme(0) : 0;
  int pL = _storage ? _storage->getPoolLengthCm() : 500;
  int pW = _storage ? _storage->getPoolWidthCm() : 300;
  int pH = _storage ? _storage->getPoolHeightCm() : 130;
  String mh = _storage ? _storage->getMqttHost("") : String("");
  uint16_t mp = _storage ? _storage->getMqttPort(1883) : 1883;
  String mu = _storage ? _storage->getMqttUser("") : String("");
  String mw = _storage ? _storage->getMqttPass("") : String("");

  String html;
  html  = F("<!doctype html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1,viewport-fit=cover'>");
  html += F("<title>Pura &middot; Instellingen</title><style>");
  html += F(
    "*{box-sizing:border-box;margin:0;padding:0}"
    "html,body{width:100%;min-height:100vh;font-family:-apple-system,'SF Pro Text',ui-sans-serif,system-ui,'Segoe UI',Roboto,sans-serif;-webkit-font-smoothing:antialiased;color:#12333C;background:#EDF4F5}"
    "a{color:inherit;text-decoration:none}"
    ".st{min-height:100vh;background:linear-gradient(180deg,#FAFDFD 0%,#EDF4F5 100%);display:flex;flex-direction:column;padding:max(44px,env(safe-area-inset-top)) 14px calc(24px + env(safe-area-inset-bottom));gap:12px;max-width:600px;margin:0 auto}"
    ".top{display:flex;align-items:center;gap:10px;font-weight:650;font-size:16px;padding:0 4px}"
    ".top .back{color:#0E7C93;font-size:22px;line-height:1;padding:0 4px}"
    ".grp{background:#fff;border:1px solid #E2EBED;border-radius:14px;overflow:hidden}"
    ".gh{display:flex;align-items:center;gap:9px;padding:12px 14px;font-size:12px;font-weight:700;letter-spacing:.05em;text-transform:uppercase;color:#4A626B;border-bottom:1px solid #EDF2F4}"
    ".gh svg{color:#0E7C93;flex:0 0 auto}"
    ".row{display:flex;justify-content:space-between;align-items:center;gap:12px;padding:11px 14px;border-bottom:1px solid #EDF2F4;font-size:14px}"
    ".row:last-child{border-bottom:0}"
    ".row .k{color:#12333C}"
    ".row .k small{display:block;font-size:11px;color:#8CA0A8;font-weight:400;margin-top:1px}"
    ".col{display:block;padding:11px 14px;border-bottom:1px solid #EDF2F4}"
    ".col:last-child{border-bottom:0}"
    ".col .k{display:block;font-size:12px;color:#4A626B;font-weight:600;margin-bottom:6px}"
    "input[type=number],input[type=text],input[type=password],select{padding:9px 12px;background:#F4F7F8;color:#12333C;border:1px solid #DCE6E9;border-radius:8px;font-family:'SF Mono',ui-monospace,Menlo,monospace;font-size:14px;font-variant-numeric:tabular-nums;outline:none}"
    "input:focus,select:focus{border-color:#0E7C93}"
    "input[type=number]{width:110px;text-align:right}"
    ".col input,.col select{width:100%}"
    /* toggle */
    ".sw{position:relative;width:44px;height:26px;flex:0 0 44px}"
    ".sw input{position:absolute;inset:0;opacity:0;margin:0;cursor:pointer;z-index:1}"
    ".sw i{position:absolute;inset:0;border-radius:99px;background:#CBD8DC;transition:background .2s}"
    ".sw i::after{content:'';position:absolute;top:3px;left:3px;width:20px;height:20px;border-radius:50%;background:#fff;box-shadow:0 1px 3px rgba(0,0,0,.25);transition:left .2s}"
    ".sw input:checked+i{background:#12A594}"
    ".sw input:checked+i::after{left:21px}"
    /* segmented */
    ".seg{display:flex;background:#EDF2F4;border-radius:9px;padding:3px;gap:3px}"
    ".seg label{flex:1;text-align:center;padding:8px 0;border-radius:7px;font-size:13px;font-weight:600;color:#7E939B;cursor:pointer}"
    ".seg input{display:none}"
    ".seg label:has(input:checked){background:#fff;color:#0E7C93;box-shadow:0 1px 3px rgba(18,51,60,.12)}"
    /* theme swatches */
    ".sws{display:grid;grid-template-columns:repeat(4,1fr);gap:8px}"
    ".sws label{display:block;padding:14px 4px;border-radius:10px;cursor:pointer;text-align:center;color:#fff;font-size:11px;font-weight:600;letter-spacing:.1em;border:2px solid transparent}"
    ".sws input{display:none}"
    ".sws label:has(input:checked){border-color:#0E7C93;box-shadow:0 0 0 2px rgba(14,124,147,.25)}"
    "button[type=submit]{width:100%;padding:14px;background:#0E7C93;color:#fff;border:none;cursor:pointer;font-size:14px;font-weight:650;border-radius:12px;margin-top:4px}"
    "button[type=submit]:active{filter:brightness(1.1)}"
    ".hint{font-size:11px;color:#8CA0A8;padding:0 4px}"
  );
  html += F("</style></head><body><div class='st'>");
  html += F("<div class='top'><a class='back' href='/'>&lsaquo;</a> Instellingen</div>");
  html += F("<form method='POST' action='/api/save' style='display:contents'>");

  // Doelwaarden
  html += F("<div class='grp'>"
            "<div class='gh'><svg width='15' height='15' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'><path d='M12 3c3 4.5 6 7.7 6 11a6 6 0 0 1-12 0c0-3.3 3-6.5 6-11z'/></svg>Doelwaarden</div>");
  html += F("<div class='row'><span class='k'>pH doel min</span><input type='number' step='0.05' min='6' max='9' name='ph_min' value='"); html += fmtFloat(phMin,2); html += F("'></div>");
  html += F("<div class='row'><span class='k'>pH doel max</span><input type='number' step='0.05' min='6' max='9' name='ph_max' value='"); html += fmtFloat(phMax,2); html += F("'></div>");
  html += F("<div class='row'><span class='k'>ORP venster min <small>mV</small></span><input type='number' step='10' name='orp_min' value='"); html += String(orpMin); html += F("'></div>");
  html += F("<div class='row'><span class='k'>ORP venster max <small>mV</small></span><input type='number' step='10' name='orp_max' value='"); html += String(orpMax); html += F("'></div>");
  html += F("</div>");

  // Doseerpompen
  html += F("<div class='grp'>"
            "<div class='gh'><svg width='15' height='15' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'><polygon points='13 2 3 14 12 14 11 22 21 10 12 10 13 2'/></svg>Doseerpompen</div>");
  html += F("<div class='row'><span class='k'>Pompen actief</span><span class='sw'><input type='checkbox' name='motors_en' value='1'");
  if (motorsEn) html += F(" checked");
  html += F("><i></i></span></div>");
  html += F("<div class='row'><span class='k'>pH&minus; snelheid <small>% van max</small></span><input type='number' min='0' max='100' name='m1' value='"); html += String(m1); html += F("'></div>");
  html += F("<div class='row'><span class='k'>pH&minus; flow <small>mL/min @ 100%</small></span><input type='number' step='0.1' min='0.1' max='500' name='m1_flow' value='"); html += fmtFloat(m1Flow,1); html += F("'></div>");
  html += F("<div class='row'><span class='k'>Chloor snelheid <small>% van max</small></span><input type='number' min='0' max='100' name='m2' value='"); html += String(m2); html += F("'></div>");
  html += F("<div class='row'><span class='k'>Chloor flow <small>mL/min @ 100%</small></span><input type='number' step='0.1' min='0.1' max='500' name='m2_flow' value='"); html += fmtFloat(m2Flow,1); html += F("'></div>");
  html += F("<div class='row'><span class='k'>Flow kalibreren <small>pomp 60 s op 100%, meet het volume in mL = flow</small></span></div>");
  html += F("<div class='row'><span class='k'>ORP slope <small>mV per Volt &middot; buffer 256 mV, zie /console</small></span><input type='number' step='1' min='50' max='5000' name='orp_mv_per_v' value='"); html += fmtFloat(orpMvPerV,1); html += F("'></div>");
  html += F("</div>");

  // Weergave
  html += F("<div class='grp'>"
            "<div class='gh'><svg width='15' height='15' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'><rect x='3' y='4' width='18' height='14' rx='2'/><path d='M8 21h8M12 18v3'/></svg>Weergave</div>");
  html += F("<div class='col'><span class='k'>Stijl</span><div class='seg'>");
  const char* styleNames[] = {"Vapor","Poolside","Helder"};
  for (int i=0;i<3;i++) {
    html += F("<label><input type='radio' name='ui_style' value='"); html += String(i); html += F("'");
    if (uiStyle == i) html += F(" checked");
    html += F(">"); html += styleNames[i]; html += F("</label>");
  }
  html += F("</div></div>");
  html += F("<div class='col'><span class='k'>Poolside-thema</span><div class='sws'>");
  const char* themeNames[] = {"Ocean","Sunset","Midnight","Verdant"};
  const char* themeGrads[] = {
    "linear-gradient(135deg,#2f6aa0,#0b2a4a)",
    "linear-gradient(135deg,#b8574a,#3d2038)",
    "linear-gradient(135deg,#1a1f2e,#050810)",
    "linear-gradient(135deg,#2b7a6f,#0a2e28)"
  };
  for (int i=0;i<4;i++) {
    html += F("<label style='background:"); html += themeGrads[i]; html += F("'>");
    html += F("<input type='radio' name='ps_theme' value='"); html += String(i); html += F("'");
    if (psTheme == i) html += F(" checked");
    html += F(">"); html += themeNames[i]; html += F("</label>");
  }
  html += F("</div></div></div>");

  // Bad & netwerk
  html += F("<div class='grp'>"
            "<div class='gh'><svg width='15' height='15' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2' stroke-linecap='round'><circle cx='12' cy='12' r='10'/><path d='M2 12h20M12 2a15 15 0 0 1 0 20 15 15 0 0 1 0-20z'/></svg>Bad &amp; netwerk</div>");
#if HAS_ZIGBEE
  {
    core::Storage::Mode modeNow = _storage ? _storage->getMode(core::Storage::MODE_WIFI_MQTT) : core::Storage::MODE_WIFI_MQTT;
    html += F("<div class='col'><span class='k'>Netwerkmodus</span><select name='mode'>");
    html += F("<option value='wifi'"); if (modeNow==core::Storage::MODE_WIFI_MQTT) html += F(" selected"); html += F(">WiFi/MQTT</option>");
    html += F("<option value='zigbee'"); if (modeNow==core::Storage::MODE_ZIGBEE) html += F(" selected"); html += F(">Zigbee</option>");
    html += F("</select></div>");
  }
#endif
  html += F("<div class='row'><span class='k'>Lengte <small>cm</small></span><input type='number' min='50' max='5000' name='pool_l' id='pl' value='"); html += String(pL); html += F("'></div>");
  html += F("<div class='row'><span class='k'>Breedte <small>cm</small></span><input type='number' min='50' max='5000' name='pool_w' id='pw' value='"); html += String(pW); html += F("'></div>");
  html += F("<div class='row'><span class='k'>Waterhoogte <small>cm</small></span><input type='number' min='30' max='500' name='pool_h' id='ph' value='"); html += String(pH); html += F("'></div>");
  html += F("<div class='row'><span class='k'>Volume <small>voor doseer-tips</small></span><span class='k' id='vol' style=\"font-family:'SF Mono',ui-monospace,Menlo,monospace\"></span></div>");
  html += F("<div class='col'><span class='k'>MQTT host <small style='display:inline'>voor Home Assistant</small></span><input type='text' name='mqtt_host' value='"); html += mh; html += F("'></div>");
  html += F("<div class='row'><span class='k'>MQTT poort</span><input type='number' min='1' max='65535' name='mqtt_port' value='"); html += String((unsigned)mp); html += F("'></div>");
  html += F("<div class='col'><span class='k'>MQTT gebruiker</span><input type='text' name='mqtt_user' value='"); html += mu; html += F("'></div>");
  html += F("<div class='col'><span class='k'>MQTT wachtwoord</span><input type='password' name='mqtt_pass' value='"); html += mw; html += F("'></div>");
  html += F("</div>");

  html += F("<button type='submit'>Opslaan</button>");
  html += F("<div class='hint'>Veiligheidslimieten en WhatsApp-meldingen staan onder <a href='/safety' style='color:#0E7C93'>Safety</a>.</div>");
  html += F("</form></div>");

  html += F("<script>"
    "function vol(){var l=+document.getElementById('pl').value||0,w=+document.getElementById('pw').value||0,h=+document.getElementById('ph').value||0;"
    "document.getElementById('vol').textContent='~'+(l*w*h/1e6).toFixed(1)+' m\\u00B3';}"
    "['pl','pw','ph'].forEach(function(i){document.getElementById(i).addEventListener('input',vol);});vol();"
  "</script></body></html>");
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

// 24h sensor history + 7-day dosing (6 stored days + today live)
void WebUI::handleApiHistory(){
  String j = "{";
  domain::History::instance().toJson(j);
  float m1[7] = {0}, m2[7] = {0};
  if (_storage) _storage->getDoseHistory(m1, m2);
  float today1 = 0, today2 = 0;
  if (_motor) { today1 = _motor->getM1Stats().dailyVolumeMl; today2 = _motor->getM2Stats().dailyVolumeMl; }
  j += ",\"dose_ph\":[";
  for (int i = 1; i < 7; i++) { j += fmtFloat(m1[i],1); j += ','; }
  j += fmtFloat(today1,1); j += "],\"dose_orp\":[";
  for (int i = 1; i < 7; i++) { j += fmtFloat(m2[i],1); j += ','; }
  j += fmtFloat(today2,1); j += "]}";
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
  if (_storage) {
    if (_http.hasArg("pool_l")) _storage->setPoolLengthCm(constrain(parseIntOr(_http.arg("pool_l"), 500), 50, 5000));
    if (_http.hasArg("pool_w")) _storage->setPoolWidthCm(constrain(parseIntOr(_http.arg("pool_w"), 300), 50, 5000));
    if (_http.hasArg("pool_h")) _storage->setPoolHeightCm(constrain(parseIntOr(_http.arg("pool_h"), 130), 30, 500));
    if (_http.hasArg("ui_style")) _storage->setUiStyle(constrain(parseIntOr(_http.arg("ui_style"), 0), 0, 2));
    if (_http.hasArg("ps_theme")) _storage->setPoolsideTheme(constrain(parseIntOr(_http.arg("ps_theme"), 0), 0, 3));
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

// ============================================================================
// Poolside UI — alternative theme (WaterGuru-inspired), 4 color variants
// ============================================================================
// ============================================================================
// Helder UI — light, daylight-readable style (ui_style = 2)
// ============================================================================
void WebUI::renderHelderIndex(String &html) {
  float phMin = _phMin ? *_phMin : 6.80f;
  float phMax = _phMax ? *_phMax : 7.60f;
  int   orpMin = _orpMin ? *_orpMin : 250;
  int   orpMax = _orpMax ? *_orpMax : 850;
  float vol_m3 = 0.0f;
  if (_storage) vol_m3 = ((float)_storage->getPoolLengthCm() * _storage->getPoolWidthCm() * _storage->getPoolHeightCm()) / 1000000.0f;

  html  = F("<!doctype html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1,viewport-fit=cover'>");
  html += F("<title>Pura</title><style>");
  html += F(
    "*{box-sizing:border-box;margin:0;padding:0}"
    "html,body{width:100%;min-height:100vh;font-family:-apple-system,'SF Pro Text',ui-sans-serif,system-ui,'Segoe UI',Roboto,sans-serif;-webkit-font-smoothing:antialiased;color:#12333C;background:#EDF4F5}"
    "a{color:inherit;text-decoration:none}"
    ".hl{min-height:100vh;background:linear-gradient(180deg,#FAFDFD 0%,#EDF4F5 100%);display:flex;flex-direction:column;padding:max(44px,env(safe-area-inset-top)) 16px calc(8px + env(safe-area-inset-bottom));gap:12px;max-width:600px;margin:0 auto}"
    ".top{display:flex;justify-content:space-between;align-items:center;padding:0 4px}"
    ".top .loc{font-weight:650;font-size:16px;display:flex;gap:7px;align-items:center}"
    ".top svg{color:#5E7780}"
    ".verdict{padding:4px 4px 0}"
    ".badge{display:inline-flex;align-items:center;gap:7px;font-size:12px;font-weight:600;border-radius:99px;padding:5px 12px}"
    ".badge i{width:7px;height:7px;border-radius:50%;display:inline-block}"
    ".badge.ok{color:#0E7C93;background:#DFF0F3}.badge.ok i{background:#12A594}"
    ".badge.warn{color:#8A5412;background:#FBEEDC}.badge.warn i{background:#E5A33C}"
    ".badge.crit{color:#A33A3A;background:#FCE4E4}.badge.crit i{background:#E05252}"
    ".verdict h1{font-weight:250;font-size:30px;letter-spacing:-.02em;margin:10px 0 2px;line-height:1.12}"
    ".verdict .when{font-size:12px;color:#7E939B;font-family:'SF Mono',ui-monospace,Menlo,monospace;font-variant-numeric:tabular-nums}"
    ".card{background:#fff;border:1px solid #E2EBED;border-radius:16px;padding:14px 16px;box-shadow:0 2px 10px rgba(18,51,60,.05)}"
    ".mrow{display:flex;justify-content:space-between;align-items:baseline}"
    ".mrow .lbl{font-size:13px;font-weight:600;color:#4A626B}"
    ".mrow .v{font-size:26px;font-weight:300;font-family:'SF Mono',ui-monospace,Menlo,monospace;font-variant-numeric:tabular-nums}"
    ".mrow .v small{font-size:12px;color:#7E939B;font-weight:400}"
    ".mrow .v.warn{color:#B4691E}.mrow .v.crit{color:#C24444}"
    /* band gauge: display range = target window +25% each side, so zone stops are constant */
    ".band{position:relative;height:8px;border-radius:4px;margin:12px 0 4px;background:linear-gradient(90deg,#F0837B 0 16.7%,#F2C063 16.7% 23.3%,#63C6B4 23.3% 76.7%,#F2C063 76.7% 83.3%,#F0837B 83.3% 100%)}"
    ".band .pin{position:absolute;top:-4px;width:16px;height:16px;border-radius:50%;background:#fff;border:3px solid #0E7C93;box-shadow:0 1px 4px rgba(0,0,0,.25);transform:translateX(-8px);transition:left .4s;left:-20px}"
    ".bandlbl{display:flex;justify-content:space-between;font-family:'SF Mono',ui-monospace,Menlo,monospace;font-size:9px;color:#9AACB3}"
    ".spark{width:100%;height:34px;margin-top:8px}"
    ".duo{display:grid;grid-template-columns:1fr 1fr;gap:10px}"
    ".mini .lbl{font-size:12px;color:#4A626B;font-weight:600}"
    ".mini .v{font-size:20px;font-weight:300;font-family:'SF Mono',ui-monospace,Menlo,monospace;font-variant-numeric:tabular-nums;margin-top:2px}"
    ".mini .v small{font-size:11px;color:#7E939B}"
    ".dose{display:flex;justify-content:space-between;align-items:center}"
    ".dose .t{font-size:12px;color:#4A626B}"
    ".dose .t b{display:block;font-size:13px;color:#12333C;margin-bottom:1px}"
    ".dose .st{font-size:12px;color:#7E939B;font-weight:600}"
    ".dose .st.on{color:#0E7C93;animation:hldose 1.2s ease-in-out infinite}"
    "@keyframes hldose{0%,100%{opacity:1}50%{opacity:.4}}"
    "@media(prefers-reduced-motion:reduce){.dose .st.on{animation:none}}"
    ".nav{margin-top:auto;display:grid;grid-template-columns:repeat(4,1fr);border-top:1px solid #E2EBED;padding-top:10px}"
    ".nav a{display:flex;flex-direction:column;align-items:center;gap:3px;font-size:10px;color:#8CA0A8}"
    ".nav a.on{color:#0E7C93}"
  );
  html += F("</style></head><body><div class='hl'>");

  // topbar
  html += F("<div class='top'><div class='loc'>"
            "<svg width='16' height='16' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'><path d='M12 2a7 7 0 0 1 7 7c0 5-7 13-7 13S5 14 5 9a7 7 0 0 1 7-7z'/><circle cx='12' cy='9' r='2.5'/></svg>"
            "Bluey</div>"
            "<a href='/settings' aria-label='Instellingen'><svg width='19' height='19' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2' stroke-linecap='round'><path d='M20 7h-9M14 17H5'/><circle cx='17' cy='17' r='3'/><circle cx='7' cy='7' r='3'/></svg></a></div>");

  // verdict
  html += F("<div class='verdict'>"
            "<span class='badge ok' id='hlBadge'><i></i><span id='hlBadgeTxt'>Wachten op meting&hellip;</span></span>"
            "<h1 id='hlHead'>Even geduld.</h1>"
            "<div class='when' id='hlWhen'>live &middot; 2 Hz</div>"
          "</div>");

  // pH card with band + sparkline
  html += F("<div class='card'>"
            "<div class='mrow'><span class='lbl'>pH</span><span class='v' id='hlPhVal'>--</span></div>"
            "<div class='band'><span class='pin' id='hlPhPin'></span></div>"
            "<div class='bandlbl'><span id='hlPhLo'></span><span>doel ");
  html += fmtFloat(phMin,2); html += F(" &ndash; "); html += fmtFloat(phMax,2);
  html += F("</span><span id='hlPhHi'></span></div>"
            "<svg class='spark' id='hlPhSpark' viewBox='0 0 280 34' preserveAspectRatio='none'></svg>"
            "<div class='bandlbl'><span>&minus;24 u</span><span>nu</span></div>"
          "</div>");

  // ORP card with band
  html += F("<div class='card'>"
            "<div class='mrow'><span class='lbl'>ORP</span><span class='v' id='hlOrpVal'>--<small> mV</small></span></div>"
            "<div class='band'><span class='pin' id='hlOrpPin'></span></div>"
            "<div class='bandlbl'><span id='hlOrpLo'></span><span>venster ");
  html += String(orpMin); html += F(" &ndash; "); html += String(orpMax);
  html += F("</span><span id='hlOrpHi'></span></div>"
          "</div>");

  // temp + volume
  html += F("<div class='duo'>"
            "<div class='card mini'><div class='lbl'>Water</div><div class='v' id='hlTempVal'>--<small> &deg;C</small></div></div>"
            "<div class='card mini'><div class='lbl'>Volume</div><div class='v'>");
  html += fmtFloat(vol_m3,1); html += F("<small> m&sup3;</small></div></div></div>");

  // dosing today
  html += F("<div class='card dose'>"
            "<div class='t'><b>Dosering vandaag</b><span id='hlDoseTxt'>chloor 0 mL &middot; pH&minus; 0 mL</span></div>"
            "<span class='st' id='hlDoseSt'>rust</span>"
          "</div>");

  // nav
  html += F("<div class='nav'>"
            "<a class='on' href='/'><svg width='18' height='18' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'><path d='M12 3c3 4.5 6 7.7 6 11a6 6 0 0 1-12 0c0-3.3 3-6.5 6-11z'/></svg>Status</a>"
            "<a href='/safety'><svg width='18' height='18' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'><path d='M12 22s8-3.5 8-10V5l-8-3-8 3v7c0 6.5 8 10 8 10z'/></svg>Safety</a>"
            "<a href='/settings'><svg width='18' height='18' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2' stroke-linecap='round'><path d='M20 7h-9M14 17H5'/><circle cx='17' cy='17' r='3'/><circle cx='7' cy='7' r='3'/></svg>Instellen</a>"
            "<a href='/console'><svg width='18' height='18' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'><polyline points='4 17 10 11 4 5'/><line x1='12' y1='19' x2='20' y2='19'/></svg>Console</a>"
          "</div>");

  html += F("</div>"); // /hl

  html += F("<script>");
  html += "var PH_MIN=" + String(phMin, 2) + ",PH_MAX=" + String(phMax, 2);
  html += ",ORP_MIN=" + String(orpMin) + ",ORP_MAX=" + String(orpMax) + ";";
  html += F(
    "var curPh=null,curOrp=null,anyDosing=false;"
    "function $(i){return document.getElementById(i);}"
    /* display range = window +25% each side (matches the .band gradient stops) */
    "function lo(mn,mx){return mn-.25*(mx-mn);}function hi(mn,mx){return mx+.25*(mx-mn);}"
    "function pin(id,v,mn,mx){var e=$(id);if(!e)return;if(v==null){e.style.left='-20px';return;}"
      "var p=(v-lo(mn,mx))/(hi(mn,mx)-lo(mn,mx))*100;e.style.left=Math.max(2,Math.min(98,p))+'%';}"
    "function zone(v,mn,mx){if(v==null)return null;if(v<mn||v>mx)return 'crit';var m=(mx-mn)*.1;"
      "return(v<mn+m||v>mx-m)?'warn':'ok';}"
    "function verdict(){var b=$('hlBadge'),t=$('hlBadgeTxt'),h=$('hlHead');"
      "if(curPh==null&&curOrp==null){b.className='badge ok';t.textContent='Wachten op meting\\u2026';h.textContent='Even geduld.';return;}"
      "var zp=zone(curPh,PH_MIN,PH_MAX),zo=zone(curOrp,ORP_MIN,ORP_MAX);"
      "var w=(zp=='crit'||zo=='crit')?'crit':(zp=='warn'||zo=='warn')?'warn':'ok';"
      "b.className='badge '+w;"
      "if(w=='ok'){t.textContent='Alles in balans';h.textContent='Je water is gezond.';}"
      "else if(w=='warn'){t.textContent='Bijna op de grens';h.textContent='Houd je water in de gaten.';}"
      "else{t.textContent='Actie nodig';h.textContent='Je water heeft aandacht nodig.';}}"
    "function vcls(id,z){var e=$(id);e.className='v'+(z=='warn'?' warn':z=='crit'?' crit':'');}"
    "function setPh(v){curPh=v;$('hlPhVal').textContent=(v==null)?'--':v.toFixed(2);"
      "pin('hlPhPin',v,PH_MIN,PH_MAX);vcls('hlPhVal',zone(v,PH_MIN,PH_MAX));verdict();stamp();}"
    "function setOrp(v){curOrp=v;$('hlOrpVal').innerHTML=(v==null)?'--<small> mV</small>':Math.round(v)+'<small> mV</small>';"
      "pin('hlOrpPin',v,ORP_MIN,ORP_MAX);vcls('hlOrpVal',zone(v,ORP_MIN,ORP_MAX));verdict();stamp();}"
    "function setTemp(v){$('hlTempVal').innerHTML=(v==null)?'--<small> \\u00B0C</small>':v.toFixed(1)+'<small> \\u00B0C</small>';}"
    "function stamp(){$('hlWhen').textContent='zojuist gemeten \\u00B7 live 2 Hz';}"
    "function fmt(ml){return ml>=1000?(ml/1000).toFixed(2)+' L':Math.round(ml)+' mL';}"
    "var todayPh=0,todayOrp=0;"
    "function dose(){$('hlDoseTxt').innerHTML='chloor '+fmt(todayOrp)+' &middot; pH&minus; '+fmt(todayPh);"
      "var s=$('hlDoseSt');s.textContent=anyDosing?'\\u25CF doseert':'rust';s.className='st'+(anyDosing?' on':'');}"
    "$('hlPhLo').textContent=lo(PH_MIN,PH_MAX).toFixed(2);$('hlPhHi').textContent=hi(PH_MIN,PH_MAX).toFixed(2);"
    "$('hlOrpLo').textContent=Math.round(lo(ORP_MIN,ORP_MAX));$('hlOrpHi').textContent=Math.round(hi(ORP_MIN,ORP_MAX));"
    "function spark(id,arr,w,h){var el=$(id);if(!el)return;var vals=[],i;"
      "for(i=0;i<arr.length;i++)if(arr[i]!=null)vals.push(arr[i]);"
      "if(vals.length<2){el.innerHTML='';return;}"
      "var mn=Math.min.apply(null,vals),mx=Math.max.apply(null,vals);if(mx-mn<1e-9){mn-=1;mx+=1;}"
      "var pts=[],lx=0,ly=0,n=arr.length;"
      "for(i=0;i<n;i++){if(arr[i]==null)continue;lx=i/(n-1)*w;ly=h-2-(arr[i]-mn)/(mx-mn)*(h-4);pts.push(lx.toFixed(1)+','+ly.toFixed(1));}"
      "el.innerHTML='<polygon points=\"'+pts[0].split(',')[0]+','+h+' '+pts.join(' ')+' '+lx.toFixed(1)+','+h+'\" fill=\"rgba(14,124,147,.08)\"/>'"
      "+'<polyline points=\"'+pts.join(' ')+'\" fill=\"none\" stroke=\"#0E7C93\" stroke-width=\"1.6\" stroke-linecap=\"round\"/>'"
      "+'<circle cx=\"'+lx.toFixed(1)+'\" cy=\"'+ly.toFixed(1)+'\" r=\"2.6\" fill=\"#0E7C93\"/>';}"
    "function loadHist(){fetch('/api/history').then(function(r){return r.json();}).then(function(h){"
      "spark('hlPhSpark',h.ph,280,34);}).catch(function(){});}"
    "loadHist();setInterval(loadHist,300000);"
    "var ws;function wsConn(){ws=new WebSocket('ws://'+location.host+':81/');ws.onclose=function(){setTimeout(wsConn,2000);};"
      "ws.onmessage=function(e){try{var d=JSON.parse(e.data);var d1=false,d2=false;"
      "if(d.pump_ph){d1=d.pump_ph.active;todayPh=d.pump_ph.daily;}"
      "if(d.pump_orp){d2=d.pump_orp.active;todayOrp=d.pump_orp.daily;}"
      "if(d.pump_ph||d.pump_orp){anyDosing=d1||d2;dose();}"
      "if(d.ph!==undefined)setPh(d.ph);"
      "if(d.orp!==undefined)setOrp(d.orp);"
      "if(d.temp!==undefined)setTemp(d.temp);"
      "}catch(_){}};}"
    "wsConn();"
    "fetch('/api/state').then(function(r){return r.json();}).then(function(d){"
      "if(d.ph!=null)setPh(d.ph);if(d.orp!=null)setOrp(d.orp);if(d.temp!=null)setTemp(d.temp);}).catch(function(){});"
  );
  html += F("</script></body></html>");
}

void WebUI::renderPoolsideIndex(String &html) {
  float phMin = _phMin ? *_phMin : 6.80f;
  float phMax = _phMax ? *_phMax : 7.60f;
  int   orpMin = _orpMin ? *_orpMin : 250;
  int   orpMax = _orpMax ? *_orpMax : 850;
  int theme = _storage ? _storage->getPoolsideTheme(0) : 0;
  const char* themeClass = "theme-ocean";
  if (theme == 1) themeClass = "theme-sunset";
  else if (theme == 2) themeClass = "theme-midnight";
  else if (theme == 3) themeClass = "theme-verdant";

  html  = F("<!doctype html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1,viewport-fit=cover'>");
  html += F("<title>Pura</title><style>");
  html += F(
    "*{box-sizing:border-box;margin:0;padding:0}"
    "html,body{width:100%;min-height:100vh;font-family:-apple-system,'SF Pro Text',ui-sans-serif,system-ui,'Segoe UI',Roboto,sans-serif;-webkit-font-smoothing:antialiased;color:var(--ps-ink);background:var(--ps-bot)}"
    "a{color:inherit;text-decoration:none}"
    ".ps{min-height:100vh;background:radial-gradient(120% 60% at 50% 0%,var(--ps-glow) 0%,transparent 50%),linear-gradient(180deg,var(--ps-top) 0%,var(--ps-mid) 45%,var(--ps-bot) 100%);display:flex;flex-direction:column;padding:max(48px,env(safe-area-inset-top)) 14px calc(8px + env(safe-area-inset-bottom));gap:11px;max-width:600px;margin:0 auto}"
    ".ps-top{display:flex;align-items:center;justify-content:space-between;padding:0 4px 2px}"
    ".ps-top .pool-select{display:inline-flex;align-items:center;gap:6px;font-weight:600;font-size:16px}"
    ".ps-top .pool-select .chev{font-size:11px;opacity:.6}"
    ".ps-top .cog{width:34px;height:34px;display:grid;place-items:center;color:var(--ps-ink-soft)}"
    ".glass{background:var(--ps-card);border:1px solid var(--ps-line);border-radius:16px;backdrop-filter:blur(14px)}"
    /* alert banner */
    ".ps-alert{display:none;align-items:center;gap:10px;padding:12px 14px;background:rgba(255,95,122,.12);border:1px solid rgba(255,95,122,.25);border-radius:12px;color:var(--ps-ink)}"
    ".ps-alert.show{display:flex}"
    ".ps-alert .badge{width:24px;height:24px;flex:0 0 24px;background:var(--ps-crit);color:#fff;display:grid;place-items:center;border-radius:8px;font-weight:700;font-size:13px}"
    ".ps-alert .msg{flex:1;font-size:13px;font-weight:600;line-height:1.35}"
    ".ps-alert .chev{color:var(--ps-ink-soft);font-size:14px}"
    /* score card */
    ".score{padding:16px;display:grid;grid-template-columns:auto 1fr;gap:16px;align-items:center}"
    ".score .s1{font-size:12px;color:var(--ps-ink-soft)}"
    ".score .s2{font-size:19px;font-weight:600;line-height:1.25;margin:2px 0}"
    ".score .s3{font-size:11px;color:var(--ps-ink-soft)}"
    ".score circle{transition:stroke-dasharray .6s,stroke .3s}"
    /* metric cards */
    ".duo{display:grid;grid-template-columns:1fr 1fr;gap:10px}"
    ".metric{padding:12px 14px}"
    ".metric .l{display:flex;justify-content:space-between;align-items:baseline;font-size:11px;color:var(--ps-ink-soft)}"
    ".metric .st{font-size:10px}"
    ".metric .st.ok{color:var(--ps-accent)}"
    ".metric .st.warn{color:var(--ps-warn)}"
    ".metric .st.crit{color:var(--ps-crit)}"
    ".metric .st.dose{color:var(--ps-accent);animation:psdose 1.2s ease-in-out infinite}"
    "@keyframes psdose{0%,100%{opacity:1}50%{opacity:.4}}"
    "@media(prefers-reduced-motion:reduce){.metric .st.dose{animation:none}}"
    ".metric .v{font-size:25px;font-weight:250;font-variant-numeric:tabular-nums;margin:3px 0 0;letter-spacing:-.02em}"
    ".metric .v small{font-size:11px;color:var(--ps-ink-soft);font-weight:400}"
    ".metric .sp{width:100%;height:22px;margin-top:6px}"
    /* temp trend */
    ".trendcard{padding:13px 14px}"
    ".trendcard .l{display:flex;justify-content:space-between;font-size:11px;color:var(--ps-ink-soft);margin-bottom:6px}"
    ".trendcard .l b{color:var(--ps-ink);font-weight:600;font-variant-numeric:tabular-nums}"
    ".trendcard svg{width:100%;height:30px}"
    /* dose bars */
    ".doses{padding:13px 14px}"
    ".doses .l{font-size:11px;color:var(--ps-ink-soft);margin-bottom:8px;display:flex;justify-content:space-between;gap:8px;flex-wrap:wrap}"
    ".doses .l .lg i{display:inline-block;width:8px;height:8px;border-radius:2px;margin:0 3px 0 8px}"
    ".doses .today{font-size:10px;color:var(--ps-ink-muted);margin-top:6px;font-variant-numeric:tabular-nums}"
    ".bars{display:flex;align-items:flex-end;gap:7px;height:56px}"
    ".bars .b{flex:1;display:flex;flex-direction:column;gap:2px;justify-content:flex-end;height:100%}"
    ".bars .b i{display:block;border-radius:2px}"
    ".bars .b span{font-size:8px;text-align:center;color:var(--ps-ink-muted);margin-top:2px}"
    /* nav */
    ".ps-nav{margin-top:auto;display:grid;grid-template-columns:repeat(4,1fr);padding:10px 0 2px;border-top:1px solid var(--ps-line)}"
    ".ps-nav-item{display:flex;flex-direction:column;align-items:center;gap:4px;font-size:9px;color:var(--ps-ink-muted)}"
    ".ps-nav-item.active{color:var(--ps-accent)}"
    /* theme tokens */
    ".theme-ocean{--ps-top:#2f6aa0;--ps-mid:#1a4a7a;--ps-bot:#0b2a4a;--ps-glow:rgba(94,200,255,.14);--ps-accent:#5ec8ff;--ps-accent-soft:#82d4ff;--ps-ink:#fff;--ps-ink-soft:rgba(255,255,255,.7);--ps-ink-muted:rgba(255,255,255,.5);--ps-card:rgba(255,255,255,.06);--ps-line:rgba(255,255,255,.10);--ps-warn:#f4b544;--ps-crit:#ff5f7a}"
    ".theme-sunset{--ps-top:#b8574a;--ps-mid:#7a3a3f;--ps-bot:#3d2038;--ps-glow:rgba(255,140,105,.16);--ps-accent:#ff8f6b;--ps-accent-soft:#ffb090;--ps-ink:#fff2e8;--ps-ink-soft:rgba(255,242,232,.72);--ps-ink-muted:rgba(255,242,232,.48);--ps-card:rgba(255,220,200,.06);--ps-line:rgba(255,220,200,.12);--ps-warn:#f4c26a;--ps-crit:#d9445a}"
    ".theme-midnight{--ps-top:#1a1f2e;--ps-mid:#0d1220;--ps-bot:#050810;--ps-glow:rgba(150,220,255,.08);--ps-accent:#a8dcff;--ps-accent-soft:#c8e8ff;--ps-ink:#f0f4fa;--ps-ink-soft:rgba(240,244,250,.7);--ps-ink-muted:rgba(240,244,250,.4);--ps-card:rgba(255,255,255,.04);--ps-line:rgba(255,255,255,.08);--ps-warn:#e8b544;--ps-crit:#f56b7a}"
    ".theme-verdant{--ps-top:#2b7a6f;--ps-mid:#1a5548;--ps-bot:#0a2e28;--ps-glow:rgba(103,232,180,.14);--ps-accent:#67e8b4;--ps-accent-soft:#a0f0c8;--ps-ink:#e8fff4;--ps-ink-soft:rgba(232,255,244,.72);--ps-ink-muted:rgba(232,255,244,.48);--ps-card:rgba(200,255,220,.05);--ps-line:rgba(200,255,220,.10);--ps-warn:#f4c86a;--ps-crit:#ff6b7a}"
  );
  html += F("</style></head><body>");
  html += F("<div class='ps "); html += themeClass; html += F("'>");

  // topbar
  html += F("<div class='ps-top'><div class='pool-select'>Bluey <span class='chev'>&#9662;</span></div>"
            "<a class='cog' href='/settings' aria-label='Instellingen'>"
            "<svg width='19' height='19' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2' stroke-linecap='round'><path d='M20 7h-9M14 17H5'/><circle cx='17' cy='17' r='3'/><circle cx='7' cy='7' r='3'/></svg></a></div>");

  // alert banner (populated by JS)
  html += F("<a class='ps-alert' id='psAlert' href='/safety'><div class='badge' id='psAlertBadge'>1</div><div class='msg' id='psAlertMsg'></div><span class='chev'>&rsaquo;</span></a>");

  // score card
  html += F("<div class='glass score'>"
            "<svg width='86' height='86' viewBox='0 0 86 86'>"
            "<circle cx='43' cy='43' r='36' fill='none' stroke='var(--ps-line)' stroke-width='7'/>"
            "<circle id='psRing' cx='43' cy='43' r='36' fill='none' stroke='var(--ps-accent)' stroke-width='7' stroke-linecap='round' stroke-dasharray='0 226.2' transform='rotate(-90 43 43)'/>"
            "<text id='psScoreN' x='43' y='41' text-anchor='middle' fill='var(--ps-ink)' font-size='24' font-weight='300'>--</text>"
            "<text x='43' y='56' text-anchor='middle' fill='var(--ps-ink-soft)' font-size='8'>SCORE</text>"
            "</svg>"
            "<div class='txt'><div class='s1'>Waterkwaliteit</div><div class='s2' id='psScoreWord'>Wachten op meting&hellip;</div><div class='s3' id='psScoreSub'></div></div>"
          "</div>");

  // pH + ORP cards with sparklines
  html += F("<div class='duo'>"
            "<div class='glass metric'><div class='l'><span>pH</span><span class='st' id='psPhSt'></span></div><div class='v' id='psPhVal'>--</div>"
            "<svg class='sp' id='psPhSpark' viewBox='0 0 120 22' preserveAspectRatio='none'></svg></div>"
            "<div class='glass metric'><div class='l'><span>ORP</span><span class='st' id='psOrpSt'></span></div><div class='v' id='psOrpVal'>--<small> mV</small></div>"
            "<svg class='sp' id='psOrpSpark' viewBox='0 0 120 22' preserveAspectRatio='none'></svg></div>"
          "</div>");

  // temperature trend
  html += F("<div class='glass trendcard'><div class='l'><span>Watertemperatuur</span><b id='psTempVal'>--</b></div>"
            "<svg id='psTempSpark' viewBox='0 0 280 30' preserveAspectRatio='none'></svg></div>");

  // 7-day dosing bars
  html += F("<div class='glass doses'>"
            "<div class='l'><span>Dosering &middot; 7 dagen</span>"
            "<span class='lg'><i style='background:var(--ps-accent)'></i>chloor<i style='background:var(--ps-crit);opacity:.75'></i>pH&minus;</span></div>"
            "<div class='bars' id='psBars'></div>"
            "<div class='today' id='psDoseToday'></div>"
          "</div>");

  // nav
  html += F("<div class='ps-nav'>"
            "<a class='ps-nav-item active' href='/'><svg width='18' height='18' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'><path d='M12 3c3 4.5 6 7.7 6 11a6 6 0 0 1-12 0c0-3.3 3-6.5 6-11z'/></svg><span>Status</span></a>"
            "<a class='ps-nav-item' href='/safety'><svg width='18' height='18' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'><path d='M12 22s8-3.5 8-10V5l-8-3-8 3v7c0 6.5 8 10 8 10z'/></svg><span>Safety</span></a>"
            "<a class='ps-nav-item' href='/settings'><svg width='18' height='18' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2' stroke-linecap='round'><path d='M20 7h-9M14 17H5'/><circle cx='17' cy='17' r='3'/><circle cx='7' cy='7' r='3'/></svg><span>Instellen</span></a>"
            "<a class='ps-nav-item' href='/console'><svg width='18' height='18' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'><polyline points='4 17 10 11 4 5'/><line x1='12' y1='19' x2='20' y2='19'/></svg><span>Console</span></a>"
          "</div>");

  html += F("</div>"); // /ps

  html += F("<script>");
  html += "var PH_MIN=" + String(phMin, 2) + ",PH_MAX=" + String(phMax, 2);
  html += ",ORP_MIN=" + String(orpMin) + ",ORP_MAX=" + String(orpMax) + ";";
  html += F(
    "var curPh=null,curOrp=null,phDosing=false,orpDosing=false;"
    "function $(i){return document.getElementById(i);}"
    /* score: 100 in the inner 60% of the target window, 50 at the edge, 0 well outside */
    "function sub(v,mn,mx){var c=(mn+mx)/2,h=(mx-mn)/2;if(h<=0)return 0;var d=Math.abs(v-c)/h;return Math.max(0,Math.round(100-Math.max(0,d-.6)*125));}"
    "function scoreWord(s){return s>=85?'Uitstekend':s>=70?'Goed':s>=50?'Matig':'Actie nodig';}"
    "function renderScore(){var n=$('psScoreN'),r=$('psRing'),w=$('psScoreWord'),sb=$('psScoreSub');"
      "if(curPh==null||curOrp==null){n.textContent='--';w.textContent='Wachten op meting\\u2026';sb.textContent='';r.setAttribute('stroke-dasharray','0 226.2');return;}"
      "var s=Math.min(sub(curPh,PH_MIN,PH_MAX),sub(curOrp,ORP_MIN,ORP_MAX));"
      "n.textContent=s;w.textContent=scoreWord(s);"
      "r.setAttribute('stroke-dasharray',(s/100*226.2).toFixed(1)+' 226.2');"
      "r.setAttribute('stroke',s>=70?'var(--ps-accent)':s>=50?'var(--ps-warn)':'var(--ps-crit)');"
      "var m=[];if(curPh<PH_MIN)m.push('pH te laag');else if(curPh>PH_MAX)m.push('pH te hoog');"
      "if(curOrp<ORP_MIN)m.push('chloor (ORP) te laag');else if(curOrp>ORP_MAX)m.push('chloor (ORP) te hoog');"
      "sb.textContent=m.length?m.join(' \\u00B7 '):'pH en chloor binnen doel';}"
    "function stat(id,v,mn,mx,dosing){var e=$(id);if(!e)return;"
      "if(dosing){e.textContent='\\u25CF doseert';e.className='st dose';return;}"
      "if(v==null){e.textContent='';e.className='st';return;}"
      "var t,c;if(v<mn){t='\\u25CF te laag';c='crit';}else if(v>mx){t='\\u25CF te hoog';c='crit';}"
      "else{var m=(mx-mn)*.1;if(v<mn+m||v>mx-m){t='\\u25CF bijna grens';c='warn';}else{t='\\u25CF in doel';c='ok';}}"
      "e.textContent=t;e.className='st '+c;}"
    "function renderAlert(){var a=$('psAlert'),b=$('psAlertMsg'),g=$('psAlertBadge');if(!a)return;var m=[];"
      "if(curPh!=null){if(curPh<PH_MIN)m.push('pH aan de lage kant');else if(curPh>PH_MAX)m.push('pH aan de hoge kant');}"
      "if(curOrp!=null){if(curOrp<ORP_MIN)m.push('ORP te laag');else if(curOrp>ORP_MAX)m.push('ORP te hoog');}"
      "if(!m.length){a.classList.remove('show');return;}"
      "g.textContent=m.length;b.innerHTML=m.join(' &middot; ');a.classList.add('show');}"
    "function setPh(v){curPh=v;var e=$('psPhVal');e.textContent=(v==null)?'--':v.toFixed(2);"
      "stat('psPhSt',v,PH_MIN,PH_MAX,phDosing);renderScore();renderAlert();}"
    "function setOrp(v){curOrp=v;var e=$('psOrpVal');e.innerHTML=(v==null)?'--<small> mV</small>':Math.round(v)+'<small> mV</small>';"
      "stat('psOrpSt',v,ORP_MIN,ORP_MAX,orpDosing);renderScore();renderAlert();}"
    "function setTemp(v){var e=$('psTempVal');if(e)e.textContent=(v==null)?'--':v.toFixed(1)+' \\u00B0C';}"
    "function fmt(ml){return ml>=1000?(ml/1000).toFixed(2)+' L':Math.round(ml)+' mL';}"
    "function setToday(){var e=$('psDoseToday');if(e)e.textContent='vandaag: chloor '+fmt(todayOrp)+' \\u00B7 pH\\u2212 '+fmt(todayPh);}"
    "var todayPh=0,todayOrp=0;"
    /* sparklines from /api/history */
    "function spark(id,arr,w,h,area){var el=$(id);if(!el)return;var vals=[],i;"
      "for(i=0;i<arr.length;i++)if(arr[i]!=null)vals.push(arr[i]);"
      "if(vals.length<2){el.innerHTML='';return;}"
      "var mn=Math.min.apply(null,vals),mx=Math.max.apply(null,vals);if(mx-mn<1e-9){mn-=1;mx+=1;}"
      "var pts=[],lx=0,ly=0,n=arr.length;"
      "for(i=0;i<n;i++){if(arr[i]==null)continue;lx=i/(n-1)*w;ly=h-2-(arr[i]-mn)/(mx-mn)*(h-4);pts.push(lx.toFixed(1)+','+ly.toFixed(1));}"
      "var s='';"
      "if(area)s+='<polygon points=\"'+pts[0].split(',')[0]+','+h+' '+pts.join(' ')+' '+lx.toFixed(1)+','+h+'\" fill=\"var(--ps-glow)\"/>';"
      "s+='<polyline points=\"'+pts.join(' ')+'\" fill=\"none\" stroke=\"var(--ps-accent-soft)\" stroke-width=\"1.4\" stroke-linecap=\"round\"/>';"
      "s+='<circle cx=\"'+lx.toFixed(1)+'\" cy=\"'+ly.toFixed(1)+'\" r=\"2.2\" fill=\"var(--ps-accent)\"/>';"
      "el.innerHTML=s;}"
    "function bars(dp,dc){var c=$('psBars');if(!c)return;var mx=1,i;"
      "for(i=0;i<7;i++)mx=Math.max(mx,dp[i]+dc[i]);"
      "var dn=['zo','ma','di','wo','do','vr','za'],dow=new Date().getDay(),h='';"
      "for(i=0;i<7;i++){h+='<div class=\"b\"><i style=\"background:var(--ps-accent);height:'+(dc[i]/mx*40).toFixed(0)+'px\"></i>"
      "<i style=\"background:var(--ps-crit);opacity:.75;height:'+(dp[i]/mx*40).toFixed(0)+'px\"></i>"
      "<span>'+dn[(dow-(6-i)+14)%7]+'</span></div>';}"
      "c.innerHTML=h;}"
    "function loadHist(){fetch('/api/history').then(function(r){return r.json();}).then(function(h){"
      "spark('psPhSpark',h.ph,120,22,false);spark('psOrpSpark',h.orp,120,22,false);spark('psTempSpark',h.temp,280,30,true);"
      "bars(h.dose_ph,h.dose_orp);}).catch(function(){});}"
    "loadHist();setInterval(loadHist,300000);"
    /* live updates */
    "var ws;function wsConn(){ws=new WebSocket('ws://'+location.host+':81/');ws.onclose=function(){setTimeout(wsConn,2000);};"
      "ws.onmessage=function(e){try{var d=JSON.parse(e.data);"
      "if(d.pump_ph){phDosing=d.pump_ph.active;todayPh=d.pump_ph.daily;setToday();}"
      "if(d.pump_orp){orpDosing=d.pump_orp.active;todayOrp=d.pump_orp.daily;setToday();}"
      "if(d.ph!==undefined)setPh(d.ph);"
      "if(d.orp!==undefined)setOrp(d.orp);"
      "if(d.temp!==undefined)setTemp(d.temp);"
      "}catch(_){}};}"
    "wsConn();"
    "fetch('/api/state').then(function(r){return r.json();}).then(function(d){"
      "if(d.ph!=null)setPh(d.ph);if(d.orp!=null)setOrp(d.orp);if(d.temp!=null)setTemp(d.temp);}).catch(function(){});"
  );
  html += F("</script></body></html>");
}

} // namespace io


