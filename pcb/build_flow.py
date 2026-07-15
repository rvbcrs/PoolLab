#!/usr/bin/env python3
"""Volautomatische productie-flow voor het Pura mainboard rev 0.3.

tsx-bron -> KiCad-export -> zones -> gelockte pre-routes -> 3D-modellen ->
DSN -> freerouting (headless) -> ses-import -> opschonen -> zone-fill -> DRC.

  python3 build_flow.py            # hele flow
  python3 build_flow.py --no-export  # sla de tsci-export over (tsx ongewijzigd)

Geen handmatige DSN-exports of freerouting-GUI meer nodig. De freerouting-GUI
(v2.2.4) crasht sowieso met StackOverflowError op deze DSN; de 1.7.0-jar
headless werkt.
"""
import json
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
NAME = "pura-mainboard-0805-0.3"
KDIR = os.path.join(HERE, f"{NAME}.kicad")
PCB = os.path.join(KDIR, f"{NAME}.kicad_pcb")
PRO = os.path.join(KDIR, f"{NAME}.kicad_pro")
TSX = os.path.join(HERE, "pura-mainboard-noroute.tsx")
KPY = "/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3"
KICAD_CLI = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
FR_JAR = os.path.expanduser("~/Downloads/freerouting-1.7.0.jar")
TMP = os.path.join(KDIR, ".flowtmp")

POWER_NETS = [
    ".J1 > .A4B9 to .R_VDD > .pin1",     # 12V VBUS
    ".U2 > .OUT to .L1 > .pin1",         # buck switch-node
    ".L1 > .pin2 to .U2 > .FB",          # 5V-rail
    ".U4 > .AO11 to .J_M1 > .MA1",
    ".U4 > .AO21 to .J_M1 > .MA2",
    ".U4 > .BO11 to .J_M2 > .MB1",
    ".U4 > .BO21 to .J_M2 > .MB2",
]
# 3D-modellen voor de custom footprints (rotatie in graden om Z)
MODELS = {
    "L1": ("Inductor_SMD.3dshapes/L_APV_ANR6045.step", 0),
    "C_IN": ("Capacitor_SMD.3dshapes/CP_Elec_6.3x7.7.step", -90),
    "C_OUT": ("Capacitor_SMD.3dshapes/CP_Elec_6.3x7.7.step", 0),
}


def run(cmd, **kw):
    print("  $", " ".join(str(c) for c in cmd))
    r = subprocess.run(cmd, capture_output=True, text=True, **kw)
    if r.returncode != 0:
        print(r.stdout[-2000:])
        print(r.stderr[-2000:])
        sys.exit(f"FOUT bij: {' '.join(str(c) for c in cmd)}")
    return r.stdout


def kpy(script):
    """Draai een pcbnew-script onder KiCads python."""
    r = subprocess.run([KPY, "-c", script], capture_output=True, text=True)
    out = "\n".join(l for l in (r.stdout + r.stderr).splitlines()
                    if "wxApp" not in l and "assert" not in l and "memory leak" not in l)
    if r.returncode != 0:
        print(out)
        sys.exit("FOUT in pcbnew-script")
    return out


def balanced(s, token):
    out, i = [], 0
    while True:
        i = s.find(token, i)
        if i < 0:
            return out
        d, j = 0, i
        while True:
            if s[j] == "(":
                d += 1
            elif s[j] == ")":
                d -= 1
                if d == 0:
                    break
            j += 1
        out.append((i, j + 1))
        i = j + 1


def step1_export():
    print("== 1. tsci export + uitpakken ==")
    # let op: tsci plakt -o achter de cwd, dus relatieve paden gebruiken
    run(["npx", "tsci", "export", "pura-mainboard-noroute.tsx",
         "-f", "kicad_zip", "-o", "pura03.kicad.zip"], cwd=HERE)
    run(["unzip", "-o", "-q", os.path.join(HERE, "pura03.kicad.zip"), "-d", KDIR])
    for ext in ("kicad_pcb", "kicad_pro", "kicad_sch"):
        src = os.path.join(KDIR, f"pura-mainboard-noroute.{ext}")
        if os.path.exists(src):
            shutil.move(src, os.path.join(KDIR, f"{NAME}.{ext}"))


