# Motor 1 Commutation Firmware

Bare-metal (kein HAL) STM32F405RGT6-Firmware für die sensorlose
6-Step-Trapez-Kommutierung von Motor 1 des 4-in-1-ESCs, nach der in der
Projektbeschreibung spezifizierten Hardware. Ordnerstruktur
(`Core/Inc`, `Core/Src`, `Core/Startup`, `STM32F405RGTX_FLASH.ld`) ist
bewusst identisch zu einem STM32CubeIDE-Projekt aufgebaut, siehe
"In STM32CubeIDE einbinden" unten.

- PWM: TIM1 CH1/CH2/CH3 auf PA8 (INHC), PA9 (INHB), PA10 (INHA), 20 kHz.
- Zero-Cross: EXTI6/7/8 auf PB6/PB7/PB8 (LM2901-Komparatorausgänge),
  beide Flanken aktiv, Filterung auf die für den aktuellen Kommutierungs-
  schritt erwartete Flankenrichtung.
- Timebase: TIM2 als freilaufender 1 MHz-Zähler zum Zeitstempeln der
  Flanken (32-bit, kein Overflow-Handling nötig dank unsigned-Subtraktion).
- Kommutierungs-Delay: TIM3 im One-Pulse-Modus, plant "jetzt + halbe
  gemessene Schrittdauer" (= 30 elektrische Grad) und löst dann den
  nächsten Schritt aus.
- SPI1 auf PB3/PB4/PB5, PB12 als software-gesteuertes nCS für den
  6EDL7141.

## Bauen

```
cd firmware
make
```

Erzeugt `build/motor1_commutation.elf` und `.bin`. Getestet mit
`arm-none-eabi-gcc` 13.2 (Ubuntu `gcc-arm-none-eabi` Paket), sauberer
Build ohne Warnungen unter `-Wall -Wextra`. Dieses `make` ist optional
(Kommandozeilen-Sanity-Check) - für die STM32CubeIDE gilt der nächste
Abschnitt.

## In STM32CubeIDE einbinden

1. **Neues Projekt anlegen**: File → New → STM32 Project. MCU/Board
   Selector → MCU-Suche "STM32F405RGTx" → Next. Projektnamen vergeben
   → Finish.
2. Es öffnet sich der CubeMX-Pinout-Editor mit der Frage nach
   Initialisierung der Peripherie - **kein** Peripheral aktivieren
   (alles auf Default lassen), einfach direkt oben links auf
   **"Project" → "Generate Code"** (oder das Zahnrad-Icon) klicken.
   CubeIDE legt jetzt ein lauffähiges Grundgerüst mit `Core/Inc`,
   `Core/Src`, `Core/Startup`, `Drivers/` und einer `.ld`-Datei an.
3. Im generierten Projekt **diese Dateien löschen/ersetzen** (sie
   kollidieren sonst mit unseren, weil beide `main()`,
   `Reset_Handler` bzw. die Interrupt-Vektortabelle definieren):
   - `Core/Src/main.c` → löschen, durch unser `Core/Src/main.c` ersetzen
   - `Core/Src/stm32f4xx_it.c` → löschen (unsere Interrupt-Handler
     stecken in `commutation.c`)
   - `Core/Src/system_stm32f4xx.c` → löschen (unser
     `system_clock.c` übernimmt das)
   - `Core/Src/stm32f4xx_hal_msp.c` → löschen (wird nicht gebraucht,
     wir nutzen kein HAL)
   - `Core/Startup/startup_stm32f4*.s` → löschen, durch unsere
     `Core/Startup/startup_stm32f405xx.s` ersetzen
   - Die generierte `STM32F405RGTX_FLASH.ld` (liegt im Projekt-Root)
     durch unsere gleichnamige Datei ersetzen
4. Alle unsere übrigen Dateien aus `Core/Inc/*.h` und `Core/Src/*.c`
   in die entsprechenden Ordner des CubeIDE-Projekts kopieren
   (Drag&Drop im Project Explorer, oder im Dateisystem kopieren und in
   CubeIDE "Refresh" (F5) drücken).
