#include "UDPSender.hpp"
#include "../common/SensorData.hpp"
#include "../common/ThreadStats.hpp"

#include <iostream>
#include <string>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <syslog.h>

#define TIMER_SIGNAL (SIGRTMIN + 3)
#define DEST_IP "192.168.12.245"     //
#define DEST_PORT 5005

extern ThreadStats udpStats;

extern SensorData sensorData;

void* UDPSenderThread(void* arg) {
    std::atomic<bool>* runningFlag = static_cast<std::atomic<bool>*>(arg);
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, TIMER_SIGNAL);
    pthread_sigmask(SIG_BLOCK, &mask, nullptr);
    openlog("UDPSender", LOG_PID | LOG_CONS, LOG_USER);

    // Create UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "UDP socket creation failed\n";
        pthread_exit(nullptr);
    }

    struct sockaddr_in dest_addr{};
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(DEST_PORT);
    inet_pton(AF_INET, DEST_IP, &dest_addr.sin_addr);

    // Create POSIX timer
    timer_t timerid;
    struct sigevent sev{};
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = TIMER_SIGNAL;
    timer_create(CLOCK_MONOTONIC, &sev, &timerid);

    struct itimerspec its{};
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = 100 * 1000000; // 0.8 seconds
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 240 * 1000000;

    timer_settime(timerid, 0, &its, nullptr);
    struct timespec start{}, end{};

    // Main loop
    while (runningFlag->load()) {

        siginfo_t info;
        sigwaitinfo(&mask, &info);
        clock_gettime(CLOCK_MONOTONIC, &start);

        bool motion = sensorData.motion.load();
        float ppm = sensorData.gas_ppm.load();
        float lux = sensorData.lux.load();

        std::string msg = "{ \"motion\": " + std::string(motion ? "true" : "false") +
                          ", \"gas\": " + std::to_string(ppm) +
                          ", \"lux\": " + std::to_string(lux) + " }";

        sendto(sock, msg.c_str(), msg.size(), 0,
               (struct sockaddr*)&dest_addr, sizeof(dest_addr));

        //std::cout << "[UDP] Sent: " << msg << std::endl;
        syslog(LOG_INFO, "[UDP] Sent: %s", msg.c_str());

        clock_gettime(CLOCK_MONOTONIC, &end);
        uint64_t exec_time = (end.tv_sec - start.tv_sec) * 1000000ULL +
                     (end.tv_nsec - start.tv_nsec) / 1000;
        udpStats.update(exec_time);
    }
    closelog();

    close(sock);
    pthread_exit(nullptr);
}