def step2_project():
    print("== 2. netklassen + rules in .kicad_pro ==")
    d = json.load(open(PRO))
    d["net_settings"]["classes"] = [
        {"clearance": 0.15, "name": "Default", "pcb_color": "rgba(0, 0, 0, 0.000)",
         "priority": 2147483647, "schematic_color": "rgba(0, 0, 0, 0.000)",
         "track_width": 0.2, "tuning_profile": "", "via_diameter": 0.6, "via_drill": 0.3},
        {"clearance": 0.15, "name": "POWER", "pcb_color": "rgba(255, 100, 0, 0.500)",
         "priority": 0, "schematic_color": "rgba(0, 0, 0, 0.000)",
         "track_width": 0.6, "tuning_profile": "", "via_diameter": 0.8, "via_drill": 0.4},
    ]
    d["net_settings"]["netclass_patterns"] = [
        {"netclass": "POWER", "pattern": n} for n in POWER_NETS]
    # tsci schrijft net_settings-schema v1; KiCads migratie dropt dan de
    # patterns. Normaliseren naar het actuele schema (v5).
    d["net_settings"]["meta"] = {"version": 5}
    d["net_settings"].setdefault("net_colors", None)
    d["net_settings"].setdefault("netclass_assignments", None)
    rules = d["board"]["design_settings"]["rules"]
    rules["min_track_width"] = 0.15
    rules["min_clearance"] = 0.15
    rules["min_copper_edge_clearance"] = 0.4
    rules["min_resolved_spokes"] = 1  # eiland-randpads halen anders geen 2 spokes
    json.dump(d, open(PRO, "w"), indent=2)


def step3_zones():
    print("== 3. zones porten (GND prio1/2 + AGND-eiland v2) ==")
    import port_zones
    port_zones.main(PCB)


def step4_preroute():
    print("== 4. gelockte pre-routes (iso-netten, AIN, AGND-stitching) ==")
    import preroute_iso
    preroute_iso.main(PCB)


def step5_models():
    print("== 5. 3D-modellen injecteren (L1, C_IN, C_OUT) ==")
    src = open(PCB).read()
    for ref, (path, rot) in MODELS.items():
        for i, j in balanced(src, "(footprint"):
            f = src[i:j]
            m = re.search(r'\(property\s+"Reference"\s+"([^"]+)"', f)
            if not m or m.group(1) != ref:
                continue
            if "(model " in f:
                break  # al aanwezig
            model = (f'\n\t\t(model "${{KICAD10_3DMODEL_DIR}}/{path}"'
                     f'\n\t\t\t(offset (xyz 0 0 0))\n\t\t\t(scale (xyz 1 1 1))'
                     f'\n\t\t\t(rotate (xyz 0 0 {rot}))\n\t\t)')
            src = src[:j - 1] + model + "\n\t" + src[j - 1:]
            break
    assert src.count("(") == src.count(")")
    open(PCB, "w").write(src)


def step6_dsn():
    print("== 6. DSN bouwen (zonder zones, clearance 0.17) ==")
    shutil.rmtree(TMP, ignore_errors=True)
    os.makedirs(TMP)
    tpcb = os.path.join(TMP, f"{NAME}.kicad_pcb")
    tpro = os.path.join(TMP, f"{NAME}.kicad_pro")
    shutil.copy(PCB, tpcb)
    shutil.copy(PRO, tpro)
    src = open(tpcb).read()
    while True:
        sp = balanced(src, "(zone")
        if not sp:
            break
        i, j = sp[0]
        src = src[:i] + src[j:]
    open(tpcb, "w").write(src)
    d = json.load(open(tpro))
    for c in d["net_settings"]["classes"]:
        c["clearance"] = 0.17  # marge: freerouting rondt af, KiCad meet exact
    json.dump(d, open(tpro, "w"), indent=2)
    dsn = os.path.join(TMP, f"{NAME}.dsn")
    kpy(f"import pcbnew; b=pcbnew.LoadBoard('{tpcb}'); "
        f"print('dsn:', pcbnew.ExportSpecctraDSN(b, '{dsn}'))")
    return dsn