5. **`Drivers/`-Ordner vom Build ausschließen** (wird nicht benötigt,
   da kein HAL verwendet wird): Rechtsklick auf `Drivers` → "Resource
   Configurations" → "Exclude from Build" → beide Konfigurationen
   (Debug/Release) anhaken. Optional, spart nur Kompilierzeit.
6. Projekt bauen: Hammer-Symbol oder Project → Build Project. Sollte
   ohne Fehler durchlaufen (identischer Code wie beim `make`-Test oben).
7. Flashen: Board per ST-LINK anschließen, grünen "Debug"- oder
   "Run"-Button drücken.

## 6EDL7141: 6PWM-Modus, nicht 3PWM (wichtig, siehe unten)

Verifiziert anhand des offiziellen Infineon-Datenblatts (Rev. 1.02,
2021-09-27), das der Nutzer bereitgestellt hat (Abschnitte 3.2, 7.1.2,
8.1, 8.2).

Die Platine hat wie vorgesehen nur 3 PWM-Leitungen von der MCU zu den
Gate-Treibern (INHA/INHB/INHC), INLA/INLB/INLC liegen fest auf GND -
daran ändert sich nichts. Der Punkt betrifft ausschließlich das
SPI-Register `PWM_CFG` (Adresse 0x13, Bitfeld `PWM_MODE`), das
festlegt, wie der Chip intern auf INHx/INLx reagiert:

- **3PWM-Modus** (`PWM_MODE=b001`): laut Datenblatt-Wahrheitstabelle
  (Table 9) ignoriert der Chip INLx komplett und schaltet die Low-Side
  automatisch komplementär zu INHx. Bei INHx=0 wird GLx **aktiv auf
  HIGH** gesetzt (Low-Side-FET an) - die "floatende" Phase wird damit
  hart auf GND gezogen statt zu floaten. Das zerstört die
  BEMF-Nulldurchgangs-Messung über LM2901/VSTAR komplett.
- **6PWM-Modus** (`PWM_MODE=b000`, Reset-Default): laut Table 8 wertet
  der Chip INHx und INLx unabhängig aus. Mit INLx fest auf GND ergibt
  INHx=0 → GHx=LOW, GLx=LOW, SHx=High-Z - die Phase floatet tatsächlich.
  Das ist exakt das Verhalten, das die Komparatorschaltung braucht.

`edl7141_configure_pwm_mode()` in `src/edl7141_spi.c` schreibt deshalb
explizit `PWM_CFG = 0x0000` (6PWM). Das ist zwar auch der
Werksreset-Wert, wird hier aber trotzdem aktiv gesetzt, falls OTP das
je abweichend programmiert. **Diesen Wert nicht auf 3PWM ändern.**

`PWM_MODE` ist laut Register-Programmierbarkeits-Tabelle (Table 20)
nur im "Standby"-Zustand wirksam, d.h. der Schreibzugriff muss
passieren, *während* `EN_DRV` noch low ist.

## CE / EN_DRV Pins (geklärt)

- **CE**: liegt fest über Pull-up auf HIGH (kein MCU-Zutun nötig,
  Versorgungs-Sequenz startet von allein).
- **EN_DRV**: auf **PB2**, als GPIO-Ausgang. `gpio_config_init()` setzt
  PB2 initial LOW (Treiberstufe aus). `main.c` schreibt zuerst
  `PWM_CFG` per SPI (muss laut Table 20 bei EN_DRV=low passieren),
  dann `gpio_en_drv_set(1)`, um die Gate-Treiber-Stufe scharf zu
  schalten.

## Open-Loop-Start (Motor dreht sich)

`commutation_open_loop_start()` (aufgerufen in `main.c`) macht Folgendes,
komplett ohne BEMF/Komparator - funktioniert unabhängig davon, ob die
Komparator-zu-Phase-Zuordnung (siehe unten) stimmt:

1. **Alignment**: hält Schritt 0 für 500 ms mit 15 % Duty, damit der
   Rotor in eine bekannte Position einrastet.
