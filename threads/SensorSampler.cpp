#include "SensorSampler.hpp"
#include "../common/SensorData.hpp"
#include "../sensors/PIR.hpp"
#include "../sensors/ads1115.hpp"
#include "../sensors/mq135.hpp"
#include "../sensors/bh1750.hpp"

#include <iostream>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <pigpio.h>

#define PIR_GPIO 17
#define TIMER_SIGNAL SIGRTMIN  // Unique signal for this thread

extern SensorData sensorData;

void* SensorSamplerThread(void* arg) {
    // Set up signal mask
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, TIMER_SIGNAL);
    pthread_sigmask(SIG_BLOCK, &mask, nullptr);

    // Setup pigpio
    if (gpioInitialise() < 0) {
        std::cerr << "pigpio init failed\n";
        pthread_exit(nullptr);
    }

    setupPIR(PIR_GPIO);

    if (!initADS1115("/dev/i2c-1", 0x48)) {
        std::cerr << "ADS1115 init failed\n";
        pthread_exit(nullptr);
    }

    int i2c_fd = open("/dev/i2c-1", O_RDWR);
    if (i2c_fd < 0 || !init_bh1750(i2c_fd)) {
        std::cerr << "BH1750 init failed\n";
        pthread_exit(nullptr);
    }

    // Create POSIX timer
    timer_t timerid;
    struct sigevent sev {};
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = TIMER_SIGNAL;
    timer_create(CLOCK_MONOTONIC, &sev, &timerid);

    // Start the timer with 500ms period
    struct itimerspec its {};
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = 500 * 1000000;  // 500ms first fire
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 500 * 1000000;  // repeat interval
    timer_settime(timerid, 0, &its, nullptr);

    // Main periodic loop
    while (true) {
        siginfo_t info;
        sigwaitinfo(&mask, &info);  // Wait for timer signal

        // Sample sensors
        bool motion = readPIR(PIR_GPIO);
        sensorData.motion.store(motion);

        int16_t raw_adc = readADS1115Raw(0);
        float voltage = convertToVoltage(raw_adc, 3.3);
        float ppm = calculatePPM(voltage, CLEAN_AIR_VOLTAGE);
        sensorData.gas_ppm.store(ppm);

        float lux = read_bh1750(i2c_fd);
        sensorData.lux.store(lux);

        std::cout << "[Sampler] Motion: " << motion
                  << ", Gas: " << ppm
                  << ", Lux: " << lux << std::endl;
    }

    gpioTerminate();
    close(i2c_fd);
    closeADS1115();
    pthread_exit(nullptr);
}
