/**
 * Pura mainboard — uses real JLCPCB-imported components.
 * Pin selectors use numbers where the JLC label differs from my schematic
 * (e.g. TB6612 has VM1/VM2/VM3 instead of a single VM, LM2596 enable is "pin5").
 */
import { CH224K } from "./imports/CH224K"
import { LM2596S_5_0_UMW_ } from "./imports/LM2596S_5_0_UMW_"
import { B0505S_1WR3 } from "./imports/B0505S_1WR3"
import { ADUM1250ARZ_RL7 } from "./imports/ADUM1250ARZ_RL7"
import { ADS1115IDGST } from "./imports/ADS1115IDGST"
import { TB6612FNG_C_8_EL } from "./imports/TB6612FNG_C_8_EL"
import { TYPE_C_16PIN_2MD_073_ } from "./imports/TYPE_C_16PIN_2MD_073_"

export default () => (
  <board width="50mm" height="50mm" routingDisabled nominalTraceWidth="0.4mm">
    {/* Named nets so copperpour can flood them */}
    <net name="GND" />
    <net name="AGND" />
    {/* Anchor each net to one component pin — connectivity propagates to rest */}
    <trace from=".U2 > .GND"   to="net.GND" />
    <trace from=".U5 > .pin3"  to="net.AGND" />
    {/* Copper pours — GND on both layers, AGND on both layers */}
    <copperpour connectsTo="net.GND"  layer="bottom" clearance="0.2mm" />
    <copperpour connectsTo="net.GND"  layer="top"    clearance="0.2mm" />
    <copperpour connectsTo="net.AGND" layer="bottom" clearance="0.2mm" />
    <copperpour connectsTo="net.AGND" layer="top"    clearance="0.2mm" />

    {/* ─── USB-C + CH224K PD trigger ──────────────────────────────── */}
    {/* Row A (Y=22): USB-C top-left, facing outward (pcbRotation=180) */}
    <TYPE_C_16PIN_2MD_073_ name="J1" schX={-17.5} schY={21} pcbX={-17.5} pcbY={21} pcbRotation={180} />

    {/* Row B (Y=12): CH224K below J1, well clear of USB-C body */}
    <CH224K    name="U1"    schX={-9}  schY={12} pcbX={-12.9} pcbY={6.9} />
    <resistor  name="R_CC1"  resistance="5.1k"   footprint="0402" schX={-20} schY={14} pcbX={-20} pcbY={14} />
    <resistor  name="R_CC2"  resistance="5.1k"   footprint="0402" schX={-20} schY={12} pcbX={-20} pcbY={12} />
    <resistor  name="R_CFG2" resistance="0"      footprint="0402" schX={-10} schY={12} pcbX={-20.3} pcbY={8.3} />
    <capacitor name="C_VDD"  capacitance="100nF" footprint="0402" schX={-10} schY={14} pcbX={-20} pcbY={10} />

    {/* Row C (Y=20): LM2596 buck — positions from manual-edits */}
    <capacitor name="C_IN"  capacitance="220uF" footprint="1210" schX={-9.5} schY={17.5} pcbX={17.1} pcbY={15.5} />
    <LM2596S_5_0_UMW_ name="U2" schX={5} schY={19} pcbX={5} pcbY={18.3} />
    <capacitor name="C_OUT" capacitance="220uF" footprint="1210" schX={17} schY={20} pcbX={17} pcbY={20} />
    {/* L1 + D1 in their own row below U2 */}
    <diode    name="D1" footprint="sma" manufacturerPartNumber="SS34" schX={1}  schY={10} pcbX={-3.5} pcbY={10} />
    <inductor name="L1" inductance="33uH" footprint="1210"             schX={11} schY={10} pcbX={4.1} pcbY={9.8} />

    {/* Row D (Y≈2): Iso DC/DC + I²C isolator + ADS1115 — positions from manual-edits */}
    <capacitor name="C_DC_IN"  capacitance="10uF"  footprint="0603" schX={-22.7} schY={2} pcbX={10.5} pcbY={10.6} />
    <B0505S_1WR3 name="U5" schX={-15} schY={2} pcbX={-13} pcbY={-15.8} pcbRotation={180}/>
    <capacitor name="C_DC_OUT" capacitance="10uF"  footprint="0603" schX={-7}  schY={2} pcbX={-7}  pcbY={2} />
    <capacitor name="C_ISO1" capacitance="100nF"   footprint="0402" schX={-3}  schY={5} pcbX={-17.1} pcbY={-3.2} />
    <ADUM1250ARZ_RL7 name="U6" schX={0} schY={0} pcbX={-13.8} pcbY={-7.5} />
    <capacitor name="C_ISO2" capacitance="100nF"   footprint="0402" schX={3}   schY={5} pcbX={-7}   pcbY={4.6} />
    <resistor name="R_SDA" resistance="4.7k"       footprint="0402" schX={6}   schY={5} pcbX={-3.2} pcbY={-1.6} />
    <resistor name="R_SCL" resistance="4.7k"       footprint="0402" schX={9}   schY={5} pcbX={-3.2} pcbY={-3.8} />
    <ADS1115IDGST name="U3" schX={13} schY={2} pcbX={-1.6} pcbY={-9.7} />
    <capacitor name="C_ADS"  capacitance="100nF"   footprint="0402" schX={18}  schY={2} pcbX={-0.1} pcbY={-1.6} />
    <resistor name="R_ADDR" resistance="0"         footprint="0402" schX={13}  schY={-2} pcbX={-0.1} pcbY={-3.6} />

    {/* ─── TB6612 motor driver ─────────────────────────────────────── */}
    <capacitor name="C_VM"  capacitance="10uF"  footprint="0603" schX={-8} schY={-6} pcbX={17} pcbY={5} />
    <TB6612FNG_C_8_EL name="U4" schX={0} schY={-12} pcbX={10.8} pcbY={-0.7} />
    <capacitor name="C_VCC" capacitance="100nF" footprint="0402" schX={8} schY={-12} pcbX={16.6} pcbY={0.1} />

    {/* ─── Power-LEDs ──────────────────────────────────────────────── */}
    <resistor name="R_LED"  resistance="2.2k" footprint="0402" schX={-22} schY={-5} pcbX={9.7} pcbY={8.3} />
    <led      name="LED1"                    footprint="0402" color="green" schX={-22} schY={-7} pcbX={12.3} pcbY={8.2} />
    <resistor name="R_LED2" resistance="2.2k" footprint="0402" schX={-22} schY={-10} pcbX={-22} pcbY={-10} />
    <led      name="LED2"                    footprint="0402" color="cyan"  schX={-22} schY={-12} pcbX={-22} pcbY={-12} />

    {/* ─── Headers (right edge, vertical) ─────────────────────────── */}
    <pinheader name="J_ESP"   pinCount={10} layer="top" pcbRotation={90}
      pinLabels={["V5","GND","SDA","SCL","PWMA","PWMB","AIN1","AIN2","BIN1","BIN2"]}
      schX={22} schY={11} pcbX={22} pcbY={11}
    />
    <pinheader name="J_PROBE" pinCount={4}  layer="top" pcbRotation={90}
      pinLabels={["V5_ISO","AGND","PH_PO","ORP_PO"]}
      schX={22} schY={-10} pcbX={22} pcbY={-10}
    />
    {/* J_M1 + J_M2 moved into lower-middle; J_12V kept at right edge */}
    <pinheader name="J_M1"  pinCount={2} layer="top" pcbRotation={90} pinLabels={["MA1","MA2"]}    schX={16} schY={-18} pcbX={10.2} pcbY={-13.6} />
    <pinheader name="J_M2"  pinCount={2} layer="top" pcbRotation={90} pinLabels={["MB1","MB2"]}    schX={12} schY={-18} pcbX={14.1} pcbY={-13.6} />
    <pinheader name="J_12V" pinCount={2} layer="top" pcbRotation={90} pinLabels={["VIN12","GNDP"]} schX={22} schY={-19.5} pcbX={22} pcbY={-19.5} />

    {/* ============================================================ */}
    {/*  Nets / connections                                           */}
    {/* ============================================================ */}
    {/* USB-C TYPE_C 16-pin pin names: A1B12/B1A12=GND, A4B9/B4A9=VBUS,
        A5=CC2, B5=CC1, A6/B6=D+, A7/B7=D-, A8=SBU1, B8=SBU2, EH1/EH2=shield */}
    {/* USB-C → CH224K (CC + VBUS routing) */}
    <trace from=".J1 > .B5" to=".U1 > .CC1" /> {/* USB-C CC1 → CH224K CC1 */}
    <trace from=".J1 > .A5" to=".U1 > .CC2" /> {/* USB-C CC2 → CH224K CC2 */}
    <trace from=".U1 > .CC1"  to=".R_CC1 > .pin1" />
    <trace from=".U1 > .CC2"  to=".R_CC2 > .pin1" />
    <trace from=".R_CC1 > .pin2" to=".U1 > .GND" />
    <trace from=".R_CC2 > .pin2" to=".U1 > .GND" />
    <trace from=".U1 > .CFG2" to=".R_CFG2 > .pin1" />
    <trace from=".R_CFG2 > .pin2" to=".U1 > .GND" />
    <trace from=".U1 > .VDD"  to=".U1 > .VBUS" />
    <trace from=".U1 > .VDD"  to=".C_VDD > .pin1" />
    <trace from=".C_VDD > .pin2" to=".U1 > .GND" />

    {/* VBUS (12V from CH224K's VBUS pin) */}
    <trace from=".U1 > .VBUS" to=".J1 > .A4B9" /> {/* USB-C VBUS */}
    <trace from=".U1 > .VBUS" to=".J1 > .B4A9" /> {/* USB-C VBUS mirror */}
    <trace from=".U1 > .VBUS" to=".U2 > .VIN" />
    <trace from=".U1 > .VBUS" to=".U4 > .VM1" />
    <trace from=".U1 > .VBUS" to=".J_12V > .pin1" />
    <trace from=".C_IN > .pin1" to=".U2 > .VIN" />
    <trace from=".C_IN > .pin2" to=".U2 > .GND" />

    {/* LM2596 → 5V */}
    <trace from=".U2 > .OUT" to=".L1 > .pin1" />
    <trace from=".U2 > .OUT" to=".D1 > .pin2" />
    <trace from=".D1 > .pin1" to=".U2 > .GND" />
    <trace from=".L1 > .pin2" to=".U2 > .FB" />
    <trace from=".L1 > .pin2" to=".C_OUT > .pin1" />
    <trace from=".C_OUT > .pin2" to=".U2 > .GND" />
    <trace from=".U2 > .pin5" to=".U2 > .GND" /> {/* ON/OFF tied low = always-on */}
    <trace from=".U2 > .EP"   to=".U2 > .GND" /> {/* exposed pad → GND for thermal */}

    {/* 5V (non-iso) consumers */}
    <trace from=".L1 > .pin2" to=".U4 > .VCC" />
    <trace from=".L1 > .pin2" to=".J_ESP > .V5" />
    <trace from=".L1 > .pin2" to=".U6 > .VDD1" />
    <trace from=".L1 > .pin2" to=".U5 > .pin2" /> {/* B0505S +Vin */}
    <trace from=".L1 > .pin2" to=".R_LED > .pin1" />
    <trace from=".R_LED > .pin2" to=".LED1 > .pin1" />
    <trace from=".LED1 > .pin2" to=".U2 > .GND" />
    <trace from=".L1 > .pin2" to=".C_DC_IN > .pin1" />
    <trace from=".C_DC_IN > .pin2" to=".U2 > .GND" />

    {/* Common GND (non-iso) */}
    <trace from=".U2 > .GND" to=".U1 > .GND" />
    <trace from=".U2 > .GND" to=".J1 > .A1B12" />  {/* USB-C GND */}
    <trace from=".U2 > .GND" to=".J1 > .B1A12" />  {/* USB-C GND mirror */}
    <trace from=".U2 > .GND" to=".J1 > .EH1" />    {/* USB-C shell tab 1 */}
    <trace from=".U2 > .GND" to=".J1 > .EH2" />    {/* USB-C shell tab 2 */}
    <trace from=".U2 > .GND" to=".U4 > .GND" />
    <trace from=".U2 > .GND" to=".U4 > .PGND11" />
    <trace from=".U2 > .GND" to=".U4 > .PGND21" />
    <trace from=".U2 > .GND" to=".J_ESP > .GND" />
    <trace from=".U2 > .GND" to=".J_12V > .pin2" />
    <trace from=".U2 > .GND" to=".U6 > .GND1" />
    <trace from=".U2 > .GND" to=".U5 > .pin1" />  {/* B0505S -Vin */}
    <trace from=".C_ISO1 > .pin1" to=".U6 > .VDD1" />
    <trace from=".C_ISO1 > .pin2" to=".U6 > .GND1" />

    {/* ===== ISO BARRIER ===== */}

    {/* 5V_iso */}
    <trace from=".U5 > .pin4" to=".U6 > .VDD2" />   {/* B0505S +Vout */}
    <trace from=".U5 > .pin4" to=".U3 > .VDD" />
    <trace from=".U5 > .pin4" to=".J_PROBE > .V5_ISO" />
    <trace from=".U5 > .pin4" to=".R_LED2 > .pin1" />
    <trace from=".R_LED2 > .pin2" to=".LED2 > .pin1" />
    <trace from=".LED2 > .pin2" to=".U5 > .pin3" />  {/* -Vout = AGND */}
    <trace from=".U5 > .pin4" to=".C_DC_OUT > .pin1" />
    <trace from=".C_DC_OUT > .pin2" to=".U5 > .pin3" />
    <trace from=".U5 > .pin4" to=".R_SDA > .pin2" />
    <trace from=".U5 > .pin4" to=".R_SCL > .pin2" />

    {/* AGND (iso) */}
    <trace from=".U5 > .pin3" to=".U6 > .GND2" />
    <trace from=".U5 > .pin3" to=".U3 > .GND" />
    <trace from=".U5 > .pin3" to=".J_PROBE > .AGND" />
    <trace from=".C_ISO2 > .pin1" to=".U6 > .VDD2" />
    <trace from=".C_ISO2 > .pin2" to=".U6 > .GND2" />

    {/* ADS1115 + I²C */}
    <trace from=".U3 > .VDD" to=".C_ADS > .pin1" />
    <trace from=".C_ADS > .pin2" to=".U3 > .GND" />
    <trace from=".U3 > .ADDR" to=".R_ADDR > .pin1" />
    <trace from=".R_ADDR > .pin2" to=".U3 > .GND" />
    <trace from=".U3 > .SDA" to=".R_SDA > .pin1" />
    <trace from=".U3 > .SCL" to=".R_SCL > .pin1" />
    <trace from=".U3 > .SDA" to=".U6 > .SDA2" />
    <trace from=".U3 > .SCL" to=".U6 > .SCL2" />

    {/* Probe analog inputs */}
    <trace from=".U3 > .AIN0" to=".J_PROBE > .PH_PO" />
    <trace from=".U3 > .AIN1" to=".J_PROBE > .ORP_PO" />

    {/* ===== END ISO ===== */}

    {/* I²C ESP32 → ADuM1250 primary */}
    <trace from=".J_ESP > .SDA" to=".U6 > .SDA1" />
    <trace from=".J_ESP > .SCL" to=".U6 > .SCL1" />

    {/* TB6612 caps + control */}
    <trace from=".U4 > .VM1" to=".C_VM > .pin1" />
    <trace from=".C_VM > .pin2" to=".U4 > .PGND11" />
    <trace from=".U4 > .VCC" to=".C_VCC > .pin1" />
    <trace from=".C_VCC > .pin2" to=".U4 > .GND" />
    <trace from=".U4 > .VM1" to=".U4 > .VM2" />
    <trace from=".U4 > .VM1" to=".U4 > .VM3" />
    <trace from=".U4 > .PWMA" to=".J_ESP > .PWMA" />
    <trace from=".U4 > .PWMB" to=".J_ESP > .PWMB" />
    <trace from=".U4 > .AIN1" to=".J_ESP > .AIN1" />
    <trace from=".U4 > .AIN2" to=".J_ESP > .AIN2" />
    <trace from=".U4 > .BIN1" to=".J_ESP > .BIN1" />
    <trace from=".U4 > .BIN2" to=".J_ESP > .BIN2" />
    <trace from=".U4 > .STBY" to=".U4 > .VCC" />
    <trace from=".U4 > .AO11" to=".J_M1 > .MA1" />
    <trace from=".U4 > .AO21" to=".J_M1 > .MA2" />
    <trace from=".U4 > .BO11" to=".J_M2 > .MB1" />
    <trace from=".U4 > .BO21" to=".J_M2 > .MB2" />
    {/* Parallel AO/BO pads + PGND12/22 internally bridged in TB6612 — skip external bridges to reduce density */}
  </board>
)
