#include "EnvironmentMonitor.hpp"
#include "../common/SensorData.hpp"
#include "../common/ThreadStats.hpp"

#include <iostream>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <iomanip>  // for std::put_time
#include <chrono>   // for system_clock

#define TIMER_SIGNAL (SIGRTMIN + 1)
extern ThreadStats envStats;

extern SensorData sensorData;

void* EnvironmentMonitorThread(void* arg) {
    std::atomic<bool>* runningFlag = static_cast<std::atomic<bool>*>(arg);

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, TIMER_SIGNAL);
    pthread_sigmask(SIG_BLOCK, &mask, nullptr);

    timer_t timerid;
    struct sigevent sev {};
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = TIMER_SIGNAL;
    timer_create(CLOCK_MONOTONIC, &sev, &timerid);

struct itimerspec its{};
its.it_value.tv_sec = 0;
its.it_value.tv_nsec = 800 * 1000000; // 0.8 seconds
its.it_interval.tv_sec = 0;
its.it_interval.tv_nsec = 800 * 1000000;

    timer_settime(timerid, 0, &its, nullptr);
    struct timespec start{}, end{};

    while (runningFlag->load()) {
        siginfo_t info;
        if (sigwaitinfo(&mask, &info) == -1) {
            perror("[ENV] sigwaitinfo");
            continue;
        }
clock_gettime(CLOCK_MONOTONIC, &start);

        bool motion = sensorData.motion.load();
        float ppm = sensorData.gas_ppm.load();
        float lux = sensorData.lux.load();

        float gas_threshold = sensorData.baseline_ppm.load() * 1.15f;
        float lux_threshold = sensorData.baseline_lux.load() * 1.15f;

        // Timestamp printing
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        std::cout << "[" << std::put_time(std::localtime(&now_time), "%H:%M:%S") << "] ";

        // Decision logic
        if (motion && ppm > gas_threshold) {
            std::cout << "[ALERT] Motion + Gas Detected!\n";
        } else if (ppm > gas_threshold && lux > lux_threshold) {
            std::cout << "[ALERT] High Gas in and Light bangg!\n";
        } else if (motion) {
            std::cout << "[INFO] Motion detected\n";
        } else if (ppm > gas_threshold) {
            std::cout << "[INFO] Gas Level High\n";
        } else {
            std::cout << "[OK] Environment Normal\n";
        }
                       clock_gettime(CLOCK_MONOTONIC, &end);
uint64_t exec_time = (end.tv_sec - start.tv_sec) * 1000000ULL +
                     (end.tv_nsec - start.tv_nsec) / 1000;
envStats.update(exec_time);
    }

    pthread_exit(nullptr);
}
