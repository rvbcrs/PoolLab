#!/usr/bin/env python3
"""Zoek Pura-componenten op AliExpress en reken de goedkoopste all-in prijs per stuk uit.

Model (jouw echte LM2596-voorbeeld):
  Choice:      10 stuks voor 4,89  -> gratis verzending, invoer inbegrepen -> 0,489/stuk
  niet-Choice: 30 stuks voor 4,38  -> +4,54 verzending +3,00 invoer = 11,92 -> 0,397/stuk (wint!)

  effectief_per_stuk = (packprijs + (0 als Choice anders VERZENDING+INVOER)) / aantal_in_pack

LET OP: de Affiliate-API geeft GEEN verzendkosten en GEEN Choice-vlag terug
(alleen de prijs). Daarom toont de tool per listing BEIDE prijzen per stuk:
  - "€/st Choice"   = prijs / aantal            (Choice = gratis verzending)
  - "€/st regulier" = (prijs + verzending + invoer) / aantal
Op de site zie je zelf of een listing Choice is (gratis verzending = Choice),
en kies je de bijbehorende kolom. Verzending + invoer zijn instelbare aannames
(--ship-noncchoice / --import-fee), want de API kent ze niet.

Credentials via env (NIET in de repo zetten):
  export ALI_KEY=538774
  export ALI_SECRET=xxxxxxxx
  export ALI_TRACKING=default        # jouw tracking-id uit portals.aliexpress.com

Gebruik:
  pip install python-aliexpress-api
  python3 ali_price.py                       # zoekt de default chip-lijst
  python3 ali_price.py "ADS1115" "TB6612FNG" # eigen zoektermen
  python3 ali_price.py --surcharge 3 --ship-to NL --top 5
"""
import argparse
import os
import re
import sys
import time

# Volledige Pura-BOM (gegroepeerd op waarde) — zelfde lijst als de web-UI
DEFAULT_TERMS = [
    "CH224K", "LM2596S-5.0", "ADS1115IDGSR", "TB6612FNG", "ADUM1250",
    "Type-C 16Pin SMD connector", "SS34 SMA diode", "33uH shielded power inductor",
    "0805 LED green", "0805 LED blue",
    "0805 5.1K resistor", "0805 4.7K resistor", "0805 2.2K resistor", "0805 0R resistor",
    "0805 24K resistor",
    "0805 100nF capacitor", "1210 100uF 25V capacitor", "220uF 16V SMD electrolytic capacitor",
    "0805 10uF capacitor",
]

# Pack-aanduidingen: "50pcs", "10-100PCS", "10PCS~500PCS", "1/5/10 pcs", "lot of 20".
# We pakken het EERSTE getal van elke pack-expressie (de laag-kant), want de
# getoonde prijs hoort bij de kleinste/goedkoopste SKU-variant.
_PACK_RE = re.compile(
    r"(?<![A-Za-z0-9])(\d+)((?:\s*[-~/]\s*\d+)*)\s*(?:pcs?|pieces?|stuks?|stk|lots?|packs?)\b"
    r"|(?:lot\s+of|pack\s+of)\s*(\d+)",
    re.IGNORECASE,
)


def parse_pack(title: str):
    """(aantal, approx) — kleinste pack-aantal; approx=True bij een range (10-100PCS).

    ponytail: titel-parsing is de beste gok zonder SKU-data; ceiling = range-listings
    waarvan de prijs bij de kleinste variant hoort. Upgrade zodra de 'SKU Dimension API'
    permissie actief is: haal per product_id de echte prijs-per-variant op (methode
    aliexpress.affiliate.product.sku.*) en vervang deze schatting door de exacte
    (aantal, prijs)-paren. Dan is 'approx' niet meer nodig.
    """
    lows, approx = [], False
    for m in _PACK_RE.finditer(title or ""):
        if m.group(3):  # "lot of 20"
            lows.append(int(m.group(3)))
            continue
        n = int(m.group(1))
        if 1 <= n <= 5000:
            lows.append(n)
        if m.group(2):  # had een "-", "~" of "/" erin -> range/keuze
            approx = True
    if not lows:
        return 1, False
    if len(set(lows)) > 1:
        approx = True
    return min(lows), approx


