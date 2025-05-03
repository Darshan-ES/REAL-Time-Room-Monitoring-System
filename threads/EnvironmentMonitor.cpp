#include "EnvironmentMonitor.hpp"
#include "../common/SensorData.hpp"

#include <iostream>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#define TIMER_SIGNAL (SIGRTMIN + 1)

extern SensorData sensorData;

void* EnvironmentMonitorThread(void* arg) {
    // Block the signal in this thread
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, TIMER_SIGNAL);
    pthread_sigmask(SIG_BLOCK, &mask, nullptr);

    // Create POSIX timer
    timer_t timerid;
    struct sigevent sev {};
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = TIMER_SIGNAL;
    timer_create(CLOCK_MONOTONIC, &sev, &timerid);

    // Start timer for 1-second period
    struct itimerspec its {};
    its.it_value.tv_sec = 1;
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = 1;
    its.it_interval.tv_nsec = 0;
    timer_settime(timerid, 0, &its, nullptr);

    // Main loop
    while (true) {
        siginfo_t info;
        sigwaitinfo(&mask, &info);

        bool motion = sensorData.motion.load();
        float ppm = sensorData.gas_ppm.load();
        float lux = sensorData.lux.load();

        std::cout << "[ENV] Motion: " << motion
                  << ", Gas: " << ppm
                  << ", Lux: " << lux << std::endl;

        if (motion && ppm > GAS_THRESHOLD) {
            std::cout << "[ALERT] Motion + Gas Detected!\n";
        } else if (ppm > GAS_THRESHOLD && lux < LUX_THRESHOLD) {
            std::cout << "[ALERT] High Gas in Dark!\n";
        } else if (motion) {
            std::cout << "[INFO] Motion Only\n";
        } else if (ppm > GAS_THRESHOLD) {
            std::cout << "[INFO] Gas Level High\n";
        } else {
            std::cout << "[OK] Environment Normal\n";
        }
    }

    pthread_exit(nullptr);
}
