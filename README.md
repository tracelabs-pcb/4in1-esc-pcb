# 4-in-1 ESC — FPV Drone

**Status: PCB-Design abgeschlossen — unbestückt**  
**Tool:** KiCad | **Layer:** 4-Layer | **Hersteller-ready:** Ja

---

## Projektbeschreibung

4-in-1 Electronic Speed Controller für FPV-Drohnen auf 4S LiPo Basis.
Eigenentwicklung mit 4× unabhängigen Motorkanälen, SPI-konfigurierbaren
Gate-Treibern und integrierter Strommessung.

**Besonderheiten:**
- 4× Infineon 6EDL7141 Gate-Treiber (SPI, nFAULT, Kelvin-Sense)
- 12× Vishay SiZF660LDT MOSFETs (60V/110A/2mΩ, PowerPAIR)
- Strommessung: Bourns 0,5mΩ Shunt + INA180A3 (Gain 100)
- P-MOSFET Soft-Start + TVS Eingangsschutz (600W bidirektional)
- DSHOT300 Input, 3PWM-Modus, Telemetrie UART
- 37×43mm, 30,5×30,5mm M3 Mounting Pattern

---

## Hardware

| Komponente | Beschreibung |
|---|---|
| MCU | STM32F405RGT6 (168MHz Cortex-M4, LQFP-64) |
| Gate-Treiber | 4× Infineon 6EDL7141 (SPI-konfigurierbar) |
| MOSFETs | 12× Vishay SiZF660LDT (60V / 110A / 2mΩ) |
| Strommessung | INA180A3IDBVT + Bourns CSS2H Shunt (0,5mΩ) |
| Buck 5V | LMR33630BDDA (1,4MHz, 3A, Ultra-Low-EMI) |
| LDO 3V3 | TL1963A-33DCYR (1,5A, 40µVRMS) |
| Versorgung | 4S LiPo (14,8V / 16,8V max) |
| Stackup | Würth WE-4L-SI, 4-Layer, 1,6mm, 35µm |

---

## PCB

- 4-Layer: Signal / GND-Plane / Power / Signal
- L2 durchgehende GND-Plane, L3 Power-Plane
- Via-Stitching GND, Thermal-Via-Felder unter MOSFETs
- 0,5mm Clearance Gate-Leiterbahnen zu VBAT

---

## Ausschlüsse

Eigenentwicklung / Portfolioprojekt — kein Serienprodukt,
kein CE/EMV, keine Firmware.

---

## Dateien in diesem Repository

- `/kicad/` — KiCad Projektdateien
- `/screenshots/` — PCB- und Schematic-Ansichten

---

> Entwickelt von [Shawn Schneider](https://tracelabs-pcb.de)  
> — PCB-Design & Lötservice, Wolfsburg
