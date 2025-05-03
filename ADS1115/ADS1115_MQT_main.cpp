#include <iostream>
#include <unistd.h>
#include "ads1115.hpp"
#include "mq135.hpp"

int main() {
    if (!initADS1115("/dev/i2c-1", 0x48)) {
        return 1;
    }

    float cleanAirVoltage = 0.46f;  // Updated calibrated clean-air voltage

    while (true) {
        int16_t raw = readADS1115Raw(0);  // Assuming MQ-135 connected to AIN0
        float voltage = convertToVoltage(raw, 3.3);  // VDD = 3.3V

        float ppm = calculatePPM(voltage, cleanAirVoltage);

        std::cout << "Air Quality: ~" << ppm << " ppm" << std::endl;

        sleep(2);
    }

    closeADS1115();
    return 0;
}