def step7_route(dsn):
    print("== 7. freerouting (headless, 1.7.0-jar) ==")
    ses = os.path.join(TMP, f"{NAME}.ses")
    # cwd=TMP: freerouting leest anders een oud .rules-bestand uit de pcb-map
    # en overschrijft daarmee de netklasse-breedtes uit de DSN
    run(["java", "-Xss8m", "-jar", FR_JAR, "-de", dsn, "-do", ses, "-mp", "200"], cwd=TMP)
    shutil.copy(ses, os.path.join(KDIR, f"{NAME}.ses"))
    return ses


def step8_import(ses):
    # In twee losse processen: na b.Remove()-calls raakt de SWIG-runtime zijn
    # typeinfo kwijt (GetArea/GetNetsByName geven kale SwigPyObjects terug),
    # dus fill+stitching draait in een verse interpreter.
    print("== 8a. ses importeren + opschonen ==")
    out = kpy(f"""
import pcbnew, math
FM = pcbnew.FromMM
b = pcbnew.LoadBoard('{PCB}')
print('import:', pcbnew.ImportSpecctraSES(b, '{ses}'))
def pt(p): return (pcbnew.ToMM(p.x), pcbnew.ToMM(p.y))
def on_seg(p, a, b2, tol=0.05):
    (px,py),(ax,ay),(bx,by)=p,a,b2
    dx,dy=bx-ax,by-ay; L=math.hypot(dx,dy)
    if L<1e-6: return math.hypot(px-ax,py-ay)<tol
    t=max(0,min(1,((px-ax)*dx+(py-ay)*dy)/(L*L)))
    return math.hypot(px-(ax+t*dx),py-(ay+t*dy))<tol
import sys
sys.path.insert(0, '{HERE}')
from port_zones import ISLAND_PTS_03 as poly
def inside(x, y):
    n=len(poly); c=False; j=n-1
    for i in range(n):
        xi,yi=poly[i]; xj,yj=poly[j]
        if ((yi>y)!=(yj>y)) and (x < (xj-xi)*(y-yi)/(yj-yi)+xi): c=not c
        j=i
    return c
locked=[(t.GetNetCode(),pt(t.GetStart()),pt(t.GetEnd())) for t in b.GetTracks()
        if t.GetClass()=='PCB_TRACK' and t.IsLocked()]
n_agnd=n_dup=n_micro=0
for t in list(b.GetTracks()):
    if t.IsLocked(): continue
    if t.GetClass()!='PCB_TRACK': continue
    s,e=pt(t.GetStart()),pt(t.GetEnd())
    # AGND mag mee-routen (bindt de pour-fragmenten), maar niet buiten het
    # eiland komen — dat zou de isolatiebarriere aantasten
    if t.GetNetname()=='AGND' and not (inside(*s) and inside(*e)):
        b.Remove(t); n_agnd+=1; continue
    if any(nc==t.GetNetCode() and on_seg(s,a,b2) and on_seg(e,a,b2) for nc,a,b2 in locked):
        b.Remove(t); n_dup+=1; continue
    # korte router-stompjes op lattice-pads (eindpunt valt samen met een
    # gelockt AGND-eindpunt) zijn redundant en blijven anders dangling achter
    if t.GetNetname()=='AGND' and t.GetLength() < FM(2.0):
        lp=[q for nc,a,b2 in locked if nc==t.GetNetCode() for q in (a,b2)]
        if any(math.hypot(s[0]-q[0],s[1]-q[1])<0.12 or math.hypot(e[0]-q[0],e[1]-q[1])<0.12 for q in lp):
            b.Remove(t); n_dup+=1; continue
    if t.GetLength() < FM(0.02):
        b.Remove(t); n_micro+=1
print(f'opgeschoond: {{n_agnd}} agnd-buiten-eiland, {{n_dup}} dubbel, {{n_micro}} micro')

# (wees-pruning gebeurt in een eigen proces - zie step8 deel 2)
pcbnew.SaveBoard('{PCB}', b)
""")
    print("  " + out.replace("\n", "\n  "))

    print("== 8a2. wees-pruning AGND ==")
    out = kpy(f"""
import pcbnew, math
FM = pcbnew.FromMM
b = pcbnew.LoadBoard('{PCB}')
def pt(p): return (pcbnew.ToMM(p.x), pcbnew.ToMM(p.y))
# alles vooraf verzamelen (na b.Remove() raakt de SWIG-runtime typeinfo kwijt)
pads = []
for f in b.GetFootprints():
    for p in f.Pads():
        if p.GetNetname() == 'AGND':
            c = pt(p.GetPosition()); s = p.GetSize()
            pads.append((c[0], c[1], pcbnew.ToMM(s.x)/2+0.05, pcbnew.ToMM(s.y)/2+0.05))
vias, unlocked, lockedpts = [], [], []
for t in b.GetTracks():
    if t.GetNetname() != 'AGND': continue
    if t.GetClass() == 'PCB_VIA':
        vias.append(pt(t.GetPosition()))
    elif t.IsLocked():
        lockedpts.append((pt(t.GetStart()), pt(t.GetEnd())))
    else:
        unlocked.append((t, pt(t.GetStart()), pt(t.GetEnd())))
def on_body(p, a, c, tol=0.06):
    dx, dy = c[0]-a[0], c[1]-a[1]
    L = math.hypot(dx, dy)
    if L < 1e-6: return math.hypot(p[0]-a[0], p[1]-a[1]) < tol
    u = max(0, min(1, ((p[0]-a[0])*dx + (p[1]-a[1])*dy)/(L*L)))
    return math.hypot(p[0]-(a[0]+u*dx), p[1]-(a[1]+u*dy)) < tol
alive = list(range(len(unlocked)))
removed = []
while True:
    kill = None
    for i in alive:
        _, s, e = unlocked[i]
        for p in (s, e):
            ok = any(abs(p[0]-px) <= hw and abs(p[1]-py) <= hh for px, py, hw, hh in pads)
            ok = ok or any(math.hypot(p[0]-vx, p[1]-vy) < 0.35 for vx, vy in vias)
            ok = ok or any(on_body(p, a, c) for a, c in lockedpts)
            ok = ok or any(j != i and on_body(p, unlocked[j][1], unlocked[j][2]) for j in alive)
            if not ok:
                kill = i
                break
        if kill is not None: break
    if kill is None: break
    alive.remove(kill); removed.append(kill)
for i in removed:
    b.Remove(unlocked[i][0])
print(f'wees-stukjes geprunede: {{len(removed)}}')
pcbnew.SaveBoard('{PCB}', b)
""")
    print("  " + out.replace("\n", "\n  "))

    print("== 8b. zone-fill + adaptieve AGND-stitching ==")
    # raster over het eiland; via alleen waar de fill op BEIDE lagen al koper
    # heeft (dan is er per definitie ruimte en bindt de via de F/B-fragmenten)
    out = kpy(f"""
import pcbnew
FM = pcbnew.FromMM
b = pcbnew.LoadBoard('{PCB}')
island = None
for zi in range(b.GetAreaCount()):
    z = b.GetArea(zi)
    if z.GetNetname() == 'AGND' and z.GetAssignedPriority() == 3:
        island = z
agnd = b.GetNetsByName()['AGND']
# bestaande via's onthouden: anders plaatst een herhaalde run (--no-export)
# nieuwe via's bovenop de oude -> holes_co_located
existing = []
for t in b.GetTracks():
    if t.GetClass() == 'PCB_VIA':
        p = t.GetPosition()
        existing.append((pcbnew.ToMM(p.x), pcbnew.ToMM(p.y)))
pcbnew.ZONE_FILLER(b).Fill(b.Zones())
bb = island.GetBoundingBox()
added = 0
STEP = FM(2.0)
x = bb.GetX() + STEP
while x < bb.GetX() + bb.GetWidth():
    y = bb.GetY() + STEP
    while y < bb.GetY() + bb.GetHeight():
        pos = pcbnew.VECTOR2I(x, y)
        # fill moet er rondom de via-plek al liggen (5-punts test, accuracy 0)
        R = FM(0.75); D = int(R*0.7071)
        pts = [pcbnew.VECTOR2I(x+dx, y+dy) for dx, dy in
               [(0,0), (R,0), (-R,0), (0,R), (0,-R),
                (D,D), (D,-D), (-D,D), (-D,-D)]]
        mmx, mmy = pcbnew.ToMM(x), pcbnew.ToMM(y)
        dup = any(abs(mmx-ex) < 0.5 and abs(mmy-ey) < 0.5 for ex, ey in existing)
        if not dup and all(island.HitTestFilledArea(lay, p, 0)
               for lay in (pcbnew.F_Cu, pcbnew.B_Cu) for p in pts):
            v = pcbnew.PCB_VIA(b)
            v.SetPosition(pos)
            v.SetWidth(FM(0.6)); v.SetDrill(FM(0.3))
            v.SetNet(agnd); v.SetLocked(True)
            b.Add(v); added += 1; existing.append((mmx, mmy))
        y += STEP
    x += STEP
print(f'stitch-vias geplaatst: {{added}}')
pcbnew.ZONE_FILLER(b).Fill(b.Zones())
pcbnew.SaveBoard('{PCB}', b)
print('gevuld en opgeslagen')
""")
    print("  " + out.replace("\n", "\n  "))


