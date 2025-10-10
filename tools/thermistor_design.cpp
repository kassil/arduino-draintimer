#include <cmath>
#include <cstring>
#include <array>
#include <vector>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <string_view>

struct Candidate {
    double R;            // series resistor (ohms)
    int adc_low;         // ADC at hot end (or temp_max depending convention)
    int adc_high;        // ADC at cold end
    int delta;           // |adc_high - adc_low|
    double mid_offset;   // distance of midpoint from ADC midpoint
};

static double thermistor_resistance(double R0, double B, double T0_K, double tempC)
{
    double T = tempC + 273.15;
    double expv = std::exp(B * (1.0 / T - 1.0 / T0_K));
    return R0 * expv;
}

static int adc_from_res_div(double Rth, double Rseries, double ADC_MAX)
{
    double vout = (Rth / (Rseries + Rth)); // fraction of Vref
    double adc = vout * ADC_MAX;
    if (adc < 0) adc = 0;
    if (adc > ADC_MAX) adc = ADC_MAX;
    return static_cast<int>(std::lround(adc));
}

int main(int argc, char** argv)
{
    // Defaults (from your parameters)
    double const ADC_MAX = 1023.0;
    double const VREF = 5.0; // not used explicitly here (vout fraction only)
    double TEMP_MIN_C = -5.0;   // coldest / pot min mapping
    double TEMP_MAX_C = 85.0;   // hottest / pot max mapping

    auto print_usage = [&](const char *prog){
        std::cout << "Usage: " << prog << " [R0] [B] [T0_C] [TEMP_MIN_C TEMP_MAX_C]\n"
                  << "  R0       thermistor nominal resistance (ohms) at T0 (default 10000)\n"
                  << "  B        Beta parameter (default 3950)\n"
                  << "  T0_C     nominal temperature for R0 in °C (default 25)\n"
                  << "  TEMP_MIN_C TEMP_MAX_C  temperature range to optimize for (defaults -5 85)\n"
                  << "\nExample:\n  " << prog << " 10000 3950 25 -5 85\n";
    };

    if (argc >= 7 || (argc >= 2 && (std::strcmp(argv[1], "-h") == 0 || std::strcmp(argv[1], "--help") == 0))) {
        print_usage(argv[0]);
        return 0;
    }

    // thermistor parameters (tune as needed)
    double R0 = 100000.0;    // ohms at T0
    double B  = 3950.0;     // Beta
    double T0_C = 25.0;
    double T0_K = T0_C + 273.15;

    // candidate series resistors (ohms) — adjust list as desired
    constexpr std::array<double,10> CAND = {4700.0, 10000.0, 22000.0, 33000.0,
        39000.0, 47000.0, 68000.0, 82000.0, 100000.0, 220000.0};

    // simple command-line overrides (optional):
    // usage: select_series_resistor [R0] [B] [T0_C] [temp_min] [temp_max]
    if (argc >= 2) R0 = std::stod(argv[1]);
    if (argc >= 3) B  = std::stod(argv[2]);
    if (argc >= 4) T0_C = std::stod(argv[3]), T0_K = T0_C + 273.15;
    if (argc >= 6) {
        TEMP_MIN_C = std::stod(argv[4]);
        TEMP_MAX_C = std::stod(argv[5]);
    }

    std::vector<Candidate> results;
    results.reserve(CAND.size());

    for (double Rseries : CAND) {
        // compute ADC at cold and hot ends (for NTC thermistor: R increases as temp falls)
        double R_cold = thermistor_resistance(R0, B, T0_K, TEMP_MIN_C); // e.g. -5°C (cold)
        double R_hot  = thermistor_resistance(R0, B, T0_K, TEMP_MAX_C); // e.g. 85°C (hot)

        int adc_cold = adc_from_res_div(R_cold, Rseries, ADC_MAX);
        int adc_hot  = adc_from_res_div(R_hot,  Rseries, ADC_MAX);

        // convention: ADC increases as temperature falls (NTC) -> adc_cold >= adc_hot
        int delta = std::abs(adc_cold - adc_hot);
        double mid = (adc_cold + adc_hot) / 2.0;
        double adc_midpoint = ADC_MAX / 2.0;
        double mid_offset = std::abs(mid - adc_midpoint);

        results.push_back({Rseries, adc_hot, adc_cold, delta, mid_offset});
    }

    // sort: prefer largest delta, break ties by closest midpoint to ADC/2
    std::sort(results.begin(), results.end(), [&](const Candidate& a, const Candidate& b){
        if (a.delta != b.delta) return a.delta > b.delta;
        return a.mid_offset < b.mid_offset;
    });

    std::cout << "Thermistor R0=" << R0 << " B=" << B << " T0=" << T0_C << "C\n";
    std::cout << "Temp range: " << TEMP_MIN_C << " .. " << TEMP_MAX_C << " C\n\n";
    std::cout << std::left << std::setw(12) << "Rseries(Ω)"
              << std::right << std::setw(10) << "ADC_hot"
              << std::setw(10) << "ADC_cold"
              << std::setw(10) << "Delta"
              << std::setw(12) << "MidOff\n";
    std::cout << std::string(54, '-') << "\n";

    for (size_t i=0;i<results.size();++i) {
        auto &c = results[i];
        std::cout << std::left << std::setw(12) << static_cast<int>(c.R)
                  << std::right << std::setw(10) << c.adc_low
                  << std::setw(10) << c.adc_high
                  << std::setw(10) << c.delta
                  << std::setw(12) << std::fixed << std::setprecision(1) << c.mid_offset;
        if (i==0) std::cout << "  <-- recommended";
        std::cout << "\n";
    }

    std::cout << "\nRecommended series resistor: " << static_cast<int>(results.front().R) << " Ω\n";
    std::cout << "Expected ADC span: " << results.front().delta << " counts\n";
    std::cout << "Midpoint offset from ADC/2: " << results.front().mid_offset << " counts\n";

    return 0;
}
