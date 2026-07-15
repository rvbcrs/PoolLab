#!/usr/bin/env python3
"""Pre-route de iso-netten + pH/ORP-lijnen als GELOCKTE sporen in het 0.3-board.

Waarom: freerouting kent het isolatie-eiland niet — het legde de AIN-lijnen
dwars door de motorsectie en knipte iso-netten buiten het eiland. Deze routes
liggen bewust: I2C/V5_ISO binnen het eiland, AIN0/AIN1 door de gang onderlangs
(B.Cu) naar J_PROBE. Locked tracks exporteert KiCad als "fix" in de DSN, zodat
freerouting ze laat staan.

  python3 preroute_iso.py [doel.kicad_pcb]
"""
import math
import re
import sys

CLR = 0.15  # minimale clearance (mm), zelfde als design rules

V5 = ".U5 > .pin4 to .U6 > .VDD1"
SDA = ".U3 > .SDA to .R_SDA > .pin1"
SCL = ".U3 > .SCL to .R_SCL > .pin1"
ADDR = ".U3 > .ADDR to .R_ADDR > .pin1"
AIN0 = ".U3 > .AIN0 to .J_PROBE > .PH_PO"
AIN1 = ".U3 > .AIN1 to .J_PROBE > .ORP_PO"

# (net, layer, width, (x1,y1), (x2,y2))
SEGS = [
    # V5_ISO: U5.4 -> R_BLD.1 -> J_PROBE.1 (gang, y=119.1 wegens R_BLD.2-pad)
    (V5, "F.Cu", 0.25, (83.19, 115.8), (83.19, 119.1)),
    (V5, "F.Cu", 0.25, (83.19, 119.1), (98.59, 119.1)),
    (V5, "F.Cu", 0.25, (98.59, 119.1), (98.59, 118.0)),
    (V5, "F.Cu", 0.25, (98.59, 119.1), (120.15, 119.1)),
    (V5, "F.Cu", 0.25, (120.15, 119.1), (120.15, 114.6)),
    (V5, "F.Cu", 0.25, (120.15, 114.6), (122.0, 113.81)),
    (V5, "F.Cu", 0.25, (88.11, 104.93), (88.11, 104.2)),
    (V5, "F.Cu", 0.25, (88.11, 104.2), (91.6, 104.2)),
    (V5, "F.Cu", 0.25, (91.6, 104.2), (91.6, 98.0)),
    (V5, "F.Cu", 0.25, (91.6, 98.0), (92.09, 98.0)),
    (V5, "F.Cu", 0.25, (92.09, 98.0), (92.09, 95.4)),
    # V5_ISO: U3.8 -> R_SCL.2 -> R_SDA.2 (pull-up rail)
    (V5, "F.Cu", 0.25, (98.40, 107.35), (98.40, 104.9)),
    (V5, "F.Cu", 0.25, (98.40, 104.9), (96.91, 104.9)),
    (V5, "F.Cu", 0.25, (96.91, 104.9), (96.91, 101.6)),
    # V5_ISO: R_LED2.1 -> U5.4 (west om LED2 heen)
    (V5, "F.Cu", 0.25, (77.09, 110.0), (77.09, 108.5)),
    (V5, "F.Cu", 0.25, (77.09, 108.5), (83.5, 108.5)),
    (V5, "F.Cu", 0.25, (83.5, 108.5), (83.5, 114.5)),
    (V5, "F.Cu", 0.25, (83.5, 114.5), (83.19, 115.8)),
    # SDA: U6.2 -> R_SDA.1 -> U3.9 (B.Cu diagonalen, U3.9 van zuidoost)
    (SDA, "F.Cu", 0.2, (86.84, 104.93), (86.84, 105.35)),
    (SDA, "B.Cu", 0.2, (86.84, 105.35), (90.59, 101.6)),
    (SDA, "B.Cu", 0.2, (90.59, 101.6), (94.35, 101.6)),
    (SDA, "F.Cu", 0.2, (94.35, 101.6), (95.09, 101.6)),
    (SDA, "B.Cu", 0.2, (94.35, 101.6), (98.1, 105.35)),
    (SDA, "B.Cu", 0.2, (98.1, 105.35), (98.1, 109.4)),
    (SDA, "F.Cu", 0.2, (98.1, 109.4), (97.90, 107.35)),
    # SCL: U6.3 -> R_SCL.1 -> U3.10 (van zuid; corridors gescheiden van SDA)
    (SCL, "F.Cu", 0.2, (85.56, 104.93), (85.56, 106.4)),
    (SCL, "B.Cu", 0.2, (85.56, 106.4), (91.75, 106.4)),
    (SCL, "B.Cu", 0.2, (91.75, 106.4), (94.35, 103.8)),
    (SCL, "F.Cu", 0.2, (94.35, 103.8), (95.09, 103.8)),
    (SCL, "B.Cu", 0.2, (94.35, 103.8), (97.0, 106.45)),
    (SCL, "B.Cu", 0.2, (97.0, 106.45), (97.0, 108.8)),
    (SCL, "F.Cu", 0.2, (97.0, 108.8), (97.40, 107.35)),
    # ADDR: U3.1 -> R_ADDR.1 (oostelijke aanloop tussen R_ADDR.1 en R_ADDR.2)
    (ADDR, "F.Cu", 0.2, (97.40, 112.05), (96.6, 112.05)),
    (ADDR, "F.Cu", 0.2, (96.6, 112.05), (96.6, 110.3)),
    (ADDR, "F.Cu", 0.2, (96.6, 110.3), (100.6, 110.3)),
    (ADDR, "F.Cu", 0.2, (100.6, 110.3), (100.6, 103.8)),
    (ADDR, "F.Cu", 0.2, (100.6, 103.8), (99.69, 103.8)),
    # AIN0: U3.4 -> (via) -> gang B.Cu -> J_PROBE.3
    (AIN0, "F.Cu", 0.25, (98.90, 112.05), (98.90, 115.5)),
    (AIN0, "F.Cu", 0.25, (98.90, 115.5), (99.5, 116.1)),
    (AIN0, "F.Cu", 0.25, (99.5, 116.1), (99.5, 117.0)),
    (AIN0, "B.Cu", 0.25, (99.5, 117.0), (99.5, 119.6)),
    (AIN0, "B.Cu", 0.25, (99.5, 119.6), (120.5, 119.6)),
    (AIN0, "B.Cu", 0.25, (120.5, 119.6), (120.5, 108.73)),
    (AIN0, "B.Cu", 0.25, (120.5, 108.73), (122.0, 108.73)),
    # AIN1: U3.5 -> (via) -> gang B.Cu -> J_PROBE.4
    (AIN1, "F.Cu", 0.25, (99.40, 112.05), (99.40, 115.0)),
    (AIN1, "F.Cu", 0.25, (99.40, 115.0), (101.5, 116.0)),
    (AIN1, "F.Cu", 0.25, (101.5, 116.0), (101.5, 116.8)),
    (AIN1, "B.Cu", 0.25, (101.5, 116.8), (101.5, 119.15)),
    (AIN1, "B.Cu", 0.25, (101.5, 119.15), (119.9, 119.15)),
    (AIN1, "B.Cu", 0.25, (119.9, 119.15), (119.9, 106.19)),
    (AIN1, "B.Cu", 0.25, (119.9, 106.19), (122.0, 106.19)),
    # --- AGND-stitching: de eiland-fill fragmenteert door sporen, en de F/B-
    # lagen van de zone verbinden nergens vanzelf. Alles pad-verankerd, anders
    # geeft KiCads connectivity-pass zwevende items een willekeurig net.
    ("AGND", "B.Cu", 0.25, (85.73, 115.8), (85.73, 117.8)),     # stub U5.3
    ("AGND", "F.Cu", 0.25, (84.30, 104.93), (83.5, 104.3)),     # stub U6.4
    ("AGND", "F.Cu", 0.25, (101.51, 101.6), (101.95, 102.4)),   # stub C_ADS.2
    # keten LED2.2 <-> U6.4 (westblok <-> topstrook, om de GND-diag heen)
    ("AGND", "F.Cu", 0.25, (78.91, 112.0), (78.91, 113.3)),
    ("AGND", "B.Cu", 0.25, (78.91, 113.3), (78.3, 112.7)),
    ("AGND", "B.Cu", 0.25, (78.3, 112.7), (78.3, 105.0)),
    ("AGND", "B.Cu", 0.25, (78.3, 105.0), (84.6, 103.95)),
    ("AGND", "F.Cu", 0.25, (84.6, 103.95), (84.3, 104.93)),
    # stitch noordvinger (anker: C_DC_OUT.2)
    ("AGND", "F.Cu", 0.25, (93.91, 98.0), (93.2, 99.3)),
    # vinger -> hoofdblok door de nek (anders stuurt freerouting AGND buitenom)
    ("AGND", "F.Cu", 0.25, (93.91, 98.0), (93.0, 98.6)),
    ("AGND", "F.Cu", 0.25, (93.0, 98.6), (93.0, 104.4)),
    # vinger -> gang-stitch, verticaal BINNEN het eiland (F-nek + via + B-af)
    ("AGND", "F.Cu", 0.25, (93.0, 104.4), (93.0, 109.0)),
    ("AGND", "B.Cu", 0.25, (93.0, 109.0), (93.0, 119.42)),
    # stitch gang (anker: U5.3-stub)
    ("AGND", "B.Cu", 0.25, (85.73, 117.8), (95.6, 120.0)),
    # stitch midden-oost (anker: U3.3)
    ("AGND", "F.Cu", 0.25, (98.40, 112.05), (98.40, 110.9)),
    ("AGND", "F.Cu", 0.25, (98.40, 110.9), (102.1, 110.9)),
    ("AGND", "F.Cu", 0.25, (102.1, 110.9), (102.1, 105.5)),
    # J_PROBE.2 oostwaarts aan de hoofdfill knopen (pocket raakt anders
    # ingesloten tussen de AIN-sporen en de connectorpads) - beide lagen
    ("AGND", "F.Cu", 0.25, (122.0, 111.27), (123.4, 111.27)),
    ("AGND", "B.Cu", 0.25, (122.0, 111.27), (123.4, 111.27)),
    # topstrook-bond west (fill-anker voor de strook boven R_SDA2/R_SCL2)
    ("AGND", "F.Cu", 0.25, (83.5, 104.3), (81.3, 104.0)),
]
# (net, (x,y)) - via 0.6/0.3, F.Cu<->B.Cu
VIAS = [
    (SDA, (86.84, 105.35)),
    (SDA, (94.35, 101.6)),
    (SDA, (98.1, 109.4)),
    (SCL, (85.56, 106.4)),
    (SCL, (94.35, 103.8)),
    (SCL, (97.0, 108.8)),
    (AIN0, (99.5, 117.0)),
    (AIN1, (101.5, 116.8)),
    ("AGND", (78.91, 113.3)),
    ("AGND", (93.0, 109.0)),
    ("AGND", (84.6, 103.95)),
    ("AGND", (93.2, 99.3)),
    ("AGND", (95.6, 120.0)),
    ("AGND", (102.1, 105.5)),
]
VIA_SIZE, VIA_DRILL = 0.6, 0.3



