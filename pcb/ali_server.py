#!/usr/bin/env python3
"""Lokale web-UI voor de AliExpress component-prijsvergelijker.

  export ALI_KEY=538774 ALI_SECRET=… ALI_TRACKING=morrison
  pip install python-aliexpress-api
  python3 ali_server.py            # open http://localhost:8737

Hergebruikt de zoeklogica uit ali_price.py. De prijs-per-stuk berekening
(Choice vs regulier, aantallen) gebeurt in de browser, zodat tunen geen
nieuwe API-call kost. Alleen /api/search doet een live AliExpress-call.
"""
import json
import os
import socket
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

import ali_price

PORT = int(os.environ.get("ALI_PORT", "8737"))
HOST = os.environ.get("ALI_HOST", "0.0.0.0")  # 0.0.0.0 = ook bereikbaar vanaf je telefoon op hetzelfde wifi
# Echte verzendkosten via de officiële affiliate shipping-API (Advanced permissie),
# met persistente cache. Cache-key: pid:sku:land.
SHIP_CACHE_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".shipcache.json")
try:
    with open(SHIP_CACHE_FILE) as f:
        _ship_cache = json.load(f)
except (FileNotFoundError, ValueError):
    _ship_cache = {}

# Variant-prijzen (SKU Dimension API) — persistent gecachet, prijzen wijzigen zelden
SKU_CACHE_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".skucache.json")
try:
    with open(SKU_CACHE_FILE) as f:
        _sku_cache = json.load(f)
except (FileNotFoundError, ValueError):
    _sku_cache = {}


def fetch_skus(pid, ship_to="NL"):
    key = f"{pid}:{ship_to}"
    if key in _sku_cache:
        return _sku_cache[key]
    try:
        variants = ali_price.get_sku_variants(pid, ship_to)
    except Exception as e:  # noqa: BLE001
        return {"error": str(e)[:200]}  # niet persistent cachen (kan transient zijn)
    out = {"variants": variants}
    _sku_cache[key] = out
    with open(SKU_CACHE_FILE, "w") as f:
        json.dump(_sku_cache, f)
    return out


def fetch_shipping(pid, sku, price, ship_to="NL"):
    key = f"{pid}:{sku}:{ship_to}"
    if key in _ship_cache:
        return _ship_cache[key]
    try:
        out = ali_price.get_shipping(pid, sku, price, ship_to)
    except Exception as e:  # noqa: BLE001
        out = {"error": str(e)[:200]}
        _ship_cache[key] = out  # in-memory: niet hameren binnen deze sessie
        return out
    _ship_cache[key] = out
    with open(SHIP_CACHE_FILE, "w") as f:
        json.dump(_ship_cache, f)  # alleen successen persistent; fouten mogen later opnieuw
    return out


def lan_ip():
    """Het LAN-IP van deze Mac (voor de URL op je telefoon)."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))  # verstuurt niets, leest alleen het lokale adres
        return s.getsockname()[0]
    except OSError:
        return "127.0.0.1"
    finally:
        s.close()
HTML = os.path.join(os.path.dirname(os.path.abspath(__file__)), "ali_ui.html")

_api = None
_lock = threading.Lock()       # serialiseer upstream-calls (test-app: ~1 req/s)
_last_call = [0.0]


def get_api():
    global _api
    if _api is None:
        _api = ali_price.make_api()
    return _api


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *a):  # stil
        pass

    def _search_with_retry(self, term, ship, n, tries=3):
        for attempt in range(tries):
            dt = time.time() - _last_call[0]
            if dt < 1.8:
                time.sleep(1.8 - dt)
            try:
                res = ali_price.search_component(get_api(), term, ship_to=ship, page_size=n)
                _last_call[0] = time.time()
                return res
            except Exception as e:  # noqa: BLE001
                _last_call[0] = time.time()
                if "frequency" in str(e).lower() and attempt < tries - 1:
                    time.sleep(2.5)  # API-ban uitzitten en opnieuw
                    continue
                raise

    def _send(self, code, body, ctype="application/json; charset=utf-8"):
        data = body.encode("utf-8") if isinstance(body, str) else body
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        u = urlparse(self.path)
        if u.path == "/":
            try:
                with open(HTML, "r", encoding="utf-8") as f:
                    self._send(200, f.read(), "text/html; charset=utf-8")
            except FileNotFoundError:
                self._send(500, "ali_ui.html ontbreekt")
            return
        if u.path == "/api/search":
            q = parse_qs(u.query)
            term = (q.get("term", [""])[0]).strip()
            if not term:
                self._send(400, json.dumps({"error": "term ontbreekt"}))
                return
            try:
                with _lock:  # rate-limit: spreid upstream-calls, retry bij ban
                    rows, dropped = self._search_with_retry(
                        term, q.get("ship", ["NL"])[0], int(q.get("n", ["40"])[0]))
                self._send(200, json.dumps({
                    "term": term, "core": ali_price.core_token(term),
                    "rows": rows, "dropped": dropped,
                }))
            except Exception as e:  # noqa: BLE001
                self._send(200, json.dumps({"term": term, "rows": [], "error": str(e)}))
            return
        if u.path == "/api/keystatus":
            self._send(200, json.dumps({"set": True}))  # legacy: shipping is nu first-party
            return
        if u.path == "/api/skus":
            q = parse_qs(u.query)
            pid = (q.get("pid", [""])[0]).strip()
            ship = (q.get("ship", ["NL"])[0]).strip()[:2]
            if not pid.isdigit():
                self._send(400, json.dumps({"error": "pid ontbreekt"}))
                return
            with _lock:  # zelfde app-rate-limit als zoeken (~1 req/s)
                key = f"{pid}:{ship}"
                if key not in _sku_cache:
                    dt = time.time() - _last_call[0]
                    if dt < 1.8:
                        time.sleep(1.8 - dt)
                    _last_call[0] = time.time()
                self._send(200, json.dumps(fetch_skus(pid, ship)))
            return
        if u.path == "/api/shipping":
            q = parse_qs(u.query)
            pid = (q.get("pid", [""])[0]).strip()
            sku = (q.get("sku", [""])[0]).strip()
            ship = (q.get("ship", ["NL"])[0]).strip()[:2]
            try:
                price = float(q.get("price", ["1"])[0])
            except ValueError:
                price = 1.0
            if not pid.isdigit() or not sku.isdigit():
                self._send(400, json.dumps({"error": "pid/sku ontbreekt"}))
                return
            with _lock:  # officiële API deelt de app-rate-limit (~1 req/s)
                if f"{pid}:{sku}:{ship}" not in _ship_cache:
                    dt = time.time() - _last_call[0]
                    if dt < 1.8:
                        time.sleep(1.8 - dt)
                    _last_call[0] = time.time()
                self._send(200, json.dumps(fetch_shipping(pid, sku, price, ship)))
            return
        self._send(404, json.dumps({"error": "not found"}))


def main():
    try:
        get_api()
    except Exception as e:  # noqa: BLE001
        print(f"Kan API niet starten: {e}")
        print("Zet ALI_KEY / ALI_SECRET / ALI_TRACKING en 'pip install python-aliexpress-api'.")
        return 2
    srv = ThreadingHTTPServer((HOST, PORT), Handler)
    print(f"Component Bench draait — Ctrl-C stopt")
    print(f"  op deze Mac:      http://localhost:{PORT}")
    if HOST == "0.0.0.0":
        print(f"  op je telefoon:   http://{lan_ip()}:{PORT}   (zelfde wifi)")
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        print("\nGestopt.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
