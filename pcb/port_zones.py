#!/usr/bin/env python3
"""Port de handgetekende copper-zones (GND prio1/2 + AGND-eiland prio3) van het
rev 0.2-board naar een vers tsci-geëxporteerd kicad_pcb.

Nodig na elke her-export van de kicad_zip, want tsci kent deze zones niet.
Verwijdert ook tsci's eigen prio-0 auto-zones: de AGND-variant daarvan legt
koper BUITEN het eiland en ondermijnt de isolatiebarrière.

Het AGND-eiland krijgt hier een polygon die bij de REV 0.3-plaatsing past
(de rev 0.2-vorm dekte 7 van de 10 AGND-pads niet meer na de verplaatsingen).

  python3 port_zones.py <doel.kicad_pcb>
"""
import re
import sys

SRC = "pura-mainboard-noroute.kicad/pura-mainboard-noroute.kicad_pcb"  # rev 0.2, bron van de zones
NETID = {"GND": "1", "AGND": "2"}  # net-nummers in tsci-exports

# AGND-eiland voor rev 0.3 (KiCad-coords, y omlaag), v2: de inkepingen tussen
# de blokken zijn dicht zodat de iso-routes (V5_ISO/I2C) er binnen blijven.
# Unie van: groot blok links+midden (76.2,103.6)-(102.6,121), noordvinger
# C_ISO2/C_DC_OUT (91.2,94.4)-(95.0,104.8), strook R_SDA..R_ADDR
# (94.3,100.6)-(102.6,103.6), gang onderlangs (76.2,116.6)-(120.4,121) en
# J_PROBE-kolom rechts (119.6,103.9)-(124.2,121). GND-pads die hierbinnen
# vallen (o.a. U5.1/2, U6 side-2) krijgen gewoon een clearance-gat in de fill.
ISLAND_PTS_03 = [
    (76.2, 103.6), (91.2, 103.6), (91.2, 94.4), (95.0, 94.4),
    (95.0, 100.6), (102.6, 100.6), (102.6, 116.6), (119.6, 116.6),
    (119.6, 103.9), (124.2, 103.9), (124.2, 121.0), (76.2, 121.0),
]


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


def strip_fills(z):
    while "(filled_polygon" in z:
        i, j = balanced(z, "(filled_polygon")[0]
        z = z[:i] + z[j:]
    return z


def replace_outline(zone, pts):
    """Vervang de (polygon (pts ...))-outline van een zone door nieuwe punten."""
    i, j = balanced(zone, "(polygon")[0]
    xy = "".join(f"\n\t\t\t\t\t(xy {x} {y})" for x, y in pts)
    return zone[:i] + "(polygon\n\t\t\t\t(pts" + xy + "\n\t\t\t\t)\n\t\t\t)" + zone[j:]


def main(target):
    old = open(SRC).read()
    zones = [old[i:j] for i, j in balanced(old, "(zone")]
    hand = [z for z in zones if re.search(r"\(priority [123]\)", z)]
    assert len(hand) == 3, f"verwacht 3 hand-zones in {SRC}, kreeg {len(hand)}"

    new = open(target).read()
    # ALLE bestaande zones weg: eerder geporte (idempotentie) én tsci's
    # prio-0 auto-zones (AGND-koper buiten het eiland = isolatie-lek).
    removed = 0
    while True:
        spans = balanced(new, "(zone")
        if not spans:
            break
        i, j = spans[0]
        new = new[:i] + new[j:]
        removed += 1

    ported = []
    for z in hand:
        z = strip_fills(z)
        name = re.search(r'\(net "([^"]+)"\)', z).group(1)
        if name == "AGND":  # eiland: rev 0.3-polygon i.p.v. de oude 0.2-vorm
            z = replace_outline(z, ISLAND_PTS_03)
        z = z.replace(f'(net "{name}")', f'(net {NETID[name]})\n\t\t(net_name "{name}")')
        ported.append(z)
    k = new.rstrip().rfind(")")
    patched = new[:k] + "\n" + "\n".join("  " + z for z in ported) + "\n" + new[k:]
    assert patched.count("(") == patched.count(")"), "haakjes uit balans"
    open(target, "w").write(patched)
    print(f"{removed} zones verwijderd, 3 zones geplaatst (eiland = 0.3-vorm) in {target}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "pura-mainboard-0805-0.3.kicad/pura-mainboard-0805-0.3.kicad_pcb")