def _seg_clear(seg, pads):
    net, lay, w, (x1, y1), (x2, y2) = seg
    for ref, pname, pnet, cx, cy, hw, hh, pl in pads:
        if pnet == net or (pl != "TH" and pl != lay):
            continue
        if seg_rect_dist(x1, y1, x2, y2, cx, cy, hw, hh) - w / 2 < CLR + 0.03:
            return False
    return True


def to45(segs, pads=None):
    """Normaliseer vrije-hoek-segmenten naar H/V + exact 45 graden (dogleg).

    Segmenten die al (visueel) horizontaal, verticaal of 45 zijn blijven staan.
    Per diagonaal worden twee varianten geprobeerd (45-eerst of recht-eerst);
    de eerste die clearance houdt wint. Eindpunten (pads/vias) verschuiven nooit.
    """
    out = []
    for net, lay, w, a, b in segs:
        dx, dy = b[0] - a[0], b[1] - a[1]
        adx, ady = abs(dx), abs(dy)
        if adx < 0.15 or ady < 0.15 or abs(adx - ady) < 0.15:
            out.append((net, lay, w, a, b))
            continue
        d = min(adx, ady)
        sx = 1 if dx > 0 else -1
        sy = 1 if dy > 0 else -1
        m1 = (round(a[0] + sx * d, 3), round(a[1] + sy * d, 3))   # 45 eerst
        m2 = (round(b[0] - sx * d, 3), round(b[1] - sy * d, 3))   # recht eerst
        for m in (m1, m2):
            cand = [(net, lay, w, a, m), (net, lay, w, m, b)]
            if pads is None or all(_seg_clear(c, pads) for c in cand):
                out.extend(cand)
                break
        else:
            out.append((net, lay, w, a, b))  # geen variant vrij: laat staan
    return out


