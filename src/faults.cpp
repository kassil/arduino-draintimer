#include "faults.h"
#include "main.h"
#include "utils.h"
#include <Arduino.h>
#include <Keypad_I2C.h>
#include <LiquidCrystal_I2C.h>
#include <PCF8574.h>
#include <stdint.h>

static void faultmenu_loop();

void faultmenu_enter()
{
    lcd.clear();
    lcd.print(F("FAULT: WDOG"));
    lcd.setCursor(0, 1);
    lcd.print(F("Fail Safe Mode"));
    lcd.setCursor(0, 2);
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
    static unsigned long last_blink = 0;
    static bool led_on = false;
    unsigned long const now = millis();
    if (now - last_blink >= 500)
    {
        last_blink = now;
        led_on = !led_on;
        digitalWrite(LED_BUILTIN, led_on ? HIGH : LOW);
        if (led_on == false)
        {
            // Cut the pilot off when the LED goes off
            pilot_off();
            relays.write8(0xFF); // All relays off (active low)
            Serial.println(F("** WDOG"));
        }
    }
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

