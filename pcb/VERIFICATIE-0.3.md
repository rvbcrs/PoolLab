# Rev 0.3 — verificatierapport (vóór bestellen)

Alles hieronder is **tegen de datasheets** geverifieerd (PDF's in de sessie-scratchpad
gedownload van WCH/TI/Toshiba/ADI/Mornsun/Sumida/Panasonic), niet uit het hoofd.
Datum: juli 2026.

## 1. Import-footprints: pin-mapping vs datasheet

Elke pin van elke geïmporteerde footprint vergeleken met de pinout-tekening/pintabel
in de datasheet. **Alle 7 kloppen:**

| Import | Package | Datasheet-check | Resultaat |
|---|---|---|---|
| CH224K | ESSOP-10 | pinout-figuur §4.1: 1=VDD, 2=CFG2, 3=CFG3, 4=DP, 5=DM, 6=CC2, 7=CC1, 8=VBUS, **9=CFG1**, 10=PG, pad=GND | ✅ match — de 24k zit écht op CFG1 |
| LM2596S TO-263-5 | KTT | pintabel: 1=VIN, 2=Output, 3=Ground, 4=Feedback, 5=ON/OFF | ✅ match; pin5→GND = regelaar AAN (datasheet-sanctioned) |
| ADS1115 | VSSOP-10 | pintabel: 1=ADDR, 2=ALERT/RDY, 3=GND, 4–7=AIN0–3, 8=VDD, 9=SDA, 10=SCL | ✅ match |
| TB6612FNG | SSOP-24 | pintabel p.2: 1/2=AO1 … 24=VM1 | ✅ match |
| ADuM1250 | SOIC-8 | 1=VDD1, 2=SDA1, 3=SCL1, 4=GND1, 5=GND2, 6=SCL2, 7=SDA2, 8=VDD2 | ✅ match |
| B0505S-1WR3 | SIP-4 | pintabel p.6: **1=GND, 2=Vin, 3=0V, 4=+Vo** | ✅ match — input dus niet omgekeerd |
| USB-C 16P | TYPE-C-31-M-12 | padvolgorde A1B12·A4B9·B8·A5·B7·A6·A7·B6·A8·B5·B4A9·B1A12, 0.5 mm pitch | ✅ match met standaard 16-pins layout |

Noot CC-pinnen: J1.A5→U1.CC2 en J1.B5→U1.CC1 zijn "gekruist" — elektrisch irrelevant,
de CH224K doet orientation-detect (een omgedraaide kabel doet exact dezelfde swap).

## 2. Pin-coverage: elke chippin verbonden of bewust NC

| Chip | Verbonden | Bewust NC (met datasheet-reden) |
|---|---|---|
| U1 CH224K | VDD (via 1k), VBUS (via 10k), CC1/CC2, CFG1 (24k), DP–DM kortgesloten, GND | CFG2/CFG3 (interne pull-down, single-resistor-config §6.1), PG (open-drain, optioneel) |
| U2 LM2596 | VIN, OUT, GND, FB, ON/OFF→GND (=aan), EP→GND | — |
| U3 ADS1115 | ADDR→0R→AGND (0x48), AIN0/1, VDD, GND, SDA, SCL | ALERT/RDY + AIN2/3: §9.1.4 "float unused analog inputs" ✔ |
| U4 TB6612 | alle inputs, VCC=3V3, STBY→VCC, VM1/2/3=12V, GND, PGND11/21, AO11/AO21/BO11/BO21 | AO12/22, BO12/22, PGND12/22 — gepaarde pinnen zitten per blokdiagram op **dezelfde interne node**; enkele pin volstaat bij ~0,3 A pompen (paar pas nodig richting 1 A+) |
| U5 B0505S | alle 4 | — |
| U6 ADuM1250 | alle 8 | — |
| J1 USB-C | VBUS ×2, GND ×2, CC ×2, shell EH1/EH2→GND | D±/SBU: PD-only per CH224K §5.5 (DP/DM juist NIET aan connector) |

Polariteit elco's: C_IN pin1(+)→12V, C_OUT pin1(+)→5V, beide pin2→GND ✅.

## 3. Fouten gevonden en gefixt in deze verificatieronde

| # | Fout | Impact | Fix |
|---|---|---|---|
| 1 | **AGND-eiland dekte de 0.3-plaatsing niet meer**: 7 van de 10 AGND-pads lagen buiten de (uit rev 0.2 geporte) polygon | iso-domein zonder AGND-koper; router had het "opgelost" met sporen dwars door het GND-domein → isolatiebarrière lek | nieuwe eiland-polygon in `port_zones.py` (`ISLAND_PTS_03`), 10/10 pads binnen, fill geverifieerd binnen de contour |
| 2 | **tsci-export genereert eigen prio-0 zones**, incl. een AGND-zone over het héle board met kopervlekjes búiten het eiland | AGND-koper buiten de isolatiebarrière | `port_zones.py` verwijdert nu álle bestaande zones vóór het plaatsen van de 3 hand-zones |
| 3 | **BOM-MPN's EEE-1EA101P / EEE-1CA221P zijn Ø8×10.2 (case F)**, niet Ø6.3 | elco's zouden niet op de getekende Ø6.3-lands passen | BOM → **EEE-1EA101XP / EEE-1CA221XP** (case D8, 6.3×7.7). ⚠️ Meet de al via Ali bestelde 100 µF: moet Ø6.3 zijn |
| 4 | **L1-land te smal**: getekend 2.0×2.6-pads, maar Sumida CD54 vraagt pads van 2.15×**5.5** (de voetjes zijn 5.2 mm breed), gap 1.7 | pads dekten <50% van de terminals: zwakke verbinding, scheeftrek-risico | tsx-footprint → 2×(2.15×5.5) op ±1.925, silk verruimd; export/DRC opnieuw gedraaid |
| 5 | SRR5028-330Y stond als TME-alternatief voor L1 | ander landpattern — past niet op het CD54-land | uit de BOM; CD54 is al via Ali besteld |