2. **Rampe**: 120 Kommutierungsschritte (= 20 elektrische Umdrehungen),
   Schrittzeit linear von 20 ms auf 3 ms verkürzt (= beschleunigt).
3. **Cruise**: läuft danach für immer mit 3 ms/Schritt und 25 % Duty
   weiter (per TIM3-Interrupt, blockiert `main()` nicht mehr).

Alle Werte (`ALIGN_DUTY_TICKS`, `ALIGN_TIME_US`, `RAMP_START/END_STEP_US`,
`RAMP_STEPS`, `RUN_DUTY_TICKS`) stehen als `#define` oben in `main.c`
und sind **konservative Startwerte, keine für euren Motor/Propeller
berechneten Werte**. Falls es beim ersten Test nicht klappt:

- **Motor bewegt sich gar nicht / brummt nur**: `ALIGN_DUTY_TICKS`
  bzw. `RUN_DUTY_TICKS` erhöhen (zu wenig Drehmoment).
- **Motor ruckelt/rastet aus statt rund hochzulaufen**: `RAMP_START_STEP_US`
  erhöhen (langsamerer Start) und/oder `RAMP_STEPS` erhöhen (sanftere
  Beschleunigung) - der Rotor kann dem elektrischen Feld nicht folgen.
- **Motor dreht, aber sehr langsam/schwach am Ende**: `RAMP_END_STEP_US`
  verkleinern (höhere Zieldrehzahl) und/oder `RUN_DUTY_TICKS` erhöhen.

`commutation_handoff_to_closed_loop()` existiert, um danach auf
sensorlose BEMF-Kommutierung umzuschalten, wird aber in `main.c`
aktuell nicht aufgerufen - erst testen, ob der Open-Loop-Teil zuverlässig
dreht, dann die geschlossene Regelung separat ausprobieren (siehe
nächster Punkt).

## Weitere Punkte, insbesondere vor dem Test der geschlossenen Regelung

1. **SPI-Rahmenformat**: 24-Bit-Frame (1 R/W-Bit + 7-Bit-Adresse +
   16-Bit-Daten), SPI-Modus 1 (CPOL=0, CPHA=1) - direkt anhand des
   vollständigen Datenblatts (Abschnitt 7.1.2, inkl. dessen eigenem
   Rechenbeispiel) verifiziert und bestätigt.
2. **Komparator-zu-Phase-Zuordnung**: bestätigt ist, dass PB6/PB7/PB8 =
   LM2901-Komparatorausgänge 1/2/3 sind (EXTI6/7/8). **Nicht bestätigt**
   ist, welche Motorphase (A/B/C bzw. U/V/W) auf IN+ von Komparator 1
   vs. 2 vs. 3 liegt - die Firmware nimmt 1→A, 2→B, 3→C an
   (`PHASE_A_LINE` usw. in `commutation.c`). Das betrifft nur
   `commutation_handoff_to_closed_loop()`, nicht den Open-Loop-Start.
3. **HSE-Frequenz**: 8 MHz bestätigt, passt zum Default in
   `system_clock.h`.
4. **IDRIVE_CFG / DT_CFG / OCP-Register**: bleiben auf Werksreset-Werten
   stehen (nicht explizit programmiert). Gate-Treiberstrom und
   Überstromschwellen ggf. auf die SiZF660LDT-MOSFETs abstimmen -
   dafür Abschnitt 8.2 (Register `IDRIVE_CFG` 0x17, `CSAMP_CFG` 0x1D)
   im Datenblatt konsultieren.

## Was fehlt (bewusst außerhalb des Scopes)

- DSHOT/PWM-Empfang, Telemetrie-UART, Strommessung (INA180A3),
  Fehlerbehandlung über nFAULT: nicht Teil dieser Abgabe.
- Motor 2-4 (identischer Aufbau auf TIM8/TIM... bzw. weiteren SPI/EXTI-
  Instanzen) sind nicht repliziert; die Struktur hier ist bewusst auf
  Motor 1 beschränkt.
