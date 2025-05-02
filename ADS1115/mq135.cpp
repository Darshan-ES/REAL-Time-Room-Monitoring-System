#include "mq135.hpp"
#include <cmath>
#include <iostream>

// Approximates gas concentration using log-log scale
float calculatePPM(float voltage, float cleanAirVoltage) {
    if (cleanAirVoltage <= 0.0f || voltage <= 0.0f) {
        std::cerr << "Error: Invalid input voltage for MQ135.\n";
        return 0.0f;
    }

    float ratio = voltage / cleanAirVoltage;

    // If sensor voltage is higher than clean air, air is likely cleaner than baseline
    if (ratio > 1.0f) {
        std::cout << "Air is cleaner than baseline. Estimating baseline PPM.\n";
        return 400.0f;  // Assume clean air = ~400 ppm CO2
    }

    // Estimate PPM using empirical log-log fit (based on datasheet)
    float ppm = pow(10.0f, (-1.0f * log10(ratio) + 1.5f));
    return ppm;
}
