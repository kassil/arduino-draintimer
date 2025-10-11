// Reworked heating.cpp now delegates temperature sampling to temperature.cpp
#include "heating.h"
#include "main.h"
#include "utils.h"
#include "temperature.h"
#include <Arduino.h>

static constexpr unsigned long DISPENSE_DURATION_MS = 5000ul; // assumption: 5s dispense

uint8_t heating_loop(uint8_t heaterState)
{
    // Use ADC thresholds from temperature module and the averaged ADC value.
    uint16_t adc_mean;
    if (!temperature_get_adc_mean(adc_mean)) {
        // It takes awhile to average the temperature stream.  In that time
        // the heater and dispenser are off.
        return heaterState;
    }

    // sensor fault: ADC at extreme values treated as fault
    if (adc_mean == 0u || adc_mean >= 1023u) {
        return HIGH; // force heater off
    }

    if (heaterState == LOW) {
        // Turn off?
        uint16_t const adc_off = temperature_get_adc_off_threshold();
        return (adc_mean <= adc_off) ? HIGH : LOW;
    } else {
        // Turn on?
        uint16_t const adc_on = temperature_get_adc_on_threshold();
        return (adc_mean >= adc_on) ? LOW : HIGH;
    }
}

// Dispense state machine restored here.  We keep the state local to this
// compilation unit to avoid touching global headers.

enum class DispenseState : uint8_t {
    Heating,
    Dispensing,
    Done
};

struct DispenseData {
    DispenseState state;
    unsigned long end_time;
};

static DispenseData dispense_data = { DispenseState::Heating, 0 };

void dispense_init()
{
    dispense_data.state = DispenseState::Heating;
}

uint8_t dispense_loop()
{
    // We use the on threshold because we don't need it to be super hot
    uint16_t const adc_on = temperature_get_adc_on_threshold();
    uint16_t temp_adc;
    if (!temperature_get_adc_mean(temp_adc))
    {
        // It takes awhile to average the temperature stream.  In that time
        // the heater and dispenser are off.
        return HIGH;
    }
    if (dispense_data.state == DispenseState::Heating)
    {
        // Waiting for water to heat
        if (temp_adc <= adc_on)
        {
            // start dispensing
            dispense_data.state = DispenseState::Dispensing;
            dispense_data.end_time = millis() + DISPENSE_DURATION_MS;
            Serial.println(F("Dispense started"));
        }
        return HIGH; // Off
    }
    else if (dispense_data.state == DispenseState::Dispensing)
    {
        if (millis() >= dispense_data.end_time)
        {
            dispense_data.state = DispenseState::Done;
            Serial.println(F("Dispense finished"));
        }
        return LOW; // On
    }
    else // Done
    {
        return HIGH; // Off
    }
}

bool calc_analog_mean(uint8_t idx, uint16_t& analog_mean)
{
    if (idx != 0) return false;
    return temperature_get_adc_mean(analog_mean);
}
