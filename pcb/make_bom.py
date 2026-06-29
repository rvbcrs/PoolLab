"""Extract BOM (JLCPCB format) from circuit-json."""
import json, csv, subprocess
subprocess.run(["npx", "tsci", "export", "pura-mainboard-noroute.tsx",
                "-f", "circuit-json", "-o", "circuit.json"], check=True, capture_output=True)
data = json.load(open("circuit.json"))

# Map LCSC part numbers (from our imports)
LCSC = {
    "CH224K": "C970725",
    "LM2596S_5_0_UMW_": "C347421",
    "ADS1115IDGST": "C468683",
    "TB6612FNG_C_8_EL": "C141517",
    "B0505S_1WR3": "C7465178",
    "ADUM1250ARZ_RL7": "C13839",
    "TYPE_C_16PIN_2MD_073_": "C2765186",
}

rows = []  # [(Designator, Comment, Footprint, LCSC)]
for c in data:
    if c.get("type") != "source_component": continue
    name = c.get("name", "")
    mfg = c.get("manufacturer_part_number", "") or ""
    # Identify component type
    if c.get("ftype") == "simple_resistor":
        rows.append((name, f"{c.get('resistance',0)}Ω", "R0402", ""))
    elif c.get("ftype") == "simple_capacitor":
        rows.append((name, f"{c.get('capacitance','')}F", "C0402/0603/1210", ""))
    elif c.get("ftype") == "simple_inductor":
        rows.append((name, f"{c.get('inductance','')}H", "1210", ""))
    elif c.get("ftype") == "simple_diode":
        rows.append((name, "SS34", "SMA", ""))
    elif c.get("ftype") == "simple_led":
        rows.append((name, "LED", "0402", ""))
    elif c.get("ftype") == "simple_chip":
        lcsc = LCSC.get(mfg, "")
        rows.append((name, mfg, "", lcsc))
    elif c.get("ftype") == "simple_pin_header":
        pins = c.get("pin_count", 2)
        rows.append((name, f"PinHeader {pins}P 2.54mm", "", ""))

with open("pura-bom.csv", "w") as f:
    w = csv.writer(f)
    w.writerow(["Designator", "Comment", "Footprint", "LCSC Part #"])
    for r in rows: w.writerow(r)

print(f"✓ {len(rows)} rows → pura-bom.csv")
