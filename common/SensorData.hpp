#ifndef SENSORDATA_HPP
#define SENSORDATA_HPP

#include <atomic>

struct SensorData {
    std::atomic<bool> motion;
    std::atomic<float> gas_ppm;
    std::atomic<float> lux;
};

// Thresholds used for alerting or decisions
constexpr float GAS_THRESHOLD = 500.0f;   // ppm
constexpr float LUX_THRESHOLD = 30.0f;    // lux
constexpr float CLEAN_AIR_VOLTAGE = 0.46f;

#endif // SENSORDATA_HPP
