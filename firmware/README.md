# Motor 1 Commutation Firmware

Bare-metal (kein HAL, kein CubeMX) STM32F405RGT6-Firmware für die
sensorlose 6-Step-Trapez-Kommutierung von Motor 1 des 4-in-1-ESCs, nach
der in der Projektbeschreibung spezifizierten Hardware:

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
Build ohne Warnungen unter `-Wall -Wextra`.

## Vor dem ersten Einschalten / ersten Spin unbedingt prüfen

Diese Punkte konnte ich in dieser Session nicht anhand des offiziellen
6EDL7141-Datenblatts verifizieren und habe sie daher bewusst *nicht*
mit geratenen Werten befüllt, um das Risiko einer Fehlkonfiguration des
Gate-Treibers (Shoot-Through, falsche OCP-Schwelle) an echter Hardware
zu vermeiden:

1. **`edl7141_configure_3pwm_mode()` in `src/edl7141_spi.c`** ist ein
   Stub. Register-Adressen/Bitfelder für 3-PWM-Modus, Deadtime und
   OCP-Konfiguration müssen aus Abschnitt 8.2 ("Register Map") des
   6EDL7141-Datenblatts übernommen werden.
2. **Floating-Phase-Verhalten**: Der aktuelle Kommutierungs-Code geht
   davon aus, dass ein INHx-Pin, der dauerhaft auf 0 % Duty steht,
   tatsächlich zu einer hochohmigen (floatenden) Phase führt - nötig,
   damit die BEMF-Nulldurchgangs-Erkennung funktioniert. Ob der
   6EDL7141 im 3-PWM-Modus das wirklich so umsetzt (statt den Low-Side-
   FET der inaktiven Phase dauerhaft einzuschalten), ist im Datenblatt
   zu bestätigen.
3. **SPI-Rahmenformat**: 24-Bit-Frame (1 R/W-Bit + 7-Bit-Adresse +
   16-Bit-Daten), SPI-Modus 1 (CPOL=0, CPHA=1) - laut öffentlich
   verfügbaren Infineon-Unterlagen zum 6EDL-SPI-Link, aber nicht anhand
   des vollständigen Datenblatts gegengeprüft.
4. **Komparator-zu-Pin-Zuordnung**: Es wird angenommen Phase A → PB6,
   Phase B → PB7, Phase C → PB8 (siehe `PHASE_A_LINE` usw. in
   `commutation.c`). Gegen das Schaltplan-Netzlisting prüfen.
5. **HSE-Frequenz**: `HSE_VALUE_HZ` in `system_clock.h` ist auf 8 MHz
   gesetzt (Standardannahme) - an den tatsächlich bestückten Quarz
   anpassen, sonst stimmen PWM-Frequenz, eRPM-Berechnung und SPI-Timing
   nicht.

## Was fehlt (bewusst außerhalb des Scopes)

- **Open-Loop-Start/Alignment**: Aus dem Stillstand liefert kein Motor
  ein auswertbares BEMF-Signal. Diese Firmware kommutiert ausschließlich
  über erkannte Nulldurchgänge - für den Start wird zusätzlich eine
  Alignment- + Open-Loop-Ramp-Stufe benötigt, die hier nicht enthalten
  ist.
- DSHOT/PWM-Empfang, Telemetrie-UART, Strommessung (INA180A3),
  Fehlerbehandlung über nFAULT: nicht Teil dieser Abgabe.
- Motor 2-4 (identischer Aufbau auf TIM8/TIM... bzw. weiteren SPI/EXTI-
  Instanzen) sind nicht repliziert; die Struktur hier ist bewusst auf
  Motor 1 beschränkt.