def core_token(term: str) -> str:
    """Kernonderdeel-code uit de zoekterm, bv 'LM2596S-5.0' -> 'LM2596S', 'ADS1115 module' -> 'ADS1115'."""
    m = re.match(r"[A-Za-z]+[0-9]+[A-Za-z0-9]*", term.strip())
    return (m.group(0) if m else term.strip().split()[0]).lower()


def wildcardize(term: str) -> str:
    """Prefix-match op onderdeelnummers: 'ADUM1250' -> 'ADUM1250*'.

    Kale term 'ADUM1250' matcht exact en faalt (titels schrijven 'ADUM1250ARZ');
    een '*' erachter maakt er een prefix-match van. Common parts blijven gelijk.
    """
    out = []
    for tok in term.split():
        out.append(tok + "*" if any(c.isdigit() for c in tok) and not tok.endswith("*") else tok)
    return " ".join(out)


def effective_unit(price: float, qty: int, choice: bool, extra: float) -> float:
    """extra = verzending + invoer, alleen voor niet-Choice."""
    return (price + (0.0 if choice else extra)) / max(qty, 1)


def demo() -> None:
    """Self-check met jouw echte LM2596-cijfers."""
    assert parse_pack("10PCS ADS1115 Module") == (10, False)
    assert parse_pack("ADS1115 lot of 50 pieces") == (50, False)
    assert parse_pack("TB6612FNG Motor Driver") == (1, False)
    # range: prijs hoort bij de kleinste pack -> pak 10, markeer approx
    assert parse_pack("10PCS~500PCS/LOT CH224K ESSOP10") == (10, True)
    assert parse_pack("1-100PCS TB6612FNG") == (1, True)
    assert parse_pack("10-20-50PCS LM2596S-5.0") == (10, True)
    assert wildcardize("ADUM1250") == "ADUM1250*"
    assert wildcardize("ADS1115 module") == "ADS1115* module"  # alleen het onderdeelnummer
    # Choice: 10 stuks voor 4,89, gratis verzending -> 0,489/stuk
    a = effective_unit(4.89, 10, choice=True, extra=0.0)
    # Niet-Choice: 30 stuks voor 4,38 + 4,54 verzending + 3,00 invoer -> 11,92/30 = 0,397/stuk
    b = effective_unit(4.38, 30, choice=False, extra=4.54 + 3.00)
    assert abs(a - 0.489) < 1e-9 and abs(b - 11.92 / 30) < 1e-9
    assert b < a  # niet-Choice pack van 30 wint, ondanks verzending + invoer
    print(f"demo OK  (choice {a:.3f}/stuk  vs  niet-choice {b:.3f}/stuk -> niet-choice wint)")


def get_sku_variants(pid, ship_to="NL"):
    """Echte prijs per pack-variant (5/30/50pcs) via aliexpress.affiliate.product.sku.detail.get.

    Vereist de 'SKU Dimension API' permissie op de app. Prijzen zijn incl. BTW.
    Geeft [{label, qty, price, sku, url}] — qty geparsed uit het variant-label.
    """
    from aliexpress_api.skd import setDefaultAppInfo
    from aliexpress_api.skd.api.base import RestApi
    setDefaultAppInfo(os.environ["ALI_KEY"], os.environ["ALI_SECRET"])

    class R(RestApi):
        def __init__(self):
            RestApi.__init__(self, "api-sg.aliexpress.com", 80)
            self.product_id = str(pid)
            self.ship_to_country = ship_to
            self.target_currency = "EUR"
            self.target_language = "EN"
            self.tracking_id = os.environ.get("ALI_TRACKING", "default")

        def getapiname(self):
            return "aliexpress.affiliate.product.sku.detail.get"

    resp = R().getResponse()
    root = resp.get("aliexpress_affiliate_product_sku_detail_get_response", {})
    res = (root.get("result") or {}).get("result") or {}
    lst = (res.get("ae_item_sku_info") or {}).get("traffic_sku_info_list") or []
    if isinstance(lst, dict):  # TOP wikkelt lijsten soms in een extra dict
        lst = lst.get("traffic_sku_info") or lst.get("string") or []
    out = []
    for v in lst:
        label = v.get("color") or ""
        qty, _ = parse_pack(label)
        try:
            price = float(v.get("sale_price_with_tax") or v.get("price_with_tax") or 0)
        except (TypeError, ValueError):
            continue
        if price <= 0:
            continue
        out.append({"label": label, "qty": qty, "price": round(price, 2),
                    "sku": v.get("sku_id", 0), "url": v.get("link", "")})
    return out


