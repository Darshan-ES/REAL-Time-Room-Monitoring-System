#ifndef SENSOR_DATA_HPP
#define SENSOR_DATA_HPP

#include <atomic>

struct SensorData {
    std::atomic<bool> motion;
    std::atomic<float> gas_ppm;
    std::atomic<float> lux;

    //  Add these baseline values
    std::atomic<float> baseline_ppm;
    std::atomic<float> baseline_lux;
};

#endif