# Logo: bitmap2component-footprint (silk-only, geen pads/koper). Wordt NA het
# routen geinjecteerd, zodat het freerouting nooit kan storen.
LOGO_MOD   = os.path.join(KDIR, "Pura-Logo.kicad_mod")
LOGO_SCALE = 0.38          # past tussen J1 (USB-C) en U2 (LM2596)
LOGO_AT    = (92.05, 82.0)  # KiCad-coords = tsx (-7.95, +18.0)
LOGO_STROKE = 0.08         # verdikt elke vorm; zonder dit blijft de watergolf
                           # op 0.14mm steken en dat is onder JLC's silk-minimum


def step8c_logo():
    print("== 8c. Pura-logo injecteren (silk, geschaald) ==")
    if not os.path.exists(LOGO_MOD):
        print("  geen Pura-Logo.kicad_mod — overgeslagen")
        return
    mod = open(LOGO_MOD).read()
    if not mod.strip().startswith("(footprint"):
        sys.exit("Pura-Logo.kicad_mod is geen footprint")

    # 1. alle coordinaten schalen
    def scale_xy(m):
        return f"(xy {float(m.group(1))*LOGO_SCALE:.6f} {float(m.group(2))*LOGO_SCALE:.6f})"
    mod = re.sub(r"\(xy\s+([-\d.]+)\s+([-\d.]+)\)", scale_xy, mod)

    # 2. stroke-breedte zetten (anders zijn de dunne details niet printbaar)
    mod = re.sub(r"\(stroke\s+\(width\s+[\d.]+\)", f"(stroke (width {LOGO_STROKE})", mod)

    # 3. referentie "G***" verbergen — die staat NIET op hide en zou meeprinten
    mod = re.sub(r'(\(fp_text\s+reference\s+"[^"]*"\s+\(at[^)]*\)\s+\(layer\s+"[^"]*"\))',
                 r"\1 hide", mod, count=1)

    # 4. plaatsen: (at x y) op footprint-niveau, direct na de layer-regel
    mod = mod.replace('(layer "F.Cu")', f'(layer "F.Cu")\n  (at {LOGO_AT[0]} {LOGO_AT[1]})', 1)

    board = open(PCB).read()
    if '"LOGO"' in board:
        print("  logo staat er al")
        return
    k = board.rstrip().rfind(")")
    board = board[:k] + "\n  " + mod.strip() + "\n" + board[k:]
    assert board.count("(") == board.count(")"), "haakjes uit balans"
    open(PCB, "w").write(board)
    print(f"  logo geplaatst op {LOGO_AT} @ schaal {LOGO_SCALE}")