def get_shipping(pid, sku, price, ship_to="NL", tax_rate="0.21"):
    """Officiële verzendkosten via aliexpress.affiliate.product.shipping.get (Advanced API).

    Geeft {fee, min_days, max_days} in EUR — exact wat de site toont.
    """
    from aliexpress_api.skd import setDefaultAppInfo
    from aliexpress_api.skd.api.base import RestApi
    setDefaultAppInfo(os.environ["ALI_KEY"], os.environ["ALI_SECRET"])

    class R(RestApi):
        def __init__(self):
            RestApi.__init__(self, "api-sg.aliexpress.com", 80)
            self.product_id = str(pid)
            self.sku_id = str(sku)
            self.ship_to_country = ship_to
            self.target_currency = "EUR"
            self.target_language = "EN"
            self.target_sale_price = str(price)
            self.tax_rate = str(tax_rate)
            self.tracking_id = os.environ.get("ALI_TRACKING", "default")

        def getapiname(self):
            return "aliexpress.affiliate.product.shipping.get"

    resp = R().getResponse()
    res = ((resp.get("aliexpress_affiliate_product_shipping_get_response", {})
            .get("resp_result") or {}).get("result")) or {}
    if "shipping_fee" not in res:
        raise RuntimeError("geen verzendinfo voor deze sku")
    return {"fee": float(res["shipping_fee"]),
            "min_days": int(res.get("min_delivery_days") or 0),
            "max_days": int(res.get("max_delivery_days") or 0)}


def make_api():
    """AliexpressApi uit env (ALI_KEY/ALI_SECRET/ALI_TRACKING). Raise met duidelijke tekst."""
    key = os.environ.get("ALI_KEY")
    secret = os.environ.get("ALI_SECRET")
    tracking = os.environ.get("ALI_TRACKING", "default")
    if not key or not secret:
        raise RuntimeError("Zet ALI_KEY en ALI_SECRET als env vars.")
    from aliexpress_api import AliexpressApi, models  # lazy: alleen nodig voor live calls
    return AliexpressApi(key, secret, models.Language.EN, models.Currency.EUR, tracking)


# Bij een kale-chip-zoekterm horen geen modules/breakout-boards (niet alleen het chipje)
_MODULE_WORDS = ("module", "board", "shield", "breakout", "development", " kit", "hat", "adapter")


def search_component(api, term, ship_to="NL", page_size=40, no_wildcard=False, no_filter=False):
    """Zoek één component; geeft (rows, dropped) met {qty,approx,price,title,url,shop}.

    LET OP: de affiliate-API zegt NIET of een listing Choice is (geverifieerd).
    In de praktijk is het gros van deze listings Choice (gratis verzending, prijs
    incl. BTW), dus de UI toont de packprijs als de werkelijke prijs.
    """
    query = term if no_wildcard else wildcardize(term)
    resp = api.get_products(keywords=query, ship_to_country=ship_to, page_size=page_size)
    products = resp.products if hasattr(resp, "products") else resp
    core = core_token(term)
    wants_module = any(w.strip() in term.lower() for w in _MODULE_WORDS)
    rows, dropped = [], 0
    for p in products or []:
        title = getattr(p, "product_title", "") or ""
        low = title.lower()
        if not no_filter and core not in low:
            dropped += 1
            continue
        if not wants_module and any(w in low for w in _MODULE_WORDS):  # kale chip gevraagd
            dropped += 1
            continue
        price = price_of(p)
        if price == float("inf"):
            continue
        qty, approx = parse_pack(title)
        rows.append({
            "qty": qty, "approx": approx, "price": round(price, 2),
            "title": title, "url": getattr(p, "product_detail_url", "") or "",
            "shop": getattr(p, "shop_name", "") or "",
            "pid": getattr(p, "product_id", 0),   # + sku: nodig voor RapidAPI freight-lookup
            "sku": getattr(p, "sku_id", 0),
            "core": core,  # onderdeel-token, om mix-listing-varianten te filteren
        })
    return rows, dropped


