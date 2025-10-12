#include "door.h"
#include <Arduino.h>

// If the door switch physically opens the coil circuit, the pilot relay cannot
// energize even if the MCU or driver fails. That’s the correct primary
// layer — safety should be enforced by wiring, not by software.
// Software monitors the door state to pause the cycle.

// Debounce by requiring N consecutive identical samples. We use an 8-sample
// shift register and transition state when all bits agree.
// Configuration: fixed pin and polarity for door sensing.
static constexpr uint8_t DOOR_PIN = 8; // change if you wire to a different MCU pin

// Door switch is wired active-low: closed == LOW. We enable INPUT_PULLUP so
// an open switch reads HIGH (open == HIGH).

// Pilot GPIO: dedicated ATmega output, active low
// The pilot relay coil is driven by a transistor on the MCU pin.  The load
// side grounds the control coils of the other relays.
constexpr uint8_t PILOT_PIN = 7;

// Milliseconds to wait after enabling the pilot so the relay can energize.
constexpr uint8_t PILOT_SETTLE_MS = 20;

static uint8_t s_history = 0xFF; // start with ones (open)
static bool s_is_open = true; // debounced state: true == open

void door_tick()
{
    // With INPUT_PULLUP: HIGH == open, LOW == closed. Map HIGH->1 (open).
    uint8_t raw = digitalRead(DOOR_PIN) ? 1 : 0;
    // shift left, insert latest at LSB
    s_history = (uint8_t)((s_history << 1) | (raw & 1));
    // if lower 8 bits are all ones, consider open; if all zeros, closed
    if (s_history == 0xFF) {
        s_is_open = true;
    } else if (s_history == 0x00) {
        s_is_open = false;
    }
}

bool door_is_open()
{
    return s_is_open;
}

void pilot_init()
{
    pinMode(PILOT_PIN, OUTPUT);
    pilot_off();
    // Initialize door input on MCU digital pin. The input is expected to be
    // low when the door is closed.  Internal pull-up reads high when the door
    // switch opens.
    pinMode(DOOR_PIN, INPUT_PULLUP);
}

void pilot_on()
{
    digitalWrite(PILOT_PIN, LOW); // active low
    delay(PILOT_SETTLE_MS);
    // When pilot is off, door state relies on MCU internal pull-up.
    // To be safe, assume door open and reset debounce.
    s_history = 0xFF;
    s_is_open = true; // assume open until we see consecutive closes
}

void pilot_off()
{
    digitalWrite(PILOT_PIN, HIGH); // Pilot OFF (inactive)
    // When pilot is off, door state relies on MCU internal pull-up.
    // To be safe, assume door open and reset debounce.
    s_history = 0xFF;
    s_is_open = true; // assume open until we see consecutive closes
}
