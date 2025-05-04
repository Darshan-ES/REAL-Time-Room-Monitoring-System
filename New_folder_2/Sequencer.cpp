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
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <errno.h>
#include "Sequencer.hpp"

// Global termination flag for signal handling
std::atomic<bool> terminateProgram{false};

// GPIO number for pin 23 on RPi
const char* GPIO_NUM = "23";  // Adjust this based on your system's GPIO numbering

// File descriptors for GPIO
static int gpio_value_fd = -1;

// Forward declaration for cleanup
void cleanupGpio();

void signalHandler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        terminateProgram = true;
        // Perform cleanup when signal is received
        cleanupGpio();
    }
}

// Setup GPIO for sysfs method
int setupGpio() {
    // Export GPIO
    int fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd == -1) {
        syslog(LOG_ERR, "Failed to open export for writing");
        return -1;
    }

    // Write GPIO number to export file
    if (write(fd, GPIO_NUM, strlen(GPIO_NUM)) != (ssize_t)strlen(GPIO_NUM)) {
        // Check if GPIO is already exported
        if (errno != EBUSY) {
            syslog(LOG_ERR, "Failed to export GPIO %s", GPIO_NUM);
            close(fd);
            return -1;
        }
    }
    close(fd);

    // Wait for udev to set up the GPIO files
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Set direction to output
    std::string dirPath = "/sys/class/gpio/gpio" + std::string(GPIO_NUM) + "/direction";
    fd = open(dirPath.c_str(), O_WRONLY);
    if (fd == -1) {
        syslog(LOG_ERR, "Failed to open direction file");
        return -1;
    }

    if (write(fd, "out", 3) != 3) {
        syslog(LOG_ERR, "Failed to set direction");
        close(fd);
        return -1;
    }
    close(fd);

    // Open value file for repeated use
    std::string valPath = "/sys/class/gpio/gpio" + std::string(GPIO_NUM) + "/value";
    gpio_value_fd = open(valPath.c_str(), O_WRONLY);
    if (gpio_value_fd == -1) {
        syslog(LOG_ERR, "Failed to open value file");
        return -1;
    }

    syslog(LOG_INFO, "GPIO %s successfully configured for output", GPIO_NUM);
    return 0;
}

// Cleanup GPIO
void cleanupGpio() {
    // First set GPIO to low state before cleanup
    if (gpio_value_fd != -1) {
        write(gpio_value_fd, "0", 1);
        close(gpio_value_fd);
        gpio_value_fd = -1;
    }

    // Set direction back to input (safer state)
    std::string dirPath = "/sys/class/gpio/gpio" + std::string(GPIO_NUM) + "/direction";
    int fd = open(dirPath.c_str(), O_WRONLY);
    if (fd != -1) {
        write(fd, "in", 2);
        close(fd);
    }

    // Unexport GPIO
    fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if (fd != -1) {
        write(fd, GPIO_NUM, strlen(GPIO_NUM));
        close(fd);
    }
    
    syslog(LOG_INFO, "GPIO %s cleaned up", GPIO_NUM);
}

// Method 2: Toggle GPIO using sysfs
void toggleGpioSysfs() {
    static bool state = false;
    
    if (gpio_value_fd == -1) {
        syslog(LOG_ERR, "GPIO value file not open");
        return;
    }

    // Write current state to GPIO
    const char* val = state ? "1" : "0";
    if (write(gpio_value_fd, val, 1) != 1) {
        syslog(LOG_ERR, "Failed to write to GPIO value file");
    }
    
    // Reset file position for next write
    lseek(gpio_value_fd, 0, SEEK_SET);
    
    // Toggle state for next call
    state = !state;
}

int main(int argc, char* argv[]) {
    int runtime_seconds = 30; // Default runtime for better statistics
    
    if (argc > 1) {
        runtime_seconds = std::atoi(argv[1]);
        if (runtime_seconds <= 0) {
            std::fprintf(stderr, "Invalid runtime. Using default 30 seconds.\n");
            runtime_seconds = 30;
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
    openlog("rt_gpio_sysfs", LOG_PID | LOG_CONS, LOG_USER);
    syslog(LOG_INFO, "Starting GPIO toggle demonstration using sysfs method");

    // Get maximum priority for SCHED_FIFO
    int maxPriority = sched_get_priority_max(SCHED_FIFO);
    if (maxPriority == -1) {
        syslog(LOG_ERR, "Failed to get maximum priority for SCHED_FIFO");
        return 1;
    }

    // Initialize GPIO
    if (setupGpio() != 0) {
        syslog(LOG_ERR, "GPIO setup failed");
        closelog();
        return 1;
    }

    // Create sequencer
    Sequencer sequencer{};

    // Add GPIO toggle service with Rate Monotonic priority assignment
    // GPIO service: Period = 100ms, Priority = maxPriority - 1 (highest priority)
    sequencer.addService(toggleGpioSysfs, 0, maxPriority - 1, 100);

    std::printf("Starting GPIO Toggle Demo with Method 2 (sysfs)\n");
    std::printf("GPIO Service: period=100ms, priority=%d\n", maxPriority - 1);
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

    // Cleanup GPIO (also called in signal handler, but this handles normal termination)
    cleanupGpio();

    std::printf("\nGPIO Toggle demonstration completed.\n");
    syslog(LOG_INFO, "GPIO Toggle demonstration completed");
    closelog();
    
    return 0;
}
