#!/usr/bin/env python3
"""Pura mainboard rev 0.4 — nieuwe plaatsing + autoroute vanuit de 0.3-netlist.

Waarom een nieuwe versie: rev 0.3 had een verweven isolatie-eiland (12 primaire
pads binnen AGND, 0.2 mm kloof). Hier loopt de barrière als rechte band over de
volle breedte (y = 103.2..104.8), U5/U6 overbruggen hem, en freerouting krijgt
de band als keepout zodat niets er doorheen kan.

Bron: de netlist + footprints van het 0.3-board (de tsci-export van de tsx).
De KiCad-schematic van tscircuit is GEEN bruikbare netlist (geen footprints,
losse netten) — zie de sessie-notitie.

  python3 build_04.py            # hele flow
  python3 build_04.py --no-route # alleen plaatsing + zones (snel checken)
"""
import json
import os
import re
import shutil
import sys

import build_flow as bf  # run/kpy/balanced + step2_project/step7_route hergebruikt

HERE = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.join(HERE, "pura-mainboard-0805-0.3.kicad")
SRC_PCB = os.path.join(SRC_DIR, "pura-mainboard-0805-0.3.kicad_pcb")
NAME = "pura-mainboard-0.4"
KDIR = os.path.join(HERE, f"{NAME}.kicad")
PCB = os.path.join(KDIR, f"{NAME}.kicad_pcb")
PRO = os.path.join(KDIR, f"{NAME}.kicad_pro")
TMP = os.path.join(KDIR, ".flowtmp")
KICAD_CLI = bf.KICAD_CLI

# isolatieband (KiCad-coords, y omlaag): niets primairs eronder, niets iso erboven
BAND_Y0, BAND_Y1 = 103.2, 104.8
BOARD = (74.95, 74.95, 125.05, 125.05)

# ref: (x, y, rot) — zie docstring van de plaatsingsregels in PLACE_NOTES
PLACE = {
    # --- USB-C / PD (links boven; J1 ligt vast door de case) ---
    "J1": (82.5, 79.34, 180),
    "U1": (83.0, 88.0, 0),
    "R_CFG1": (78.3, 86.5, 0),
    "R_VSNS": (78.3, 88.8, 0),
    "R_VDD": (78.3, 91.1, 0),
    "C_VDD": (78.3, 93.5, 0),
    "J_12V": (77.8, 98.5, 90),
    # --- buck (midden boven) ---
    "U2": (96.5, 81.7, 0),
    "C_IN": (117.0, 79.8, 0),
    "D1": (94.0, 92.3, 0),
    "L1": (101.2, 92.3, 0),
    "C_OUT": (109.0, 86.0, 90),
    "R_LED": (104.5, 98.0, 0),
    "LED1": (104.5, 100.3, 0),
    # --- ESP-kant van de ADuM + B0505S-ingang ---
    "R_SDA2": (92.6, 98.3, 0),
    "R_SCL2": (99.8, 98.3, 180),
    "C_ISO1": (96.2, 98.3, 0),
    "C_DC_IN": (81.5, 101.3, 90),   # west van de U5-pinrij (het lichaam zit oost)
    # --- barrière-overbruggers: pinrij haaks op de band ---
    "U5": (85.0, 103.6, 270),      # pins 1,2 noord (GND/5V), 3,4 zuid; 0.4 omhoog: pin2-pad vrij van de band
    "U6": (98.5, 104.0, 0),        # pins 5-8 noord (ESP-kant), 1-4 zuid (iso)
    # --- motordriver (rechts) ---
    "U4": (113.3, 94.8, 0),
    "C_VCC": (118.0, 86.2, 0),
    "C_VM": (119.3, 91.0, 90),
    "C_VM2": (119.3, 94.6, 90),
    "J_M1": (111.5, 101.6, 0),
    "J_M2": (117.6, 101.6, 0),
    "J_ESP": (122.9, 89.5, 90),
    # --- iso-eiland (onderste strook) ---
    "C_DC_OUT": (92.8, 107.0, 90),
    "R_BLD": (88.0, 112.5, 0),
    "R_LED2": (82.5, 112.5, 0),
    "LED2": (78.5, 112.5, 0),
    "C_ISO2": (98.5, 109.3, 0),
    "C_ADS": (106.5, 110.7, 0),
    "R_SDA": (106.5, 113.0, 0),
    "R_SCL": (106.5, 115.3, 0),
    "U3": (113.0, 112.5, 90),      # AIN-pads oost richting J_PROBE
    "R_ADDR": (113.0, 117.0, 0),
    "J_PROBE": (122.0, 114.0, 90),
    "G***": (88.0, 119.0, 90),     # logo (silk) in de vrije eilandruimte
}
# echte lichamen die de footprint-silk niet tekent: (w, h) lokaal bij rot 0
BODY = {"C_IN": (6.3, 6.3), "C_OUT": (6.3, 6.3),            # elco-bus (D8)
        "J_12V": (5.08, 2.54), "J_M1": (5.08, 2.54), "J_M2": (5.08, 2.54),
        "J_PROBE": (10.16, 2.54), "J_ESP": (27.94, 2.54)}  # header-kunststof