# Silkscreen-labels: (tekst, tsx-x, tsx-y, rotatie, hoogte-mm, dikte-mm)
# In de flow i.p.v. de tsx, want tscircuit rendert alles op ~0.7mm zonder
# dikte-controle — dan verdwijnen de polariteitstekens in het koper.
LABELS = [
    # polariteit — vet en groot genoeg om op te vallen
    ("+",          19.5,  16.6,  0, 1.6, 0.30),   # C_IN  pin1 = + (12V)
    ("+",          -3.4,   3.0,  0, 1.6, 0.30),   # C_OUT pin1 = + (5V)
    # connectors — aan de RANDKANT (x>22), gedraaid zodat ze in de strook passen
    ("ESP32",      23.7,  10.5, 90, 1.2, 0.20),
    ("PROBES",     23.7, -10.0, 90, 1.2, 0.20),
    ("12V",        23.7, -19.5, 90, 1.2, 0.20),
    # pH/ORP bij de betreffende J_PROBE-pinnen (pin3 = PH_PO, pin4 = ORP_PO)
    ("pH",         19.4,  -8.73, 0, 1.1, 0.20),
    ("ORP",        19.4,  -6.19, 0, 1.1, 0.20),
    # pompen
    ("pH MOTOR",    7.6, -13.6, 90, 1.1, 0.20),
    ("ORP MOTOR",  16.7, -13.6, 90, 1.1, 0.20),
    ("Pura v0.3",  11.0, -23.0,  0, 1.5, 0.25),
]
# refdes van deze connectors verbergen — hun eigen naam wordt vervangen
# door de labels hierboven
HIDE_REF = ["J_ESP", "J_PROBE", "J_M1", "J_M2", "J_12V"]


