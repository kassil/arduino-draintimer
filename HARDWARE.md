# Hardware wiring notes

This document summarizes the wiring used by the firmware. Two physical form‑factors are covered:
- Arduino Uno / compatible (board with full headers)
- Smaller ATmega328P boards (Nano / Pro Mini / custom breadboarded ATmega328P)

Make all low‑voltage/logic grounds for MCU, sensors, driver input supplies in common.  Do not tie MCU ground to mains line, neutral, or ground.

---

## Summary / overview

- LCD and keypad each use its own PCF8574 I/O expander.
- An NTC thermistor senses temperature of the wash water.
- A block of six relays is controlled by a dedicated PCF8574 I/O expander (on the I2C bus).
- The pilot relay (safety/power control) is driven directly by the MCU on digital pin D7 (PD7).
- Expander slave addresses are set by jumpers on the PCF8574 boards and are matched in firmware.
- MCU analog reference is VCC (DEFAULT). Do NOT feed the divider top from AREF through a resistor.

---

## Thermistor voltage divider (ADC input)

- NTC thermistor is wired as a pull‑down (NTC to ground) in a voltage divider.
- ADC pin: A0 (analog 0)
- Top of divider: VCC (5.0V)
- Series resistor: 68 kΩ (this project uses 68k; adjust if you change thermistor)
- Thermistor: NTC (Prusa MK3 common: 100 kΩ @ 25°C, B ≈ 3950 K). Use measured R0/B if you calibrated.
- Bottom: GND

Schematic (ASCII)
```
 VCC (5V)
   |
 68k (Rseries)
   |
 ADC (A0)  ---> analogRead on MCU
   |
  NTC (thermistor)
   |
  GND
```

Notes:
- If ADC reads near full scale (~1023) the thermistor or its ground wire is open.
- If ADC reads near 0 the thermistor is shorted or ADC node is tied to ground.
- Measure the thermistor with a DMM (power off) to confirm nominal resistance at room temp.

---

## I²C peripherals (LCD + keypad)

- Bus: SDA / SCL
  - Uno / Nano / Pro Mini: SDA = A4, SCL = A5
  - Standalone ATmega328P pins: SDA = PC4, SCL = PC5
- Connect VCC and GND of the I²C modules to MCU 5V and GND.
- Typical LCD PCF8574 I²C address: 0x27 (confirm with an I²C scanner). The keypad's expander will usually have a different address — check your hardware.
- Multiple I²C devices (both PCF8574 expanders and any other peripherals) share the same SDA/SCL lines; ensure pull-ups are present and addresses do not conflict.

---

## Relay / pump / heater outputs

- Use driver transistors or MOSFETs to switch relays and pumps. Do NOT drive inductive loads directly from MCU pins.
- Relay board input pin(s) → MCU digital pin (e.g., D2..D7). Check `Relays` mapping in code for exact pins.
- Relay coil supply: separate 5V or suitable supply. Relays need flyback diodes or use driver modules that include them.
- Connect grounds: MCU GND must be connected to relay/pump/heater power GND.
- The pilot is connected directly to the microcontroller.  Loss of I2C communication causes the micro to open the pilot relay and stop the cycle in fail-safe mode.

Safety:
- Keep mains wiring isolated and follow safe wiring practices.
- Use proper fusing and isolation for heaters and pumps.

---

## Power / AREF / ADC reference

- MCU VCC: 5.0 V (measured VCC is used as ADC reference by default).
- Do not feed the divider top from AREF through a resistor. If you use `analogReference(EXTERNAL)` then AREF must be driven by a low‑impedance stable voltage (do not feed it through resistors).
- Decoupling: add 0.1 µF close to VCC/GND of the MCU and modules.

---

## Component values & thermistor info

- Example values used in firmware/tools:
  - SERIES_RESISTOR ≈ 68,000 Ω
  - THERMISTOR_R0 ≈ 100,000 Ω @ 25°C (Prusa MK3 type)
  - THERMISTOR_BETA ≈ 3950 K (typical; measure for accuracy)
- If your thermistor measures different (example measured values in this project: ~109.3k @ 21 °C and ~60k @ 35 °C), compute R0 and Beta and update firmware constants.

---

## Troubleshooting checklist

- ADC shows ~1023: open thermistor, loose connection to GND, or wrong wiring.
- ADC shows ~0: thermistor shorted to GND.
- Confirm series resistor value with a DMM.
- Confirm VCC (measured) equals the ADC reference expected by firmware.
- Print diagnostic: ADC raw, computed Vout, computed Rth, computed °C to compare against host tool predictions.
- If display shows odd glyphs for characters read from PROGMEM, place the PROGMEM array at file scope (not inside a function) and read with `pgm_read_byte()` and print using `lcd.print((char)code)` or `lcd.write(code)`.

---

## Canonical MCU pin mapping (applies to Uno, Nano/Pro Mini, or standalone ATmega328P)

- ADC0 / A0 — Thermistor voltage divider ADC input
- PC4 (SDA), PC5 (SCL) — I²C bus for LCD, keypad, and the relay PCF8574 expander
- Digital D7 (PD7) — Pilot relay control (driven directly by MCU, active low)
- Six relays (wash/drain/heater/dispenser/...) — driven by the dedicated PCF8574 I/O expander on the same I²C bus (addresses set by expanders' jumpers and matched in firmware)
- RESET, GND, VCC — power wiring as usual

Notes for standalone ATmega328P:
- Ensure AVCC is connected to VCC (through ferrite or decoupling) for ADC to work.
- AREF: leave unconnected (uses VCC) unless you explicitly set an external reference.

---

## Safety & final notes

- Use proper isolation and mains safety when switching heaters/pumps.
- Test sensor wiring with MCU unpowered (measure thermistor resistance directly).
- Use a single common ground for all supplies.
- Keep ADC reference and divider top on the same voltage domain (VCC) unless intentionally using an external reference.
