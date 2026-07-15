# Pura mainboard — van bron naar productie-PCB (rev 0.3, 0805)

Flow: **`python3 build_flow.py`** — tscircuit (bron) → KiCad → freerouting (headless) → DRC-schoon board, volautomatisch. Daarna alleen nog silk/gerbers/bestellen.
Klaarstaand voor deze revisie: map **`pura-mainboard-0805-0.3.kicad/`** (met GND-zones + het **nieuwe rev 0.3 AGND-eiland** uit `port_zones.py`, al gevuld en DRC-gecheckt)
**Verificatiestatus: zie `VERIFICATIE-0.3.md`** — alle import-pinmappings, pin-coverage en custom lands zijn datasheet-geverifieerd.

## 0. Openstaande beslissingen (vóór je begint)

- [x] **C_OUT**: besloten — **220µF/16V SMD-elco (Ø6.3 V-chip)**, zit in de 0.3-bron. Polariteit: **pin 1 = +** naar de 5V-rail; de zwarte minstreep van de cup hoort aan de GND-kant.
- [ ] **Elco-maat meten**: TME-nummers zijn **EEE-1EA101XP / EEE-1CA221XP** (X = Ø6.3 case D8). De varianten *zonder* X zijn Ø8 en passen NIET op de lands. Meet ook de al via Ali bestelde 100 µF (moet Ø6.3 zijn).
- [ ] **LCSC-nummers invullen** in `pura-bom.csv` voor alle `TBD`-regels (0805-weerstanden/condensatoren/LEDs, 100µF 25V, 24k). Alleen nodig bij JLC-assembly; bij handmontage met AliExpress-onderdelen (Component Bench) niet.
- [x] Klopt de **24k op CFG1** met de CH224K-datasheet-tabel (6.8k=9V, **24k=12V**, 56k=15V)? ✔ Nagelezen én pin 9=CFG1 in de footprint geverifieerd tegen de pinout-figuur (§4.1) — zie `VERIFICATIE-0.3.md`.

## 1–4. Volautomatische flow: `build_flow.py`

Alles van tsx-bron tot geroute, DRC-schoon board is één commando:

```bash
cd pcb
python3 build_flow.py              # hele flow (incl. tsci-export)
python3 build_flow.py --no-export  # tsx ongewijzigd: sla de export over
```

De flow doet achtereenvolgens:

1. **tsci-export** (kicad_zip) + hernoemen naar de 0.3-bestandsnamen
2. **netklassen + rules** in de `.kicad_pro`: Default 0.2 mm (via 0.6/0.3), **POWER 0.6 mm** (via 0.8/0.4) voor 12V-VBUS, buck-switchnode, 5V-rail en de 4 motoruitgangen; min. track/clearance 0.15, edge 0.4, thermal spokes 1. (tsci schrijft schema-v1; de flow normaliseert naar v5, anders dropt KiCad de klasse-toewijzingen!)
3. **zones porten** (`port_zones.py`): GND prio 1/2 + AGND-eiland v2, tsci's auto-zones eruit
4. **gelockte pre-routes** (`preroute_iso.py`, met eigen clearance-validator): iso-netten binnen het eiland, AIN0/AIN1 (pH/ORP) door de gang onderlangs, en het complete AGND-stitching-raamwerk (pad-verankerd)
5. **3D-modellen** injecteren voor de custom footprints (L1 → `L_APV_ANR6045`, C_IN/C_OUT → `CP_Elec_6.3x7.7`)
6. **DSN bouwen** (tijdelijke kopie zonder zones, clearance 0.17 als afrondmarge)
7. **freerouting headless** (`~/Downloads/freerouting-1.7.0.jar`, `-mp 200`) — de gelockte sporen gaan als `type fix` mee en blijven staan
8. **ses importeren** + opschonen (AGND-sporen buiten het eiland weg, dubbelen weg) + zone-fill + **adaptieve AGND-stitching** (raster-vias, alleen waar beide lagen fill hebben)
9. **DRC-gate**: faalt hard op unconnected/clearance/shorts/edge; silk/tekst-cosmetica mag

> ⚠️ De freerouting-**GUI** (v2.2.4) crasht met `StackOverflowError` op deze DSN — daarom headless met de 1.7.0-jar. Niet meer handmatig DSN'en/importeren.
> ⚠️ Handmatige board-edits (silk, verplaatsingen) overleven een flow-run **niet** — de bron is de tsx. Backups: `*.goodhand.bak` e.d. in de kicad-map.
> Los valideren van de bron kan nog steeds: `npx tsci export pura-mainboard-noroute.tsx -f circuit-json -o circuit.json` (0 errors verwacht).

Na de flow: board openen in KiCad voor de visuele check + silkscreen/logo/versienummer **v0.3**, daarna stap 5.

## 5. Productiebestanden exporteren (KiCad)

**Gerbers** — `File > Fabrication Outputs > Gerbers`:
- Lagen: F.Cu, B.Cu, F.Mask, B.Mask, F.Silkscreen, B.Silkscreen, Edge.Cuts
- Opties: Protel-extensies **uit**, "subtract soldermask from silk" **aan**
- `Generate Drill Files`: Excellon, PTH+NPTH samengevoegd mag
- Alles in één map → zip: `pura-mainboard-0805-0.3-gerbers.zip`

