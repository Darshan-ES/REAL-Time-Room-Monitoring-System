/*
 * This is a C++ version of the canonical pthread service example. It intends
 * to abstract the service management functionality and sequencing for ease
 * of use. Much of the code is left to be implemented by the student.
 *
 * Build with g++ --std=c++23 -Wall -Werror -pedantic
 * Steve Rizor 3/16/2025
 */
#include <cstdint>
#include <cstdio>
#include <csignal>
#include <atomic>
#include <chrono>
#include <thread>
#include <sys/syslog.h>
#include <sched.h>
#include "Sequencer.hpp"

// Global termination flag for signal handling
std::atomic<bool> terminateProgram{false};

void signalHandler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        terminateProgram = true;
    }
}

// Fibonacci Load Generator for simulating CPU work
uint64_t fibonacciIterative(uint64_t n) {
    if (n <= 1) return n;
    
    uint64_t a = 0, b = 1;
    for (uint64_t i = 2; i <= n; ++i) {
        uint64_t temp = a + b;
        a = b;
        b = temp;
    }
    return b;
}

void generateLoad(double targetMilliseconds) {
    auto start = std::chrono::high_resolution_clock::now();
    uint64_t iterations = 0;
    
    while (true) {
        fibonacciIterative(100);  // Do some work
        iterations++;
        
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - start).count();
        
        if (elapsed >= targetMilliseconds * 1000) {
            break;
        }
    }
}

// Service functions
void service1() {
    // Generate load for approximately 20.04ms
    generateLoad(20.00);
}

void service2() {
    // Generate load for approximately 50.00ms
    generateLoad(50.00);
}

int main(int argc, char* argv[]) {
    int runtime_seconds = 10; // Default runtime
    
    if (argc > 1) {
        runtime_seconds = std::atoi(argv[1]);
        if (runtime_seconds <= 0) {
            std::fprintf(stderr, "Invalid runtime. Using default 10 seconds.\n");
            runtime_seconds = 10;
        }
    }

    // Set up signal handlers
    struct sigaction sa;
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    // Initialize syslog
    openlog("rt_services", LOG_PID | LOG_CONS, LOG_USER);
    syslog(LOG_INFO, "Starting real-time services demonstration");

    // Get maximum priority for SCHED_FIFO
    int maxPriority = sched_get_priority_max(SCHED_FIFO);
    if (maxPriority == -1) {
        syslog(LOG_ERR, "Failed to get maximum priority for SCHED_FIFO");
        return 1;
    }

    // Create sequencer
    Sequencer sequencer{};

    // Add services with Rate Monotonic priority assignment
    // Service 1: Period = 20ms, Priority = maxPriority - 1 (higher)
    sequencer.addService(service1, 0, maxPriority - 1, 20);

    // Service 2: Period = 50ms, Priority = maxPriority - 2 (lower)
    sequencer.addService(service2, 0, maxPriority - 2, 50);

    std::printf("Starting services with Rate Monotonic scheduling\n");
    std::printf("Service 1: period=20ms, priority=%d, target WCET=20ms\n", maxPriority - 1);
    std::printf("Service 2: period=50ms, priority=%d, target WCET=50ms\n", maxPriority - 2);
    std::printf("Runtime: %d seconds (or press Ctrl+C to terminate)\n", runtime_seconds);
    std::printf("----------------------------------------\n\n");

    // Start services
    sequencer.startServices();

    // Wait for termination signal or runtime expiration
    auto start_time = std::chrono::steady_clock::now();
    while (!terminateProgram) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Check if runtime has expired
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();
        
        if (elapsed >= runtime_seconds) {
            std::printf("\nRuntime of %d seconds completed.\n", runtime_seconds);
            break;
        }
    }

    std::printf("\nTerminating services...\n");
    std::printf("----------------------------------------\n");
    
    // Stop services  
    sequencer.stopServices();

    std::printf("\nReal-time services demonstration completed.\n");
    syslog(LOG_INFO, "Real-time services demonstration completed");
    closelog();
    
    return 0;
}