Land C_IN/C_OUT geverifieerd tegen Panasonic "Recommended land Pattern": D/D8 = a 1.8 / b 3.2 / c 1.6.
Getekend: pads 3.2 lang, gap 2.0 — terminal (binnenrand op 1,5 mm van hart) valt volledig op de pad ✅.

## 4. Status board-bestand

`pura-mainboard-0805-0.3.kicad/` is opnieuw geëxporteerd (na de L1-fix), zones geport
(nieuw eiland), gevuld en headless door DRC gehaald:

- 68 unconnected (verwacht: board is nog niet geroute)
- 4× starved_thermal → bekende instelling: Board Setup → Constraints → **min thermal spoke count 2→1**
- rest = silk/text/lib-cosmetica

## 5. Rest-risico's (klein, expliciet benoemd)

1. **Ali-onderdelen fysiek meten vóór solderen**: de 100 µF elco (Ø6.3?), de CD54 (voetjes 5.2 breed?), de USB-C (16P TYPE-C-31-M-12 — leg 'm op een print van de gerber 1:1).
2. **CH224K-kloon-kwaliteit** van Ali is niet te verifiëren op papier; de bring-up-stap "eerst VBUS=12V meten vóór de rest aansluiten" (PRODUCTIE.md §7) vangt dit af.
3. **Placement-kwaliteit** (geen fout, wel suboptimaal): C_ISO2 zit ~10 mm van U6 en C_DC_OUT ~7 mm van U5; decoupling werkt het best <3 mm. Functioneel prima op 100 kHz-I²C, maar als je toch schuift in KiCad: deze twee dichterbij.
4. De AIN-sporen (pH/ORP, hoogohmig) lopen straks door de eiland-gang onderlangs, langs J_M1/J_M2 (motoruitgangen). Route de motorsporen aan de bovenkant van de connectors, niet door de gang.

## 6. Go/no-go

**GO** voor bestellen — ná deze twee handmatige checks:
- [ ] gemeten: Ali-elco 100 µF is Ø6.3 (anders EEE-1EA101XP bij TME meebestellen)
- [ ] freerouting gedaan → refill (B) → DRC 0 echte fouten → gerbers

Schema voor eigen review: `pura-0.3-schema.svg`.

---

# Eindcontrole vóór bestellen (juli 2026, 2e ronde)

Volledige her-verificatie op het definitieve board. **Geen board-killende fouten.**

## Geverifieerd OK

| Check | Resultaat |
|---|---|
| DRC | 0 unconnected, 0 shorts, 0 clearance, 0 schematic-parity |
| Netlist | 102 traces, alles verbonden |
| Polariteit | D1/C_IN/C_OUT/LED1/LED2 geverifieerd tegen tscircuit's **eigen port-hints** (pin1=anode/pos). Catch-diode: anode→GND, kathode→switchnode = correct |
| Stroomcapaciteit | 0.6mm = 1,64A (IPC-2221, 1oz, 10°C rise); zwaarste net 12V ≈ 0,85A |
| Maakbaarheid JLC | sporen 0.2/0.25/0.6 (min 0.127) · via's 0.6/0.3 + 0.8/0.4, annular 0.15/0.20 (min 0.13) · boringen 0.7–1.0 (min 0.3) |
| Mechanisch | geen lijf-overlaps; L1↔C_OUT 0,55mm |
| Board | 50 × 50 mm |

## Gefixt in deze ronde

- **Silkscreen-polariteit** toegevoegd (was er niet — handsoldeer-risico): `+` bij C_IN/C_OUT/LED1/LED2 pin1, `K` bij D1's kathode. Posities gescand op >0.5mm vrije ruimte en geverifieerd dat elke markering het dichtst bij de júiste pin staat.
- **Versienummer** `Pura v0.3` op de silk.

## Bewust geaccepteerd (geen fout, wél een compromis)

**Isolatiekloof is 0,2 mm** tussen het GND- en AGND-domein (B0505S levert 1,5kV, ADuM1250 2,5kV). 12 primaire pads liggen binnen het AGND-eiland (o.a. U6's ESP-zijde, U5's ingang, R_SDA2/R_SCL2, J_12V) en er loopt 279mm GND-koper doorheen. De oorzaak is de plaatsing, die de domeinen verweeft.

**Bewust geaccepteerd**: het hele board is SELV (max 12V, USB-PD gevoed) — er is nergens hoogspanning. De hoofdfunctie van de isolatie (aardlus richting het zwembadwater verbreken, zodat de hoogohmige pH-meting klopt) wérkt: de DC-scheiding is intact. Wat je inlevert is robuustheid tegen zeldzame surge-/foutscenario's. Wil je dat alsnog: dan moet de plaatsing van de onderste boardhelft herzien worden zodat de barrière als schone lijn ónder U5/U6 loopt, met ≥1,5mm koper-vrije band.
