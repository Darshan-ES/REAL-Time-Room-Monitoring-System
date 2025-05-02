#ifndef ADS1115_HPP
#define ADS1115_HPP

#include <cstdint>

bool initADS1115(const char *device = "/dev/i2c-1", int address = 0x48);
void closeADS1115();
int16_t readADS1115Raw(uint8_t channel);
float convertToVoltage(int16_t raw_adc, float vref = 3.3);

#endif