**Assembly (alleen bij JLC-SMT):**
- BOM: `pura-bom.csv` (eerst TBD's → echte LCSC-nummers; kolommen Designator/Comment/Footprint/LCSC)
- CPL: `File > Fabrication Outputs > Component Placement (.pos)` → CSV met kolommen `Designator, Mid X, Mid Y, Layer, Rotation` (JLC-format, zie `pura-cpl-jlcpcb.csv` van rev 0.2 als voorbeeld)

> ⚠️ `make_bom.py` **overschrijft** de handmatig verrijkte `pura-bom.csv` — niet draaien tenzij je de LCSC-kolom opnieuw wilt invullen.

## 6. Bestellen bij JLCPCB

1. Gerber-zip uploaden → controleer de render (outline 50×50 mm, 2 lagen)
2. PCB-opties: 1.6 mm FR-4, 2 laags, HASL of ENIG, kleur naar smaak — verder defaults
3. **Met assembly**: SMT Assembly aanvinken (top side) → BOM + CPL uploaden → parts matchen (streef naar basic parts voor de passives; extended = ~$3 setup per uniek onderdeel) → **placement-preview**: rotaties van U1–U6, D1, LEDs en J1 visueel controleren en zo nodig draaien (tab-fouten hier = dode boards)
4. **Zonder assembly** (handmontage): alleen de gerber-zip; onderdelen bestel je via de Component Bench (`python3 ali_server.py`) — zie `pura-self-source.csv`
5. DFM-opmerkingen van JLC afwachten → bevestigen

## 7. Bring-up checklist (als de boards binnen zijn)

1. Visuele inspectie: bruggen bij U3 (VSSOP-10, 0.5 mm pitch) en U4 (SSOP-24)
2. Multimeter, **vóór** aansluiten: geen kortsluiting VBUS↔GND, 5V↔GND, AGND↔GND (moet ~∞, AGND↔GND alleen via de isolatie gescheiden)
3. USB-C PD-lader aansluiten → **VBUS = 12 V** meten (dit valideert de CFG1/24k-fix; 5 V = config fout)
4. 5V-rail meten na de LM2596 (4.9–5.1 V), rimpel checken als je een scoop hebt
5. ESP + firmware: I²C-scan moet de ADS1115 op **0x48** zien (valideert R_ADDR/soldeerbrug)
6. Pompen los testen op de TB6612-uitgangen (12 V blokgolf bij PWM)
7. pH/ORP-proefmeting via de geïsoleerde ingang; waardes vergelijken met buffer

## Bekende afwijkingen t.o.v. rev 0.2 (de geproduceerde boards)

| Wat | rev 0.2 (besteld) | rev 0.3 (deze flow) |
|---|---|---|
| Passives | 0402/0603 | **0805** |
| CH224K config | 0R op CFG2 → vraagt 5 V (bug!) | **24k op CFG1 → 12 V** |
| C_IN | C15008 = 6.3 V (te laag op 12 V-rail) | **100µF 25 V** |
| C_OUT | C15008 (MLCC) | **220µF 16 V SMD-elco** (Ø6.3) |
| L1 | 33µH **1210, 70mA** (RF-choke!) | **33µH CD54 power-inductor ~0.7A** (5.8×5.2mm) |
| CH224K-voeding | VDD **direct aan 12 V** (abs max 3,6 V — chip sterft!) | **1 kΩ serie + 1 µF** (R_VDD/C_VDD), VBUS-sense via **10 kΩ** (R_VSNS) |
| CC-pulldowns | externe 5,1 kΩ (fout: Rd zit intern) | **vervallen** — CC1/CC2 direct |
| DP/DM | zwevend | **kortgesloten** (PD-only, datasheet §5.5) |
| TB6612 VCC | 5 V (VIH=3,5 V > 3,3 V ESP: buiten spec) | **3,3 V via nieuwe V33-pin** op J_ESP (11-pins!) |
| ADuM1250 | ESP aan side 1 (VOL 0,9 V > ESP-drempel) | **omgedraaid**: ESP aan side 2 @3,3 V + pull-ups R_SDA2/R_SCL2 |
| B0505S | geen minimumbelasting (Vout kan ~6 V worden) | **R_BLD 330 Ω** op V5_ISO |

> ⚠️ **Rev 0.2-boards: de PD-sectie is NIET veilig te bodgen.** Naast de CFG-fout ligt daar CH224K-VDD **rechtstreeks aan 12 V** (abs max 3,6 V) — de chip gaat direct stuk zodra een PD-lader 12 V levert. Repareren vergt trace-snijden + 2 weerstanden + zijden-swap van de ADuM: niet realistisch. Gebruik rev 0.2 hooguit als soldeeroefening; bouw op rev 0.3.

**Let op de ESP-kabel:** J_ESP is nu **11-pins** — er is een **V33-ader bij** (3,3 V vanuit de ESP-module) die TB6612 en de ADuM-ESP-kant voedt. Zonder die ader doen de pompen en de I²C het niet.