def chamfer(segs, vias, pads=None, d_max=1.0):
    """Vervang haakse binnenhoeken door 45-graden-afschuiningen.

    Alleen op punten waar precies twee as-uitgelijnde, loodrechte segmenten
    van hetzelfde net+laag samenkomen en waar geen via zit — pad-ingangen,
    T-splitsingen en via-overgangen blijven ongemoeid.
    """
    key = lambda p: (round(p[0], 4), round(p[1], 4))
    via_pts = {key(p) for _, p in vias}
    joints = {}
    for idx, (net, lay, w, a, b) in enumerate(segs):
        for p in (a, b):
            joints.setdefault((net, lay, key(p)), []).append(idx)
    segs = [list(s) for s in segs]
    extra = []
    for (net, lay, pt), idxs in joints.items():
        if len(idxs) != 2 or pt in via_pts:
            continue
        dirs = []
        for i in idxs:
            _, _, w, a, b = segs[i]
            far = b if key(a) == pt else a
            dx, dy = far[0] - pt[0], far[1] - pt[1]
            L = math.hypot(dx, dy)
            dirs.append((i, (dx / L, dy / L), L, w))
        (i1, u1, l1, w1), (i2, u2, l2, w2) = dirs
        axis = lambda u: (abs(round(u[0])), abs(round(u[1])))
        if {axis(u1), axis(u2)} != {(1, 0), (0, 1)}:
            continue  # niet allebei as-uitgelijnd loodrecht
        d = min(d_max, l1 * 0.5, l2 * 0.5)
        ok = None
        while d >= 0.3:
            p1 = (round(pt[0] + u1[0] * d, 3), round(pt[1] + u1[1] * d, 3))
            p2 = (round(pt[0] + u2[0] * d, 3), round(pt[1] + u2[1] * d, 3))
            w = min(w1, w2)
            clear = True
            for ref, pname, pnet, cx, cy, hw, hh, pl in (pads or []):
                if pnet == net or (pl != "TH" and pl != lay):
                    continue
                if seg_rect_dist(p1[0], p1[1], p2[0], p2[1], cx, cy, hw, hh) - w / 2 < CLR + 0.03:
                    clear = False
                    break
            if clear:
                ok = (p1, p2, w)
                break
            d /= 2  # kleinere afschuining proberen
        if not ok:
            continue  # hoek blijft haaks
        p1, p2, w = ok
        for i, pnew in ((i1, p1), (i2, p2)):
            if key(tuple(segs[i][3])) == pt:
                segs[i][3] = pnew
            else:
                segs[i][4] = pnew
        extra.append((net, lay, w, p1, p2))
    return [tuple(s) for s in segs] + extra


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


