#include "heating.h"
#include "main.h"
#include <PCF8574.h>
#include <Arduino.h>

struct HeatingData
{
    uint8_t temp_n;
    uint32_t temp_mean;
} h_data;

constexpr uint32_t DISPENSE_DURATION_MS = 20000;
constexpr uint16_t TEMP_THRESHOLD = 512;

enum class DispenseState {
    Heating,
    Dispensing,
    Done
};


struct DispenseData
{
    DispenseState state : 2; // compiler wants 16??
    uint32_t end_time;
} dispense_data;

void heating_init()
{
    h_data.temp_n = 0;
    h_data.temp_mean = 0;
    relays.write(Relays::HeaterL, HIGH);
    relays.write(Relays::HeaterN, LOW);
}

void heating_loop()
{
    if (h_data.temp_n != (1<<10) - 1)
    {
        // 10-bit ADC: Given 32-bit accumulator, we bound samples such that
        // 1024 x samples < 2^32
        // log(1024) + log(samples) < 32
        // log(samples) < 22
        // samples < 4194304
        h_data.temp_n++;
        h_data.temp_mean += analogRead(A0);
    }
    else
    {
        h_data.temp_n = 0;
        h_data.temp_mean >>= 10;
        auto heaterCommand = (static_cast<uint16_t>(h_data.temp_mean) > TEMP_THRESHOLD) ? HIGH : LOW;
        relays.write(Relays::HeaterL, heaterCommand);
    }
}

void dispense_init()
{
    dispense_data.state = DispenseState::Heating;
    relays.write(Relays::Dispenser, HIGH);
}

void dispense_loop()
{
    if (dispense_data.state == DispenseState::Heating)
    {
        // Waiting for water to heat
        if ((relays.valueOut() & (1 << Relays::HeaterL)) == 0)
        {
            // Dispense
            dispense_data.state = DispenseState::Dispensing;
            dispense_data.end_time = millis() + DISPENSE_DURATION_MS;
            relays.write(Relays::Dispenser, LOW);
        }
    }
    else if (dispense_data.state == DispenseState::Dispensing)
    {
        // Dispensing
        if (millis() >= dispense_data.end_time)
        {
            // Finish dispensing
            dispense_data.state = DispenseState::Done;
            relays.write(Relays::Dispenser, HIGH);
        }
    }
}
