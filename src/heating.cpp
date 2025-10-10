#include "heating.h"
#include "main.h"
#include <PCF8574.h>
#include <Arduino.h>
#include <math.h>

struct HeatingData
{
    uint32_t temp_n;
    uint32_t temp_mean;
} h_data;

// Samples per averaged window (power of two makes shifts possible)
constexpr uint32_t SAMPLE_COUNT = 1024u;
// Build-time checks and documentation
// -------------------------------
// We want several properties guaranteed at compile time:
// - SAMPLE_COUNT must be non-zero and a power of two (we rely on shift/div properties elsewhere)
// - ADC_MAX/VREF and thermistor constants must be positive
// - TEMP thresholds must be in a reasonable range and hysteresis smaller than threshold
// - Beta must be in a plausible range for NTC thermistors
// These static_asserts help trap misconfiguration early and document expectations.
static_assert(SAMPLE_COUNT != 0, "SAMPLE_COUNT must be > 0");
static_assert((SAMPLE_COUNT & (SAMPLE_COUNT - 1)) == 0, "SAMPLE_COUNT must be a power of two");
// Thermistor / ADC configuration
// Assumptions:
// - Thermistor is connected as a divider between GND and VCC with the
//   fixed resistor (SERIES_RESISTOR) connected to VCC and the thermistor to GND.
//   Vout (A0) is between them. If your wiring differs, swap the math below or
//   adjust SERIES_RESISTOR accordingly.
constexpr double ADC_MAX = 1023.0;
constexpr double VREF = 5.0; // ADC reference voltage (set to Vcc by default)

// Change these values to match your thermistor and series resistor.
constexpr double SERIES_RESISTOR = 10000.0; // ohms
constexpr double THERMISTOR_R0 = 10000.0;   // ohms @ T0
constexpr double THERMISTOR_BETA = 3950.0;  // Beta parameter
constexpr double THERMISTOR_T0_K = 25.0 + 273.15;

// Temperature control thresholds (degrees Celsius)
constexpr double TEMP_THRESHOLD_C = 60.0; // target water temp, adjust as needed
constexpr double TEMP_HYSTERESIS_C = 2.0; // degrees C

// Static sanity checks
static_assert(ADC_MAX > 0.0, "ADC_MAX must be positive");
static_assert(VREF > 0.0, "VREF must be positive");
static_assert(SERIES_RESISTOR > 0.0, "SERIES_RESISTOR must be positive");
static_assert(THERMISTOR_R0 > 0.0, "THERMISTOR_R0 must be positive");
static_assert(THERMISTOR_BETA > 0.0 && THERMISTOR_BETA < 20000.0, "THERMISTOR_BETA out of expected range");
static_assert(TEMP_HYSTERESIS_C >= 0.0 && TEMP_HYSTERESIS_C < 20.0, "TEMP_HYSTERESIS_C out of expected range");
static_assert(TEMP_THRESHOLD_C > -40.0 && TEMP_THRESHOLD_C < 130.0, "TEMP_THRESHOLD_C out of expected range");
static_assert(TEMP_HYSTERESIS_C < TEMP_THRESHOLD_C + 273.15, "Hysteresis must be less than threshold range");

// Compute ADC thresholds at compile time using constexpr helpers so the MCU
// doesn't pay for expensive math at runtime and thresholds live in flash.

// Simple constexpr-friendly exponential approximation using a truncated Taylor
// series evaluated in Horner form.  Approximates e^x ≈ 1 + x + x^2/2! + ...
// Horner form reduces multiplies/adds and is constexpr-friendly.
static constexpr double exp_constexpr(double x) {
    return 1.0 + x*(1.0 + x*(0.5 + x*(0.16666666666666666 + x*(0.041666666666666664 + x*(0.008333333333333333 + x*(0.001388888888888889))))));
}

// Constexpr version of temp -> ADC conversion
// Steps: exponent = B*(1/T - 1/T0); R = R0 * e^{exponent};
// Vout = Vref * R/(R + Rseries); ADC = (Vout / Vref) * ADC_MAX.
static constexpr uint16_t temp_to_adc_constexpr(double tempC) {
    return static_cast<uint16_t>(
        ((THERMISTOR_R0 * exp_constexpr(THERMISTOR_BETA * (1.0 / (tempC + 273.15) - 1.0 / THERMISTOR_T0_K)))
         / ((THERMISTOR_R0 * exp_constexpr(THERMISTOR_BETA * (1.0 / (tempC + 273.15) - 1.0 / THERMISTOR_T0_K))) + SERIES_RESISTOR))
        * ADC_MAX + 0.5);
}

// Compile-time ADC thresholds
static constexpr uint16_t adc_off_threshold = temp_to_adc_constexpr(TEMP_THRESHOLD_C + TEMP_HYSTERESIS_C);
static constexpr uint16_t adc_on_threshold  = temp_to_adc_constexpr(TEMP_THRESHOLD_C - TEMP_HYSTERESIS_C);

