#include "ControlService.hpp"
#include "../common/SensorData.hpp"
#include "../common/GpioMmap.hpp"
#include "../common/ThreadStats.hpp"

#include <iostream>
#include <iomanip>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <chrono>
extern ThreadStats ctrlStats;

#define ALERT_GPIO 23
#define TIMER_SIGNAL (SIGRTMIN + 2)
int blink_counter = 0;
bool led_state = false;
bool in_blink_phase = true;
int total_blinks = 5;  

extern SensorData sensorData;

void* ControlServiceThread(void* arg) {
    std::atomic<bool>* runningFlag = static_cast<std::atomic<bool>*>(arg);

    std::cout << "[ControlService] Thread started\n";

    // Block signal
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, TIMER_SIGNAL);
    pthread_sigmask(SIG_BLOCK, &mask, nullptr);

    setGpioOutput(ALERT_GPIO);
    writeGpio(ALERT_GPIO, false);

    // POSIX timer setup
    timer_t timerid;
    struct sigevent sev{};
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = TIMER_SIGNAL;

    if (timer_create(CLOCK_MONOTONIC, &sev, &timerid) != 0) {
        perror("[ControlService] timer_create");
        pthread_exit(nullptr);
    }

struct itimerspec its{};
its.it_value.tv_sec = 0;
its.it_value.tv_nsec = 40 * 1000000; // 0.4 seconds
its.it_interval.tv_sec = 0;
its.it_interval.tv_nsec = 200 * 1000000;

    if (timer_settime(timerid, 0, &its, nullptr) != 0) {
        perror("[ControlService] timer_settime");
        pthread_exit(nullptr);
    }

    std::cout << "[ControlService] Waiting for signal...\n";
    struct timespec start{}, end{};

while (runningFlag->load()) {
    siginfo_t info;
    if (sigwaitinfo(&mask, &info) == -1) {
        perror("[ControlService] sigwaitinfo");
        break;
    }

    clock_gettime(CLOCK_MONOTONIC, &start);

    if (in_blink_phase) {
        led_state = !led_state;
        writeGpio(ALERT_GPIO, led_state);
        std::cout << "[Blink] LED " << (led_state ? "ON" : "OFF") << "\n";

        if (!led_state) blink_counter++;  // Count full blink cycle

        if (blink_counter >= total_blinks) {
            in_blink_phase = false;

            // Set new interval for normal alert mode (e.g., 400ms)
            its.it_value.tv_nsec = 400 * 1000000;
            its.it_interval.tv_nsec = 400 * 1000000;
            timer_settime(timerid, 0, &its, nullptr);
        }
    } else {
        // Normal control logic
        //bool motion = sensorData.motion.load();
        float ppm = sensorData.gas_ppm.load();
        float lux = sensorData.lux.load();

        float ppm_baseline = sensorData.baseline_ppm.load();
        float lux_baseline = sensorData.baseline_lux.load();

        float ppm_threshold = ppm_baseline * 1.01f;
        float lux_threshold = lux_baseline * 1.15f;


        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        std::cout << "[" << std::put_time(std::localtime(&now_c), "%T") << "] ";

        if (lux >  lux_threshold|| ppm > ppm_threshold) {
            writeGpio(ALERT_GPIO, true);
            std::cout << "[ControlService] Alert ON\n";
        } else {
            writeGpio(ALERT_GPIO, false);
            std::cout << "[ControlService] Alert OFF\n";
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    uint64_t exec_time = (end.tv_sec - start.tv_sec) * 1000000ULL +
                         (end.tv_nsec - start.tv_nsec) / 1000;
    ctrlStats.update(exec_time);
}


    pthread_exit(nullptr);
}