def price_of(product) -> float:
    for attr in ("target_sale_price", "sale_price", "target_original_price", "original_price"):
        v = getattr(product, attr, None)
        if v:
            try:
                return float(v)
            except (TypeError, ValueError):
                pass
    return float("inf")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("terms", nargs="*", default=None, help="zoektermen (default: Pura chip-lijst)")
    ap.add_argument("--ship-noncchoice", type=float, default=4.54, help="aanname verzendkosten niet-Choice (EUR)")
    ap.add_argument("--import-fee", type=float, default=3.0, help="aanname invoerkosten niet-Choice (EUR)")
    ap.add_argument("--ship-to", default="NL", help="land van levering, bv NL")
    ap.add_argument("--currency", default="EUR")
    ap.add_argument("--top", type=int, default=5, help="aantal beste treffers per component")
    ap.add_argument("--page-size", type=int, default=40, help="hoeveel listings ophalen per component")
    ap.add_argument("--no-filter", action="store_true", help="titel-filter uit (toont ook niet-matchende ruis)")
    ap.add_argument("--no-wildcard", action="store_true", help="geen '*' achter onderdeelnummers (exacte match)")
    ap.add_argument("--raw", action="store_true", help="dump alle velden van de eerste treffer (debug)")
    ap.add_argument("--self-test", action="store_true", help="alleen de rekenregel checken, geen API")
    args = ap.parse_args()

    if args.self_test:
        demo()
        return 0

    try:
        api = make_api()
    except RuntimeError as e:
        print(e, file=sys.stderr)
        return 2
    except ImportError:
        print("Installeer eerst: pip install python-aliexpress-api", file=sys.stderr)
        return 2

    extra = args.ship_noncchoice + args.import_fee
    terms = args.terms or DEFAULT_TERMS

    if args.raw:  # dump alle velden van de eerste treffer (debug, bv voor SKU-API)
        import pprint
        resp = api.get_products(keywords=wildcardize(terms[0]), ship_to_country=args.ship_to, page_size=5)
        prods = resp.products if hasattr(resp, "products") else resp
        pprint.pprint(vars(prods[0]) if prods else "geen resultaten")
        return 0

    for i, term in enumerate(terms):
        if i:
            time.sleep(1.2)  # test-app rate limit is ~1 req/s
        print(f"\n=== {term} ===")
        try:
            rows, dropped = search_component(api, term, args.ship_to, args.page_size,
                                             args.no_wildcard, args.no_filter)
        except Exception as e:  # noqa: BLE001 - de API geeft brede fouten (permissie/rate/tracking)
            print(f"  API-fout: {e}")
            continue
        if not rows:
            print(f"  geen echte '{core_token(term)}'-treffers")
            continue
        for r in rows:  # bereken prijs per stuk met de gekozen aanname
            r["choice_u"] = r["price"] / r["qty"]
            r["regular_u"] = (r["price"] + extra) / r["qty"]
        rows.sort(key=lambda r: min(r["choice_u"], r["regular_u"]))
        print(f"  €/st Choice  €/st regul{'':>2} qty {'prijs':>7}  titel   ({dropped} ruis weg)")
        for r in rows[: args.top]:
            qtxt = f"{r['qty']}{'~' if r['approx'] else ''}"
            print(f"  {r['choice_u']:>10.3f} {r['regular_u']:>10.3f} {qtxt:>5} {r['price']:>7.2f}  {r['title'][:48]}")
            print(f"       {r['url']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