// Dispense time (ms)
constexpr uint32_t DISPENSE_DURATION_MS = 20000;

enum class DispenseState {
    Heating,
    Dispensing,
    Done
};


struct DispenseData
{
    DispenseState state; 
    uint32_t end_time;
} dispense_data;

// Convert averaged ADC reading (0..1023) to thermistor resistance (ohms)
// From divider: Vout = Vref * Rth/(Rseries + Rth) => Rth = Rseries * (Vref/Vout - 1).
// With ADC proportional to Vout, this avoids floating-point division by computing via ADC scale.
static float adc_to_resistance(uint16_t adc)
{
    if (adc == 0)
        return INFINITY; // open or 0V -> invalid
    if (adc >= (uint16_t)ADC_MAX)
        adc = (uint16_t)(ADC_MAX - 1);

    float vout = (adc / ADC_MAX) * VREF;
    if (vout <= 0.0f)
        return INFINITY;

    // Voltage divider: Vout = Vref * R_therm / (R_series + R_therm)
    // => R_therm = R_series * (Vref / Vout - 1)
    float r = SERIES_RESISTOR * (VREF / vout - 1.0f);
    return r;
}

// Convert resistance (ohms) to Celsius using Beta parameter equation
// Beta model: 1/T = 1/T0 + (1/B) * ln(R/R0).  Compute T (Kelvin) then subtract 273.15.
static float resistance_to_celsius(float r)
{
    if (!isfinite(r) || r <= 0.0f)
        return -273.15f;
    float invT = 1.0f / THERMISTOR_T0_K + (1.0f / THERMISTOR_BETA) * logf(r / THERMISTOR_R0);
    float tK = 1.0f / invT;
    return tK - 273.15f;
}

// Public helper
float adc_to_celsius(uint16_t adc)
{
    float r = adc_to_resistance(adc);
    return resistance_to_celsius(r);
}

// Convert Celsius to ADC (inverse of adc_to_celsius path):
// Given target temperature in C, compute the thermistor resistance via the Beta equation
// and return the expected ADC reading for the voltage divider.
// (runtime temp_to_adc retained earlier was removed — we use constexpr thresholds)

void heating_init()
{
    // Reset rolling average
    h_data.temp_n = 0;
    h_data.temp_mean = 0;
    relays.write(Relays::HeaterL, HIGH);
    relays.write(Relays::HeaterN, HIGH);
}

uint8_t heating_loop(uint8_t heaterState)
{
    if (h_data.temp_n < SAMPLE_COUNT)
    {
        // Accumulate exactly SAMPLE_COUNT samples then evaluate.
        h_data.temp_mean += analogRead(A0);
        h_data.temp_n++;
        return heaterState; // no change yet
    }

    // We have SAMPLE_COUNT samples accumulated.
    uint16_t adc_mean = static_cast<uint16_t>(h_data.temp_mean / SAMPLE_COUNT);
    // Reset accumulator for next window
    h_data.temp_n = 0;
    h_data.temp_mean = 0;

    // Safety: if ADC is 0 (short to GND) or saturated at ADC full-scale (open circuit),
    // treat as sensor fault and force heater OFF to avoid unsafe operation.
    if (adc_mean == 0u || adc_mean == 1023u)
    {
        return HIGH; // turn off
    }

    // Compare using precomputed ADC thresholds (integer math, cheap)
    if (heaterState == LOW)
    {
        // Heater currently ON (active low). Turn OFF when measured ADC indicates temperature
        // has risen above threshold + hysteresis (i.e. ADC has dropped below adc_off_threshold).
        if (adc_mean <= adc_off_threshold)
            return HIGH; // turn off
    }
    else
    {
        // Heater currently OFF. Turn ON when ADC indicates temperature has fallen below threshold - hysteresis
        // (i.e. ADC is above adc_on_threshold because ADC increases as temperature decreases).
        if (adc_mean >= adc_on_threshold)
            return LOW; // turn on
    }
    return heaterState;  // No change
}

void dispense_init()
{
    dispense_data.state = DispenseState::Heating;
    relays.write(Relays::Dispenser, HIGH);
}

uint8_t dispense_loop(uint8_t relayState)
{
    if (dispense_data.state == DispenseState::Heating)
    {
        // Waiting for water to heat
        if ((relayState & (1 << Relays::HeaterL)) == 0)
        {
            // Dispense
            dispense_data.state = DispenseState::Dispensing;
            dispense_data.end_time = millis() + DISPENSE_DURATION_MS;
            relayState &= ~(1<<Relays::Dispenser); // Turn on
        }
    }
    else if (dispense_data.state == DispenseState::Dispensing)
    {
        // Dispensing
        if (millis() >= dispense_data.end_time)
        {
            // Finish dispensing
            dispense_data.state = DispenseState::Done;
            relayState |= (1<<Relays::Dispenser); // Turn off
        }
    }
    return relayState;
}
