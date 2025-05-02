#include <iostream>
#include <unistd.h>
#include "ads1115.hpp"

int main() {
    if (!initADS1115()) {
        return 1;
    }

    while (true) {
        int16_t raw = readADS1115Raw(0);  // Read AIN0
        float voltage = convertToVoltage(raw, 3.3); //0-3.3v range

        std::cout << "AIN0 Raw: " << raw << " | Voltage: " << voltage << " V\n";
        sleep(1);
    }

    closeADS1115();
    return 0;
}