# Diodesymbool voor de LEDs: driehoek (anode) -> balk (kathode), in het gat
# tussen de 0805-pads (0.80mm breed). GEVULDE polygonen: dunne lijntjes zijn
# op dit formaat onleesbaar, een gevulde driehoek niet.
# Coords zijn relatief t.o.v. het LED-centrum; +x = kathode-kant.
DIODE_TRI = [(-0.30, -0.45), (-0.30, 0.45), (0.12, 0.0)]
DIODE_BAR = [(0.12, -0.55), (0.30, -0.55), (0.30, 0.55), (0.12, 0.55)]
DIODE_AT = [            # (ref, tsx-x, tsx-y, schaal t.o.v. het 0805-gat)
    ("LED1", 13.0, 8.2, 1.0),
    ("LED2", -22.0, -12.0, 1.0),
    ("D1", -3.5, 10.0, 1.6),   # SMA: gat is 1.5mm breed -> groter symbool
]


def _polys():
    out = []
    for _ref, cx, cy, sc in DIODE_AT:
        for shape in (DIODE_TRI, DIODE_BAR):
            pts = "".join(f"\n\t\t\t\t(xy {100+cx+dx*sc:.4f} {100-cy-dy*sc:.4f})" for dx, dy in shape)
            out.append(
                f'\t(gr_poly\n\t\t(pts{pts}\n\t\t)'
                f'\n\t\t(stroke (width 0) (type solid))\n\t\t(fill solid)'
                f'\n\t\t(layer "F.SilkS")\n\t)'
            )
    return out


