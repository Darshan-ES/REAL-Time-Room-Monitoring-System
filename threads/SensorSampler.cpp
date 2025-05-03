
#include "SensorSampler.hpp"
#include "../common/SensorData.hpp"
#include "../common/GpioMmap.hpp"
#include "../sensors/PIR.hpp"
#include "../sensors/ads1115.hpp"
#include "../sensors/mq135.hpp"
#include "../sensors/bh1750.hpp"

#include <iostream>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits>

#define PIR_GPIO 17
#define TIMER_SIGNAL SIGRTMIN

extern SensorData sensorData;

void* SensorSamplerThread(void* arg) {
    std::cout << "[Sampler] Thread started\n";

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, TIMER_SIGNAL);
    pthread_sigmask(SIG_BLOCK, &mask, nullptr);

    setupPIR(PIR_GPIO);
    std::cout << "[Sampler] PIR initialized\n";

    if (!initADS1115("/dev/i2c-1", 0x48)) {
        std::cerr << "[Sampler] ADS1115 init failed\n";
        pthread_exit(nullptr);
    }
    std::cout << "[Sampler] ADS1115 ready\n";

    int i2c_fd = open("/dev/i2c-3", O_RDWR);
    if (i2c_fd < 0 || !init_bh1750(i2c_fd)) {
        std::cerr << "[Sampler] BH1750 init failed\n";
        pthread_exit(nullptr);
    }
    std::cout << "[Sampler] BH1750 ready\n";

    // Calibration
    std::cout << "[Sampler] Starting calibration for 5 seconds...\n";
    float total_ppm = 0.0, total_lux = 0.0;
    int motion_count = 0, samples = 0;
    const int calibration_duration_ms = 7200;
    const int sample_interval_ms = 100;

    for (int elapsed = 0; elapsed < calibration_duration_ms; elapsed += sample_interval_ms) {
        usleep(sample_interval_ms * 1000);

        bool motion = readPIR(PIR_GPIO);
        int16_t raw_adc = readADS1115Raw(0);
        float voltage = convertToVoltage(raw_adc, 3.3);
        float ppm = calculatePPM(voltage, CLEAN_AIR_VOLTAGE);
        float lux = read_bh1750(i2c_fd);

        motion_count += motion ? 1 : 0;
        total_ppm += ppm;
        total_lux += lux;
        samples++;
    }

    float baseline_ppm = total_ppm / samples;
    float baseline_lux = total_lux / samples;
    float motion_ratio = (float)motion_count / samples;

    std::cout << "[Sampler] Calibration done.\n";
    std::cout << "[Baseline] Gas: " << baseline_ppm << " ppm, Lux: " << baseline_lux
              << ", Motion Rate: " << motion_ratio << std::endl;

    // POSIX timer setup
    timer_t timerid;
    struct sigevent sev{};
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = TIMER_SIGNAL;

    if (timer_create(CLOCK_MONOTONIC, &sev, &timerid) != 0) {
        perror("[Sampler] timer_create");
        pthread_exit(nullptr);
    }

    struct itimerspec its{};
    its.it_value.tv_nsec = 500 * 1000000;
    its.it_interval.tv_nsec = 500 * 1000000;

    if (timer_settime(timerid, 0, &its, nullptr) != 0) {
        perror("[Sampler] timer_settime");
        pthread_exit(nullptr);
    }

    std::cout << "[Sampler] Waiting for signal...\n";

    // WCET tracking variables
    timespec start{}, end{};
    double min_time = std::numeric_limits<double>::max();
    double max_time = 0.0;
    double total_time = 0.0;
    int sample_count = 0;

    while (true) {
        siginfo_t info;
        if (sigwaitinfo(&mask, &info) == -1) {
            perror("[Sampler] sigwaitinfo");
            break;
        }

        clock_gettime(CLOCK_MONOTONIC, &start);

        // Sensor read and store
        bool motion = readPIR(PIR_GPIO);
        int16_t raw_adc = readADS1115Raw(0);
        float voltage = convertToVoltage(raw_adc, 3.3);
        float ppm = calculatePPM(voltage, CLEAN_AIR_VOLTAGE);
        float lux = read_bh1750(i2c_fd);

        sensorData.motion.store(motion);
        sensorData.gas_ppm.store(ppm);
        sensorData.lux.store(lux);

        clock_gettime(CLOCK_MONOTONIC, &end);

        double exec_time = (end.tv_sec - start.tv_sec) * 1000.0 +
                           (end.tv_nsec - start.tv_nsec) / 1e6;

        min_time = std::min(min_time, exec_time);
        max_time = std::max(max_time, exec_time);
        total_time += exec_time;
        sample_count++;

        double avg_time = total_time / sample_count;
        double jitter = max_time - min_time;

        std::cout << "[Sampler] Motion: " << motion
                  << ", Gas: " << ppm
                  << ", Lux: " << lux << std::endl;

        if (sample_count % 10 == 0) {
            std::cout << "[WCET] Avg: " << avg_time << " ms, Min: " << min_time
                      << " ms, Max: " << max_time << " ms, Jitter: " << jitter << " ms\n";
        }
    }

    pthread_exit(nullptr);
}