ISO_NETS = {"AGND", ".U5 > .pin4 to .U6 > .VDD1", ".U3 > .SDA to .R_SDA > .pin1",
            ".U3 > .SCL to .R_SCL > .pin1", ".U3 > .ADDR to .R_ADDR > .pin1",
            ".U3 > .AIN0 to .J_PROBE > .PH_PO", ".U3 > .AIN1 to .J_PROBE > .ORP_PO",
            ".R_LED2 > .pin2 to .LED2 > .pin1"}

LABELS = [  # (tekst, x, y, rot, hoogte, dikte)
    ("+", 114.3, 84.4, 0, 1.0, 0.2),       # C_IN pin1 (+12V) zuid
    ("+", 111.6, 89.7, 0, 1.0, 0.2),       # C_OUT pin1 (+5V) zuid
    ("K", 96.5, 89.7, 0, 1.0, 0.2),        # D1 kathode (pin2, oost)
    ("PROBES", 124.2, 114.0, 90, 1.1, 0.2),
    ("12V", 80.6, 98.5, 90, 1.1, 0.2),
    ("pH", 119.2, 112.73, 0, 0.9, 0.18),
    ("ORP", 118.6, 110.19, 0, 0.9, 0.18),
    ("M-pH", 111.5, 99.68, 0, 0.8, 0.16),
    ("M-ORP", 117.6, 99.68, 0, 0.8, 0.16),
    ("ISO", 78.5, 118.5, 0, 1.2, 0.2),
    ("Pura v0.4", 106.0, 121.5, 0, 1.5, 0.25),
]
HIDE_REF = ["J_ESP", "J_PROBE", "J_M1", "J_M2", "J_12V"]


def zone_sexpr(net_name, layers, prio, pts, name=""):
    xy = "".join(f"\n\t\t\t\t(xy {x} {y})" for x, y in pts)
    nm = f'\n\t\t(name "{name}")' if name else ""
    return (f'\t(zone\n\t\t(net "{net_name}")\n\t\t(layers {layers}){nm}'
            f'\n\t\t(hatch edge 0.5)\n\t\t(priority {prio})\n\t\t(connect_pads\n\t\t\t(clearance 0.2)\n\t\t)'
            f'\n\t\t(min_thickness 0.2)\n\t\t(fill yes\n\t\t\t(thermal_gap 0.5)\n\t\t\t(thermal_bridge_width 0.5)'
            f'\n\t\t\t(island_removal_mode 0)\n\t\t)\n\t\t(polygon\n\t\t\t(pts{xy}\n\t\t\t)\n\t\t)\n\t)')


