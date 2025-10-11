#include "temperature.h"
#include <Arduino.h>

// --- Configuration constants (match previous heating.cpp values) ---
static constexpr float ADC_MAX_F = 1023.0f;
static constexpr float VREF = 5.0f;

// Thermistor parameters (defaults; override at compile-time if needed)
static constexpr float THERMISTOR_R0 = 100000.0f; // 100k at T0
static constexpr float THERMISTOR_BETA = 3950.0f; // Beta value
static constexpr float SERIES_RESISTOR = 68000.0f; // series resistor

// Hysteresis thresholds in Celsius used by heating logic
static constexpr float TEMP_THRESHOLD_C = 60.0f;
static constexpr float TEMP_HYSTERESIS_C = 2.0f;

// Sampling configuration copied from heating.cpp
static const uint8_t SAMPLE_COUNT = 64;

// Internal accumulation state
struct AnalogStats {
    uint32_t acc = 0;
    uint16_t mean = 0;
    uint8_t n = 0;
};

static AnalogStats g_stats;
static bool g_has_update = false;
static float g_celsius = NAN;

// Forward declares of helpers
static float resistance_to_celsius(float r);
static float adc_to_resistance(uint16_t adc);

void temperature_init() {
    g_stats.acc = 0;
    g_stats.mean = 0;
    g_stats.n = 0;
    g_has_update = false;
    g_celsius = NAN;
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
            g_celsius = NAN; // sensor fault (open/short)
        } else {
            float r = adc_to_resistance(g_stats.mean);
            g_celsius = resistance_to_celsius(r);
        }
        g_has_update = true;
    }
}

bool temperature_has_update() {
    return g_has_update;
}

bool temperature_get_adc_mean(uint16_t &out_adc) {
    if (!g_has_update) return false;
    out_adc = g_stats.mean;
    return true;
}

float temperature_get_celsius() {
    return g_celsius;
}

float temperature_adc_to_celsius(uint16_t adc) {
    if (adc == 0 || adc >= (uint16_t)ADC_MAX_F) return NAN;
    float r = adc_to_resistance(adc);
    return resistance_to_celsius(r);
}

uint16_t temperature_get_adc_on_threshold() {
    // Compute on/off ADC thresholds using the same math as previously used.
    // Convert temperature thresholds to ADC counts via exponential relation.
    auto temp_to_adc = [](float t_c) -> uint16_t {
        // T0 = 25C
        const float T0 = 25.0f + 273.15f;
        float t_kelvin = t_c + 273.15f;
        // resistance at temperature t via Beta equation
        float r_t = THERMISTOR_R0 * exp(THERMISTOR_BETA * (1.0f / t_kelvin - 1.0f / T0));
        // voltage divider: Vout = Vref * (Rtherm / (Rseries + Rtherm))
        float vout = VREF * (r_t / (SERIES_RESISTOR + r_t));
        // adc count
        return (uint16_t)((vout / VREF) * ADC_MAX_F + 0.5f);
    };

    float on_temp = TEMP_THRESHOLD_C - (TEMP_HYSTERESIS_C * 0.5f);
    return temp_to_adc(on_temp);
}

uint16_t temperature_get_adc_off_threshold() {
    auto temp_to_adc = [](float t_c) -> uint16_t {
        const float T0 = 25.0f + 273.15f;
        float t_kelvin = t_c + 273.15f;
        float r_t = THERMISTOR_R0 * exp(THERMISTOR_BETA * (1.0f / t_kelvin - 1.0f / T0));
        float vout = VREF * (r_t / (SERIES_RESISTOR + r_t));
        return (uint16_t)((vout / VREF) * ADC_MAX_F + 0.5f);
    };
    float off_temp = TEMP_THRESHOLD_C + (TEMP_HYSTERESIS_C * 0.5f);
    return temp_to_adc(off_temp);
}

// --- helpers ---
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
