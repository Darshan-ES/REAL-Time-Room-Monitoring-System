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

    // Calculate ppm regardless of ratio to allow real-time dynamic response
    float ppm = pow(10.0f, (1.0f * log10(ratio) + log10(400.0f)));

    // Optional: clamp ppm to prevent unrealistic output
    if (ppm < 1.0f) ppm = 1.0f;
    if (ppm > 10000.0f) ppm = 10000.0f;

    return ppm;
}

