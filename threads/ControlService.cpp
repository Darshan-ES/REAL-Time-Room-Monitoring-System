#include "ControlService.hpp"
#include "../common/SensorData.hpp"

#include <pigpio.h>
#include <iostream>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#define ALERT_GPIO 27
#define TIMER_SIGNAL (SIGRTMIN + 2)

extern SensorData sensorData;

void* ControlServiceThread(void* arg) {
    // Block signal in thread
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, TIMER_SIGNAL);
    pthread_sigmask(SIG_BLOCK, &mask, nullptr);

    // Initialize GPIO
    gpioSetMode(ALERT_GPIO, PI_OUTPUT);
    gpioWrite(ALERT_GPIO, PI_LOW);

    // Create POSIX timer
    timer_t timerid;
    struct sigevent sev{};
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = TIMER_SIGNAL;
    timer_create(CLOCK_MONOTONIC, &sev, &timerid);

    // Start timer (500ms period)
    struct itimerspec its{};
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = 500 * 1000000; // 500ms
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 500 * 1000000;
    timer_settime(timerid, 0, &its, nullptr);

    while (true) {
        siginfo_t info;
        sigwaitinfo(&mask, &info);

        bool motion = sensorData.motion.load();
        float ppm = sensorData.gas_ppm.load();

        if (motion || ppm > GAS_THRESHOLD) {
            gpioWrite(ALERT_GPIO, PI_HIGH);
            std::cout << "[CTRL] Alert ON (Motion or Gas)\n";
        } else {
            gpioWrite(ALERT_GPIO, PI_LOW);
        }
    }

    gpioWrite(ALERT_GPIO, PI_LOW);  // Cleanup
    pthread_exit(nullptr);
}