def load_pads(src):
    """[(ref, pad, net|None, cx, cy, halfw, halfh, layers)] — bbox is conservatief
    (rotatie -> omhullende doos)."""
    pads = []
    for i, j in balanced(src, "(footprint"):
        f = src[i:j]
        at = re.search(r"\(at\s+([-\d.]+)\s+([-\d.]+)(?:\s+([-\d.]+))?\)", f)
        fx, fy, frot = float(at.group(1)), float(at.group(2)), float(at.group(3) or 0)
        ref = re.search(r'\(property\s+"Reference"\s+"([^"]+)"', f)
        ref = ref.group(1) if ref else "?"
        for k, l in balanced(f, "(pad"):
            p = f[k:l]
            pa = re.search(r"\(at\s+([-\d.]+)\s+([-\d.]+)(?:\s+([-\d.]+))?\)", p)
            sz = re.search(r"\(size\s+([\d.]+)\s+([\d.]+)\)", p)
            if not pa or not sz:
                continue
            px, py, prot = float(pa.group(1)), float(pa.group(2)), float(pa.group(3) or 0)
            r = math.radians(frot)
            ax = fx + px * math.cos(r) + py * math.sin(r)
            ay = fy - px * math.sin(r) + py * math.cos(r)
            w, h = float(sz.group(1)), float(sz.group(2))
            if (prot - frot) % 180 == 90 or frot % 180 == 90:
                w, h = h, w
            net = re.search(r'\(net\s+(?:\d+\s+)?"([^"]*)"\)', p)
            name = re.search(r'\(pad\s+"([^"]*)"', p).group(1)
            lay = re.search(r"\(layers\s+([^)]*)\)", p)
            lay = lay.group(1) if lay else ""
            if "thru_hole" in p[:80] or ("F.Cu" in lay and "B.Cu" in lay) or "*.Cu" in lay:
                layers = "TH"
            elif "F.Cu" in lay:
                layers = "F.Cu"
            else:
                layers = "B.Cu"
            pads.append((ref, name, net.group(1) if net else None, ax, ay, w / 2, h / 2, layers))
    return pads