def keepout_sexpr(pts, name="ISO-BARRIER", pour="not_allowed"):
    xy = "".join(f"\n\t\t\t\t(xy {x} {y})" for x, y in pts)
    return (f'\t(zone\n\t\t(net "")\n\t\t(layers "F&B.Cu")\n\t\t(name "{name}")'
            '\n\t\t(hatch edge 0.5)\n\t\t(keepout\n\t\t\t(tracks not_allowed)\n\t\t\t(vias not_allowed)'
            f'\n\t\t\t(pads allowed)\n\t\t\t(copperpour {pour})\n\t\t\t(footprints allowed)\n\t\t)'
            f'\n\t\t(polygon\n\t\t\t(pts{xy}\n\t\t\t)\n\t\t)\n\t)')


def rect(x0, y0, x1, y1):
    return [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]


def step1_project():
    print("== 1. project 0.4 aanmaken uit 0.3 ==")
    if os.path.exists(KDIR):
        shutil.rmtree(KDIR)
    os.makedirs(KDIR)
    for ext in ("kicad_pro", "kicad_prl", "kicad_sch"):
        shutil.copy(os.path.join(SRC_DIR, f"pura-mainboard-0805-0.3.{ext}"),
                    os.path.join(KDIR, f"{NAME}.{ext}"))
    shutil.copy(os.path.join(SRC_DIR, "fp-lib-table"), KDIR)
    os.symlink("../pura-mainboard-0805-0.3.kicad/3dmodels", os.path.join(KDIR, "3dmodels"))  # 13 MB, niet dubbel
    shutil.copy(SRC_PCB, PCB)
    bf.PRO = PRO
    bf.step2_project()


def step2_place():
    print("== 2. strippen + nieuwe plaatsing ==")
    # tekstueel strippen: na b.Remove() verliest de SWIG-runtime zijn typeinfo
    src = open(PCB).read()
    for tok in ("(segment", "(via", "(arc", "(zone", "(gr_text", "(gr_poly", "(group"):
        for i, j in reversed(bf.balanced(src, tok)):
            src = src[:i] + src[j:]
    assert src.count("(") == src.count(")")
    open(PCB, "w").write(src)
    out = bf.kpy(f"""
import pcbnew
b = pcbnew.LoadBoard('{PCB}')
place = {PLACE!r}
seen = set()
for f in b.GetFootprints():
    r = f.GetReference()
    x, y, rot = place[r]
    f.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y)))
    f.SetOrientationDegrees(rot)
    seen.add(r)
missing = set(place) - seen
assert not missing, f'onbekende refs in PLACE: {{missing}}'
pcbnew.SaveBoard('{PCB}', b)
print('geplaatst:', len(seen))
""")
    print("  " + out.strip())
    # zones + keepout als tekst (zelfde instellingen als de 0.3-handzones)
    src = open(PCB).read()
    x0, y0, x1, y1 = BOARD
    E = 0.5
    zones = [
        zone_sexpr("GND", '"F.Cu"', 1, rect(x0, y0, x1, BAND_Y0)),
        zone_sexpr("GND", '"B.Cu"', 2, rect(x0, y0, x1, BAND_Y0)),
        zone_sexpr("AGND", '"F.Cu" "B.Cu"', 3, rect(x0, BAND_Y1, x1, y1)),
        keepout_sexpr(rect(x0 - 5, BAND_Y0, x1 + 5, BAND_Y1)),
        # randstroken: freerouting kent de 0.4 mm edge-clearance niet
        keepout_sexpr(rect(x0 - 5, y0 - 5, x1 + 5, y0 + E), "EDGE-N", "allowed"),
        keepout_sexpr(rect(x0 - 5, y1 - E, x1 + 5, y1 + 5), "EDGE-S", "allowed"),
        keepout_sexpr(rect(x0 - 5, y0 - 5, x0 + E, y1 + 5), "EDGE-W", "allowed"),
        keepout_sexpr(rect(x1 - E, y0 - 5, x1 + 5, y1 + 5), "EDGE-E", "allowed"),
    ]
    k = src.rstrip().rfind(")")
    src = src[:k] + "\n" + "\n".join(zones) + "\n" + src[k:]
    assert src.count("(") == src.count(")")
    open(PCB, "w").write(src)
    print("  3 zones + keepout-band + 4 rand-keepouts geplaatst")