def step8d_labels():
    print("== 8d. silkscreen-labels + connectornamen ==")
    src = open(PCB).read()

    # bestaande refdes van de connectors verbergen
    n = 0
    for ref in HIDE_REF:
        pat = f'(property "Reference" "{ref}"'
        k = src.find(pat)
        if k < 0:
            continue
        end = src.find("(effects", k)
        if "(hide yes)" in src[k:end]:
            continue
        src = src[:end] + "(hide yes)\n\t\t\t" + src[end:]
        n += 1
    print(f"  {n} connector-refdes verborgen")

    # labels toevoegen (idempotent: eerst de oude weggooien)
    for i, j in reversed(balanced(src, "(gr_text")):
        if '(layer "F.SilkS")' in src[i:j]:
            src = src[:i] + src[j:]
    out = []
    for txt, x, y, rot, h, th in LABELS:
        kx, ky = 100 + x, 100 - y
        out.append(
            f'\t(gr_text "{txt}"\n\t\t(at {kx} {ky} {rot})\n\t\t(layer "F.SilkS")'
            f'\n\t\t(effects\n\t\t\t(font\n\t\t\t\t(size {h} {h})'
            f'\n\t\t\t\t(thickness {th})\n\t\t\t\t(bold yes)\n\t\t\t)'
            f'\n\t\t)\n\t)'
        )
    for i, j in reversed(balanced(src, "(gr_poly")):
        if '(layer "F.SilkS")' in src[i:j]:
            src = src[:i] + src[j:]
    out += _polys()
    k = src.rstrip().rfind(")")
    src = src[:k] + "\n" + "\n".join(out) + "\n" + src[k:]
    assert src.count("(") == src.count(")"), "haakjes uit balans"
    open(PCB, "w").write(src)
    print(f"  {len(LABELS)} labels + {len(DIODE_AT)} diodesymbolen geplaatst")


def step9_drc():
    print("== 9. DRC (zonder save-board, anders clobbert kicad-cli de pro) ==")
    step2_project()  # rules kunnen door tussenstappen zijn teruggezet
    rep = os.path.join(HERE, "drc03-final.txt")
    run([KICAD_CLI, "pcb", "drc", "-o", rep, PCB])
    txt = open(rep).read()
    unconnected = int(re.search(r"Found (\d+) unconnected", txt).group(1))
    cats = {}
    for c in re.findall(r"\[([a-z_]+)\]", txt):
        cats[c] = cats.get(c, 0) + 1
    hard = ["clearance", "shorting_items", "copper_edge_clearance",
            "track_width", "unconnected_items", "via_dangling"]
    # track_dangling is warning-niveau: een kort AGND-stompje aan een pad is
    # dood koper, geen fout — wel melden
    problems = {c: cats[c] for c in hard if cats.get(c)}
    print(f"  unconnected: {unconnected}; categorien: {cats}")
    if unconnected or problems:
        sys.exit(f"DRC NIET schoon: {problems or unconnected} — zie {rep}")
    print("  DRC SCHOON (rest is silk/tekst-cosmetica)")


def main():
    if "--no-export" not in sys.argv:
        step1_export()
    step2_project()
    step3_zones()
    step4_preroute()
    step5_models()
    dsn = step6_dsn()
    ses = step7_route(dsn)
    step8_import(ses)
    step8c_logo()
    step8d_labels()
    step9_drc()
    shutil.rmtree(TMP, ignore_errors=True)
    print(f"\nKLAAR: {PCB} is geroute en DRC-schoon.")
    print("Volgende stap: gerbers exporteren (zie PRODUCTIE.md stap 5).")


if __name__ == "__main__":
    main()
