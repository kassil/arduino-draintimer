#include "temperature.h"
#include <Arduino.h>

// --- Configuration constants ---
// Hysteresis thresholds in Celsius used by heating logic
static constexpr float TEMP_THRESHOLD_C = 60.0f; // target water temp, adjust as needed
static constexpr float TEMP_HYSTERESIS_C = 2.0f; // degrees C
static_assert(TEMP_HYSTERESIS_C >= 0.0 && TEMP_HYSTERESIS_C < 20.0, "TEMP_HYSTERESIS_C out of expected range");
static_assert(TEMP_THRESHOLD_C > -40.0 && TEMP_THRESHOLD_C < 130.0, "TEMP_THRESHOLD_C out of expected range");
static_assert(TEMP_HYSTERESIS_C < TEMP_THRESHOLD_C + 273.15, "Hysteresis must be less than threshold range");

static constexpr float ADC_MAX_F = 1023.0f;
static constexpr float VREF = 5.0f;  // ADC reference voltage (set to Vcc by default)
static_assert(ADC_MAX_F > 0.0, "ADC_MAX_F must be positive");
static_assert(VREF > 0.0, "VREF must be positive");

//#define ADC_POT_MODE 1

#ifndef ADC_POT_MODE
// Thermistor parameters (defaults; override at compile-time if needed)
static constexpr float THERMISTOR_R0 = 100000.0f; // 100k at T0
static constexpr float THERMISTOR_BETA = 3950.0f; // Beta param
static constexpr float THERMISTOR_T0_K = 25.0f + 273.15f; // @ R0
static constexpr float SERIES_RESISTOR = 68000.0f; // Series resistor
// Thermistor R0=100000 B=3950 T0=25C
// Temp range:  -5 C    85 C
// ADC range:    141     886
// - Beta must be in a plausible range for NTC thermistors
static_assert(SERIES_RESISTOR > 0.0, "SERIES_RESISTOR must be positive");
static_assert(THERMISTOR_R0 > 0.0, "THERMISTOR_R0 must be positive");
static_assert(THERMISTOR_BETA > 0.0 && THERMISTOR_BETA < 20000.0, "THERMISTOR_BETA out of expected range");

// Simple constexpr-friendly exponential approximation using a truncated Taylor
// series evaluated in Horner form.  Approximates e^x ≈ 1 + x + x^2/2! + ...
// Horner form reduces multiplies/adds and is constexpr-friendly.
static constexpr float exp_constexpr(float x) {
    return 1.0f + x*(1.0f + x*(0.5f + x*(0.16666667f + x*(0.041666667f + x*(0.008333333f + x*(0.001388889f))))));
}

// Temperature to ADC conversion
// Convert Celsius to ADC
// Given target temperature in C, compute the thermistor resistance via the Beta equation
// and return the expected ADC reading for the voltage divider.
// Steps: exponent = B*(1/T - 1/T0); R = R0 * e^{exponent};
// Vout = Vref * R/(R + Rseries); ADC = (Vout / Vref) * ADC_MAX_F.
static constexpr uint16_t temp_to_adc(float tempC) {
    return static_cast<uint16_t>(
        ((THERMISTOR_R0 * exp_constexpr(THERMISTOR_BETA * (1.0f / (tempC + 273.15f) - 1.0f / THERMISTOR_T0_K)))
         / ((THERMISTOR_R0 * exp_constexpr(THERMISTOR_BETA * (1.0f / (tempC + 273.15f) - 1.0f / THERMISTOR_T0_K))) + SERIES_RESISTOR))
        * ADC_MAX_F + 0.5f);
}

static float adc_to_resistance(uint16_t adc) {
    if (adc == 0) return INFINITY; // open circuit
    float frac = ((float)adc) / ADC_MAX_F; // 0..1
    // Vout = Vref * (Rth / (Rser + Rth)) -> frac = Rth/(Rser+Rth)
    // Rth = Rser * frac / (1 - frac)
    float denom = (1.0f - frac);
    if (denom <= 0.0f) return 0.0f; // shorted
    return SERIES_RESISTOR * (frac / denom);
}

static float resistance_to_celsius(float r) {
    if (!isfinite(r) || r <= 0.0f) return NAN;
    const float T0 = 25.0f + 273.15f;
    float temp_k = 1.0f / ((log(r / THERMISTOR_R0) / THERMISTOR_BETA) + (1.0f / T0));
    return temp_k - 273.15f;
}