def check_overlap():
    """Grove overlap-check op footprint-bboxes (incl. silk) + pads vs band."""
    out = bf.kpy(f"""
import pcbnew
M = pcbnew.ToMM
b = pcbnew.LoadBoard('{PCB}')
fps = list(b.GetFootprints())
body = {BODY!r}
def mm(r): return (M(r.GetX()), M(r.GetY()), M(r.GetX()+r.GetWidth()), M(r.GetY()+r.GetHeight()))
bb = {{}}
for f in fps:
    x0, y0, x1, y1 = mm(f.GetBoundingBox(False, False))
    if f.GetReference() in body:   # silk-bbox verenigen met het echte lichaam
        w, h = body[f.GetReference()]
        if int(f.GetOrientationDegrees()) % 180 == 90: w, h = h, w
        cx, cy = M(f.GetPosition().x), M(f.GetPosition().y)
        if f.GetReference().startswith('J'):   # header: silk-kader is decoratie, kunststof telt
            x0, y0, x1, y1 = cx-w/2, cy-h/2, cx+w/2, cy+h/2
        else:
            x0, y0 = min(x0, cx-w/2), min(y0, cy-h/2); x1, y1 = max(x1, cx+w/2), max(y1, cy+h/2)
    bb[f.GetReference()] = (x0, y0, x1, y1)
bad = []
refs = sorted(bb)
for i, a in enumerate(refs):
    ax0, ay0, ax1, ay1 = bb[a]
    for c in refs[i+1:]:
        cx0, cy0, cx1, cy1 = bb[c]
        if ax0 < cx1 and cx0 < ax1 and ay0 < cy1 and cy0 < ay1:
            bad.append(f'overlap {{a}} {{bb[a]}} x {{c}} {{bb[c]}}')
iso = {ISO_NETS!r}
for f in fps:
    for p in f.Pads():
        pb = mm(p.GetBoundingBox())   # koper-tot-rand >= 0.4 (DRC) + marge
        if pb[0] < {BOARD[0]}+0.45 or pb[1] < {BOARD[1]}+0.45 or pb[2] > {BOARD[2]}-0.45 or pb[3] > {BOARD[3]}-0.45:
            bad.append(f'pad {{f.GetReference()}}.{{p.GetNumber()}} te dicht bij de boardrand {{pb}}')
        y = M(p.GetPosition().y); n = p.GetNetname()
        if not n: continue
        if n in iso and y < {BAND_Y1}: bad.append(f'iso-pad {{f.GetReference()}}.{{p.GetNumber()}} ({{n[:20]}}) boven de band y={{y:.2f}}')
        if n not in iso and y > {BAND_Y0}: bad.append(f'primaire pad {{f.GetReference()}}.{{p.GetNumber()}} ({{n[:20]}}) onder de band y={{y:.2f}}')
print('OK' if not bad else 'PROBLEMEN:'); [print('  ', x) for x in bad]
""")
    print("  " + out.strip().replace("\n", "\n  "))
    if "PROBLEMEN" in out:
        sys.exit("plaatsing niet schoon")


