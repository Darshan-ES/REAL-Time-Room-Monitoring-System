#include <iostream>
#include <pthread.h>
#include <sched.h>
#include <cstring>
#include <csignal>
#include <atomic>
#include <chrono>

#include "common/ThreadStats.hpp"
#include "common/GpioMmap.hpp"
#include "threads/SensorSampler.hpp"
#include "threads/EnvironmentMonitor.hpp"
#include "threads/ControlService.hpp"
#include "threads/UDPSender.hpp"
#include "common/SensorData.hpp"

SensorData sensorData;
std::atomic<bool> running(true);  // shared flag

ThreadStats samplerStats;
ThreadStats envStats;
ThreadStats ctrlStats;
ThreadStats udpStats;

pthread_t samplerThread, monitorThread, controlThread, udpThread;

void setupThread(pthread_t &thread, void *(*func)(void*), int priority, void* arg, int core_id) {
    pthread_attr_t attr;
    cpu_set_t cpuset;
    struct sched_param param;

    pthread_attr_init(&attr);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    param.sched_priority = priority;
    pthread_attr_setschedparam(&attr, &param);

    // Set CPU affinity
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset);

    int ret = pthread_create(&thread, &attr, func, arg);
    if (ret != 0)
        std::cerr << "Thread creation failed: " << std::strerror(ret) << std::endl;

    pthread_attr_destroy(&attr);
}


void sigint_handler(int) {
    std::cout << "\n[MAIN] SIGINT received. Shutting down...\n";
    running = false;
}

int main() {
    signal(SIGINT, sigint_handler);

    if (!mmapGpioInit()) {
        std::cerr << "GPIO mmap init failed\n";
        return 1;
    }

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGRTMIN);
    sigaddset(&mask, SIGRTMIN + 1);
    sigaddset(&mask, SIGRTMIN + 2);
    sigaddset(&mask, SIGRTMIN + 3);
    pthread_sigmask(SIG_BLOCK, &mask, nullptr);

setupThread(samplerThread, SensorSamplerThread, 99, &running, 0);   // Core 0, 0.4s
setupThread(udpThread, UDPSenderThread, 98, &running, 1);          // Core 0, 0.8s
setupThread(monitorThread, EnvironmentMonitorThread, 99, &running, 1); // Core 1, 0.8s
setupThread(controlThread, ControlServiceThread, 98, &running, 0); // Core 1, 0.4s


    // Main loop stats
    double min_exec_time = 1e9, max_exec_time = 0, total_exec_time = 0;
    int execution_count = 0;

    while (running) {
        auto begin = std::chrono::steady_clock::now();
        usleep(500000); // 500 ms
        auto end = std::chrono::steady_clock::now();

        double exec_time = std::chrono::duration<double, std::milli>(end - begin).count();
        min_exec_time = std::min(min_exec_time, exec_time);
        max_exec_time = std::max(max_exec_time, exec_time);
        total_exec_time += exec_time;
        execution_count++;
    }

    // Join threads gracefully
    pthread_join(samplerThread, nullptr);
    pthread_join(monitorThread, nullptr);
    pthread_join(controlThread, nullptr);
    pthread_join(udpThread, nullptr);

    mmapGpioClose();

    // Final stats
    double avg = execution_count ? total_exec_time / execution_count : 0.0;
    double jitter = max_exec_time - min_exec_time;
    std::cout << "\n[Main Loop Timing Stats]\n";
    std::cout << "Average Interval: " << avg << " ms\n";
    std::cout << "Minimum Interval: " << min_exec_time << " ms\n";
    std::cout << "Maximum Interval: " << max_exec_time << " ms\n";
    std::cout << "Jitter (Max - Min): " << jitter << " ms\n\n";

    std::cout << "Final Performance Summary:\n";
    samplerStats.printStats("SensorSampler");
    envStats.printStats("EnvironmentMonitor");
    ctrlStats.printStats("ControlService");
    udpStats.printStats("UDPSender");

    return 0;
}
