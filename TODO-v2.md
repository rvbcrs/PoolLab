# Pura v2.0 — TODO

Changes queued for the next PCB + firmware revision. Do NOT touch until
v1.0 is assembled, flashed, and validated in a real pool.

## PCB (`pcb/pura-mainboard-noroute.tsx` + `.kicad_pcb`)

- **J_PROBE header**: 4-pin → 6-pin
  - Current: `V5_ISO, AGND, PH_PO, ORP_PO`
  - v2.0: `V5_ISO, AGND, PH_PO, ORP_PO, TDS_PO, TURB_PO`
- Route ADS1115 `AIN2` → J_PROBE.TDS_PO
- Route ADS1115 `AIN3` → J_PROBE.TURB_PO
- Bump revision label on silkscreen: `v1.0` → `v2.0`
- Re-export gerbers + BOM + CPL (BOM unchanged, CPL only if header footprint moves)

## Firmware — new sensors

- Rename `AdsPhOrpSensor` → `AdsPoolSensor` (4 channels)
- Add telemetry fields: `float tdsPpm; float turbNtu; bool haveTds; bool haveTurb;`
- Storage: `getTdsFactor(0.5f)`, `getTurbZeroVolts(2.5f)` + setters
- `/settings` page: new **💧 Water Quality Sensors** section with calibration inputs
- WebUI `/`: add 2 metric cards (TDS ppm, TURB NTU) — grid stays 3-cols on desktop but wraps on mobile
- MQTT / Home Assistant: publish `pura/{id}/tds` and `pura/{id}/turb`
- Alert banners:
  - TDS > 600 ppm → "Water verdunnen (verversen)"
  - TURB > 10 NTU → "Algencheck — filter draaien, chloor-boost"

## Sensor hardware to source

- **TDS**: DFRobot Gravity Analog TDS Sensor (SEN0244) — ~€10
  - Output: 0-2.3V analog, 5V input
  - Range: 0-1000 ppm
- **TURB**: DFRobot Gravity Turbidity Sensor (SEN0189) — ~€8
  - Output: 0-4.5V analog, 5V input
  - Range: 0-3000 NTU

Both draw ~40mA @ 5V — well within B0505S iso-supply capacity.

## Things to double-check before v2.0

- v1.0 real-world validation: does the ADuM1250 + B0505S isolation actually
  eliminate the pool-water ground loop?
- Is 6-pin J_PROBE ergonomically OK, or split into 2× 3-pin?
- Do we want the CYA-reminder UI feature (manual monthly test tracking)?
- LM2596 thermal check under full load (all 4 sensors + motors + WiFi)

## Optional v2.0 nice-to-haves

- Salinity mode: reuse TDS sensor, different scaling factor
- 5V_ISO current limit / short protection (polyfuse or discrete)
- Better silkscreen: net names on pin headers (VBUS, GND, SDA etc.)
- Solder-jumper for `I2C_ADDR` on ADS (currently fixed via R_ADDR)