def step3_dsn():
    print("== 3. DSN (koperzones eruit, keepout blijft, clearance 0.17) ==")
    shutil.rmtree(TMP, ignore_errors=True)
    os.makedirs(TMP)
    tpcb = os.path.join(TMP, f"{NAME}.kicad_pcb")
    tpro = os.path.join(TMP, f"{NAME}.kicad_pro")
    shutil.copy(PRO, tpro)
    src = open(PCB).read()
    for i, j in reversed(bf.balanced(src, "(zone")):
        if "(keepout" not in src[i:j]:
            src = src[:i] + src[j:]
    open(tpcb, "w").write(src)
    d = json.load(open(tpro))
    for c in d["net_settings"]["classes"]:
        c["clearance"] = 0.17
    json.dump(d, open(tpro, "w"), indent=2)
    dsn = os.path.join(TMP, f"{NAME}.dsn")
    bf.kpy(f"import pcbnew; b=pcbnew.LoadBoard('{tpcb}'); "
           f"print('dsn:', pcbnew.ExportSpecctraDSN(b, '{dsn}'))")
    txt = open(dsn).read()
    assert "keepout" in txt, "keepout-band zit niet in de DSN — freerouting zou de barrière kruisen"
    print(f"  keepouts in DSN: {txt.count('(keepout')}")
    return dsn


def step4_route(dsn):
    bf.TMP, bf.KDIR, bf.NAME = TMP, KDIR, NAME
    return bf.step7_route(dsn)


def step5_import(ses):
    print("== 5. ses importeren + opschonen ==")
    out = bf.kpy(f"""
import pcbnew
FM = pcbnew.FromMM
b = pcbnew.LoadBoard('{PCB}')
print('import:', pcbnew.ImportSpecctraSES(b, '{ses}'))
n = 0
for t in list(b.GetTracks()):
    if t.GetClass() == 'PCB_TRACK' and t.GetLength() < FM(0.02):
        b.Remove(t); n += 1
print('micro-stukjes weg:', n)
pcbnew.SaveBoard('{PCB}', b)
""")
    print("  " + out.strip().replace("\n", "\n  "))


def step6_fill():
    print("== 6. zone-fill + AGND-stitching ==")
    out = bf.kpy(f"""
import pcbnew
FM = pcbnew.FromMM
b = pcbnew.LoadBoard('{PCB}')
island = [z for z in b.Zones() if z.GetNetname() == 'AGND'][0]
agnd = b.GetNetsByName()['AGND']
existing = [(pcbnew.ToMM(t.GetPosition().x), pcbnew.ToMM(t.GetPosition().y))
            for t in b.GetTracks() if t.GetClass() == 'PCB_VIA']
pcbnew.ZONE_FILLER(b).Fill(b.Zones())
bb = island.GetBoundingBox()
added = 0; STEP = FM(3.5)
x = bb.GetX() + STEP
while x < bb.GetX() + bb.GetWidth():
    y = bb.GetY() + STEP
    while y < bb.GetY() + bb.GetHeight():
        R = FM(0.75); D = int(R*0.7071)
        pts = [pcbnew.VECTOR2I(x+dx, y+dy) for dx, dy in
               [(0,0),(R,0),(-R,0),(0,R),(0,-R),(D,D),(D,-D),(-D,D),(-D,-D)]]
        mx, my = pcbnew.ToMM(x), pcbnew.ToMM(y)
        dup = any(abs(mx-ex) < 0.8 and abs(my-ey) < 0.8 for ex, ey in existing)
        if not dup and all(island.HitTestFilledArea(l, p, 0) for l in (pcbnew.F_Cu, pcbnew.B_Cu) for p in pts):
            v = pcbnew.PCB_VIA(b); v.SetPosition(pcbnew.VECTOR2I(x, y))
            v.SetWidth(FM(0.6)); v.SetDrill(FM(0.3)); v.SetNet(agnd); v.SetLocked(True)
            b.Add(v); added += 1; existing.append((mx, my))
        y += STEP
    x += STEP
print('stitch-vias:', added)
pcbnew.ZONE_FILLER(b).Fill(b.Zones())
pcbnew.SaveBoard('{PCB}', b)
""")
    print("  " + out.strip().replace("\n", "\n  "))