float temperature_adc_to_celsius(uint16_t adc) {
    if (adc == 0 || adc >= (uint16_t)ADC_MAX_F) return NAN;
    return resistance_to_celsius(adc_to_resistance(adc));
}

#else
// In lieu of a thermistor, attach a three-wire potentiometer
// Linear mapping 0 -> 85, ADC_MAX_F -> -5°C
// The inverse relationship counts/temperature follows that of thermistor
constexpr float ADC_POT_TEMP_MIN_C = -5.0f;  // coldest temp
constexpr float ADC_POT_TEMP_MAX_C = 85.0f;  // hottest temp
static constexpr float ADC_POT_TEMP_RANGE = (ADC_POT_TEMP_MAX_C - ADC_POT_TEMP_MIN_C);

// Linear constexpr inverse: temp -> ADC (for compile-time thresholds)
static constexpr uint16_t temp_to_adc(float tempC) {
    return (tempC <= ADC_POT_TEMP_MIN_C) ? static_cast<uint16_t>(ADC_MAX_F)
         : (tempC >= ADC_POT_TEMP_MAX_C) ? 0
         : static_cast<uint16_t>(((ADC_POT_TEMP_MAX_C - tempC) / ADC_POT_TEMP_RANGE) * ADC_MAX_F + 0.5);
}

// POT_MODE: linear mapping ADC(0..ADC_MAX_F) -> Celsius (-10..110)
float temperature_adc_to_celsius(uint16_t adc)
{
    double a = (adc > static_cast<uint16_t>(ADC_MAX_F)) ? ADC_MAX_F : static_cast<double>(adc);
    double frac = a / ADC_MAX_F;
    return static_cast<float>(ADC_POT_TEMP_MAX_C - frac * ADC_POT_TEMP_RANGE);
}

#endif

// Compute ADC thresholds at compile time
// Turn off heating above this ADC reading
static constexpr uint16_t adc_off_threshold = static_cast<uint16_t>(temp_to_adc(TEMP_THRESHOLD_C + TEMP_HYSTERESIS_C));
// Turn on heating above this ADC reading
static constexpr uint16_t adc_on_threshold  = static_cast<uint16_t>(temp_to_adc(TEMP_THRESHOLD_C - TEMP_HYSTERESIS_C));

static const uint8_t SAMPLE_COUNT = 64;
// We want several properties guaranteed at compile time:
// - SAMPLE_COUNT must be a power of two (for shift/div)
// - TEMP thresholds must be in a reasonable range and hysteresis smaller than threshold
// These static_asserts help trap misconfiguration early and document expectations.
static_assert(SAMPLE_COUNT != 0, "SAMPLE_COUNT must be > 0");
static_assert((SAMPLE_COUNT & (SAMPLE_COUNT - 1)) == 0, "SAMPLE_COUNT must be a power of two");

// Internal accumulation state
struct AnalogStats {
    uint32_t acc = 0;
    uint16_t mean = 0;
    uint8_t n = 0;
    bool has_update = false;
    float celsius = NAN;
};

static AnalogStats g_stats;

void temperature_init() {
    g_stats.acc = 0;
    g_stats.mean = 0;
    g_stats.n = 0;
    g_stats.has_update = false;
    g_stats.celsius = NAN;
}

void temperature_tick() {
    // Read analog pin A0 (same as previous code). Keep this local so replacing
    // the pin later is straightforward.
    uint16_t sample = analogRead(A0);
    g_stats.acc += sample;
    g_stats.n++;
    if (g_stats.n >= SAMPLE_COUNT) {
        // compute average
        g_stats.mean = (uint16_t)(g_stats.acc / SAMPLE_COUNT);
        g_stats.acc = 0;
        g_stats.n = 0;
        // compute temperature if valid
        if (g_stats.mean == 0 || g_stats.mean >= (uint16_t)ADC_MAX_F) {
            g_stats.celsius = NAN; // sensor fault (open/short)
        } else {
            g_stats.celsius = temperature_adc_to_celsius(g_stats.mean);
        }
        g_stats.has_update = true;
    }
}

bool temperature_has_update() {
    return g_stats.has_update;
}

bool temperature_get_adc_mean(uint16_t &out_adc) {
    if (!g_stats.has_update) return false;
    out_adc = g_stats.mean;
    return true;
}

float temperature_get_celsius() {
    return g_stats.celsius;
}

uint16_t temperature_get_adc_on_threshold() {
    return adc_on_threshold;
}

uint16_t temperature_get_adc_off_threshold() {
    return adc_off_threshold;
}