def seg_rect_dist(x1, y1, x2, y2, cx, cy, hw, hh):
    """Afstand lijnsegment tot rechthoek (as-aligned), 0 bij overlap."""
    steps = max(2, int(math.hypot(x2 - x1, y2 - y1) / 0.05))
    best = 1e9
    for t in (k / steps for k in range(steps + 1)):
        x, y = x1 + (x2 - x1) * t, y1 + (y2 - y1) * t
        dx = max(abs(x - cx) - hw, 0.0)
        dy = max(abs(y - cy) - hh, 0.0)
        best = min(best, math.hypot(dx, dy))
    return best


def seg_seg_dist(a, b):
    (x1, y1, x2, y2), (x3, y3, x4, y4) = a, b
    steps = 40
    best = 1e9
    for t in (k / steps for k in range(steps + 1)):
        px, py = x1 + (x2 - x1) * t, y1 + (y2 - y1) * t
        for u in (k / steps for k in range(steps + 1)):
            qx, qy = x3 + (x4 - x3) * u, y3 + (y4 - y3) * u
            best = min(best, math.hypot(px - qx, py - qy))
    return best


def validate(pads):
    errs = []
    for net, layer, w, (x1, y1), (x2, y2) in SEGS:
        for ref, pname, pnet, cx, cy, hw, hh, pl in pads:
            if pnet == net:
                continue
            if pl != "TH" and pl != layer:
                continue
            d = seg_rect_dist(x1, y1, x2, y2, cx, cy, hw, hh) - w / 2
            if d < CLR:
                errs.append(f"seg {net[:24]} {layer} ({x1},{y1})-({x2},{y2}) vs pad {ref}.{pname} [{pnet}]: {d:.3f}")
    for i, (n1, l1, w1, p1, p2) in enumerate(SEGS):
        for n2, l2, w2, p3, p4 in SEGS[i + 1:]:
            if n1 == n2 or l1 != l2:
                continue
            d = seg_seg_dist((*p1, *p2), (*p3, *p4)) - w1 / 2 - w2 / 2
            if d < CLR:
                errs.append(f"seg-seg {n1[:20]}/{n2[:20]} {l1}: {d:.3f}")
    for vnet, (vx, vy) in VIAS:
        for ref, pname, pnet, cx, cy, hw, hh, pl in pads:
            if pnet == vnet:
                continue
            dx = max(abs(vx - cx) - hw, 0.0)
            dy = max(abs(vy - cy) - hh, 0.0)
            d = math.hypot(dx, dy) - VIA_SIZE / 2
            if d < CLR:
                errs.append(f"via {vnet[:24]} @({vx},{vy}) vs pad {ref}.{pname} [{pnet}]: {d:.3f}")
        for snet, sl, sw, sp1, sp2 in SEGS:
            if snet == vnet:
                continue
            d = seg_rect_dist(*sp1, *sp2, vx, vy, 0, 0) - VIA_SIZE / 2 - sw / 2
            if d < CLR:
                errs.append(f"via {vnet[:24]} @({vx},{vy}) vs seg {snet[:24]}: {d:.3f}")
    for i, (n1, (ax, ay)) in enumerate(VIAS):
        for n2, (bx, by) in VIAS[i + 1:]:
            if n1 == n2:
                continue
            d = math.hypot(ax - bx, ay - by) - VIA_SIZE
            if d < CLR:
                errs.append(f"via-via {n1[:20]}/{n2[:20]}: {d:.3f}")
    return errs