def step7_silk():
    print("== 7. silk-labels ==")
    src = open(PCB).read()
    for ref in HIDE_REF:
        k = src.find(f'(property "Reference" "{ref}"')
        end = src.find("(effects", k)
        if k >= 0 and "(hide yes)" not in src[k:end]:
            src = src[:end] + "(hide yes)\n\t\t\t" + src[end:]
    out = []
    for txt, x, y, rot, h, th in LABELS:
        out.append(f'\t(gr_text "{txt}"\n\t\t(at {x} {y} {rot})\n\t\t(layer "F.SilkS")'
                   f'\n\t\t(effects\n\t\t\t(font\n\t\t\t\t(size {h} {h})\n\t\t\t\t(thickness {th})'
                   f'\n\t\t\t\t(bold yes)\n\t\t\t)\n\t\t)\n\t)')
    k = src.rstrip().rfind(")")
    src = src[:k] + "\n" + "\n".join(out) + "\n" + src[k:]
    assert src.count("(") == src.count(")")
    open(PCB, "w").write(src)


def step8_check():
    print("== 8. barrière-check + DRC ==")
    out = bf.kpy(f"""
import pcbnew
M = pcbnew.ToMM
b = pcbnew.LoadBoard('{PCB}')
iso = {ISO_NETS!r}
bad = 0
for t in b.GetTracks():
    ys = [M(t.GetStart().y), M(t.GetEnd().y)] if t.GetClass() == 'PCB_TRACK' else [M(t.GetPosition().y)]
    n = t.GetNetname()
    if n in iso and min(ys) < {BAND_Y1} - 0.01: bad += 1; print('  iso-koper boven band:', n[:25], ys)
    if n not in iso and max(ys) > {BAND_Y0} + 0.01: bad += 1; print('  primair koper onder band:', n[:25], ys)
    if min(ys) < {BAND_Y0} - 0.01 and max(ys) > {BAND_Y1} + 0.01: bad += 1; print('  kruist band:', n[:25], ys)
print('barriere-schendingen:', bad)
print('tracks:', sum(1 for t in b.GetTracks() if t.GetClass()=='PCB_TRACK'),
      'vias:', sum(1 for t in b.GetTracks() if t.GetClass()=='PCB_VIA'))
""")
    print("  " + out.strip().replace("\n", "\n  "))
    bf.PRO = PRO
    bf.step2_project()
    rep = os.path.join(HERE, "drc04.txt")
    bf.run([KICAD_CLI, "pcb", "drc", "-o", rep, PCB])
    txt = open(rep).read()
    unconnected = int(re.search(r"Found (\d+) unconnected", txt).group(1))
    cats = {}
    for c in re.findall(r"\[([a-z_]+)\]", txt):
        cats[c] = cats.get(c, 0) + 1
    print(f"  unconnected: {unconnected}; categorien: {cats}")
    hard = ["clearance", "shorting_items", "copper_edge_clearance", "track_width",
            "unconnected_items", "via_dangling", "items_not_allowed", "courtyards_overlap"]
    problems = {c: cats[c] for c in hard if cats.get(c)}
    if unconnected or problems:
        print(f"  DRC NIET schoon: {problems or unconnected} — zie {rep}")
        return False
    print("  DRC SCHOON (rest is silk/tekst-cosmetica)")
    return True


def render():
    for side in ("top", "bottom"):
        png = os.path.join(HERE, f"render04-{side}.png")
        bf.run([KICAD_CLI, "pcb", "render", "-o", png, "--width", "1400", "--height", "1400",
                "--side", side, "--background", "opaque", "--quality", "basic", PCB])
        print("  render:", png)


def main():
    step1_project()
    step2_place()
    check_overlap()
    if "--no-route" in sys.argv:
        render()
        return
    dsn = step3_dsn()
    ses = step4_route(dsn)
    step5_import(ses)
    step6_fill()
    step7_silk()
    ok = step8_check()
    render()
    shutil.rmtree(TMP, ignore_errors=True)
    print(f"\n{'KLAAR' if ok else 'KLAAR MET DRC-FOUTEN'}: {PCB}")


if __name__ == "__main__":
    main()
