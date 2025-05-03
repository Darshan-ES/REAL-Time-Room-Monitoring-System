#include "UDPSender.hpp"
#include "../common/SensorData.hpp"

#include <iostream>
#include <string>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>

#define TIMER_SIGNAL (SIGRTMIN + 3)
#define DEST_IP "192.168.1.100"     // 🔁 Change to actual receiver IP
#define DEST_PORT 5005

extern SensorData sensorData;

void* UDPSenderThread(void* arg) {
    // Block signal in this thread
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, TIMER_SIGNAL);
    pthread_sigmask(SIG_BLOCK, &mask, nullptr);

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

    // Start timer (2s period)
    struct itimerspec its{};
    its.it_value.tv_sec = 2;
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = 2;
    its.it_interval.tv_nsec = 0;
    timer_settime(timerid, 0, &its, nullptr);

    // Main loop
    while (true) {
        siginfo_t info;
        sigwaitinfo(&mask, &info);

        bool motion = sensorData.motion.load();
        float ppm = sensorData.gas_ppm.load();
        float lux = sensorData.lux.load();

        std::string msg = "{ \"motion\": " + std::string(motion ? "true" : "false") +
                          ", \"gas\": " + std::to_string(ppm) +
                          ", \"lux\": " + std::to_string(lux) + " }";

        sendto(sock, msg.c_str(), msg.size(), 0,
               (struct sockaddr*)&dest_addr, sizeof(dest_addr));

        std::cout << "[UDP] Sent: " << msg << std::endl;
    }

    close(sock);
    pthread_exit(nullptr);
}