def emit():
    out = []
    for net, layer, w, (x1, y1), (x2, y2) in SEGS:
        out.append(
            f'\t(segment\n\t\t(start {x1} {y1})\n\t\t(end {x2} {y2})\n\t\t(width {w})'
            f'\n\t\t(locked yes)\n\t\t(layer "{layer}")\n\t\t(net "{net}")\n\t)'
        )
    for net, (x, y) in VIAS:
        out.append(
            f'\t(via\n\t\t(at {x} {y})\n\t\t(size {VIA_SIZE})\n\t\t(drill {VIA_DRILL})'
            f'\n\t\t(locked yes)\n\t\t(layers "F.Cu" "B.Cu")\n\t\t(net "{net}")\n\t)'
        )
    return "\n".join(out)


def main(target):
    src = open(target).read()
    if "(locked yes)" in src:
        print("doel bevat al gelockte pre-routes — niets gedaan")
        return
    pads = load_pads(src)
    global SEGS
    SEGS = to45(SEGS, pads)         # vrije hoeken -> H/V + 45
    SEGS = chamfer(SEGS, VIAS, pads)  # haakse hoeken -> 45-afschuining
    errs = validate(pads)
    if errs:
        print(f"{len(errs)} clearance-problemen — NIET geschreven:")
        for e in errs:
            print("  " + e)
        sys.exit(1)
    k = src.rstrip().rfind(")")
    patched = src[:k] + "\n" + emit() + "\n" + src[k:]
    assert patched.count("(") == patched.count(")")
    open(target, "w").write(patched)
    print(f"{len(SEGS)} gelockte segmenten + {len(VIAS)} vias geschreven naar {target}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else
         "pura-mainboard-0805-0.3.kicad/pura-mainboard-0805-0.3.kicad_pcb")
