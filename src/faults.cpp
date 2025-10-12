#include "faults.h"
#include "door.h"
#include "main.h"
#include "utils.h"
#include <Arduino.h>
#include <Keypad_I2C.h>
#include <LiquidCrystal_I2C.h>
#include <PCF8574.h>
#include <stdint.h>

static void faultmenu_loop();
static unsigned long last_blink;

void faultmenu_enter()
{
    // Cut pilot and all relays for safety, then enter the fault menu.
    pilot_off();
    relays.write8(0xFF); // All relays off (active low)
    digitalWrite(LED_BUILTIN, HIGH);
    last_blink = millis();

    lcd.setCursor(0, 3);
    lcd.print(F("# to continue"));

    loop_function = faultmenu_loop;
}

void faultmenu_loop()
{
    // Wait for user to acknowledge
    char const customKey = customKeypad.getKey();
    if (customKey == '#')
    {
        // Reset the device?
        mainmenu_enter();
    }

    // Blink the LED to indicate fault state
    unsigned long const now = millis();
    if (now - last_blink >= 500)
    {
        last_blink = now;
        digitalWrite(LED_BUILTIN, digitalRead(LED_BUILTIN) ? LOW : HIGH);
        // Cut the pilot off when the LED goes off
        pilot_off();
        relays.write8(0xFF); // All relays off (active low)
        //TODO Periodic UART
    }
}

bool faultmenu_is_active()
{
    return loop_function == faultmenu_loop;
}

//#include "faults.h"

// Placeholder for future fault handling code
// Currently, faults are handled directly in main.cpp and diags.cpp
// This file can be expanded to include more sophisticated fault management
// such as logging, error codes, or recovery procedures as needed.
// For now, it serves as a stub to keep the project structure organized.
// Example function to log a fault (currently does nothing)
void log_fault(const char* fault_description)
{
    // In a real implementation, this could write to EEPROM, send over serial, etc.
    // For now, we just ignore it.
    (void)fault_description; // Suppress unused parameter warning
}

