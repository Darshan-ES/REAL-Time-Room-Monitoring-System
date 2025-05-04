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
#include <sys/ioctl.h>
#include <linux/gpio.h>
#include <cstring>
#include "Sequencer.hpp"

// Global termination flag for signal handling
std::atomic<bool> terminateProgram{false};

// GPIO device and line info
static int gpio_fd = -1;
static struct gpiohandle_request req;
static struct gpiohandle_data data;

// GPIO chip and line number
const char* GPIO_CHIP = "/dev/gpiochip0";
const int GPIO_LINE = 23;  // BCM GPIO 23

void signalHandler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        terminateProgram = true;
    }
}

// Setup GPIO using ioctl
int setupGpio() {
    // Open GPIO chip
    gpio_fd = open(GPIO_CHIP, O_RDONLY);
    if (gpio_fd < 0) {
        syslog(LOG_ERR, "Failed to open %s", GPIO_CHIP);
        return -1;
    }

    // Request GPIO line
    req.lineoffsets[0] = GPIO_LINE;
    req.flags = GPIOHANDLE_REQUEST_OUTPUT;
    req.default_values[0] = 0;
    strcpy(req.consumer_label, "gpio_toggle");
    req.lines = 1;

    int ret = ioctl(gpio_fd, GPIO_GET_LINEHANDLE_IOCTL, &req);
    if (ret < 0) {
        syslog(LOG_ERR, "Failed to get line handle");
        close(gpio_fd);
        return -1;
    }

    syslog(LOG_INFO, "GPIO %d configured for output using ioctl", GPIO_LINE);
    return 0;
}

// Cleanup GPIO
void cleanupGpio() {
    if (req.fd > 0) {
        // Set GPIO to low before cleanup
        data.values[0] = 0;
        ioctl(req.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data);
        
        close(req.fd);
    }
    if (gpio_fd > 0) {
        close(gpio_fd);
    }
    syslog(LOG_INFO, "GPIO cleaned up");
}

// Method 3: Toggle GPIO using ioctl
void toggleGpioIoctl() {
    static bool state = false;
    
    if (req.fd <= 0) {
        syslog(LOG_ERR, "GPIO not initialized");
        return;
    }

    // Toggle the state
    state = !state;
    data.values[0] = state ? 1 : 0;

    // Set GPIO value using ioctl
    int ret = ioctl(req.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data);
    if (ret < 0) {
        syslog(LOG_ERR, "Failed to set GPIO value");
    }
}

// Fibonacci Load Generator (kept for reference but commented out)
/*
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
        fibonacciIterative(100);
        iterations++;
        
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - start).count();
        
        if (elapsed >= targetMilliseconds * 1000) {
            break;
        }
    }
}

void service1() {
    generateLoad(20.00);
}

void service2() {
    generateLoad(50.00);
}
*/

int main(int argc, char* argv[]) {
    int runtime_seconds = 30; // Default runtime
    
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
    openlog("rt_gpio_ioctl", LOG_PID | LOG_CONS, LOG_USER);
    syslog(LOG_INFO, "Starting GPIO toggle demonstration using ioctl method");

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
    sequencer.addService(toggleGpioIoctl, 0, maxPriority - 1, 100);

    std::printf("Starting GPIO Toggle Demo with Method 3 (ioctl)\n");
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

    // Cleanup GPIO
    cleanupGpio();

    std::printf("\nGPIO Toggle demonstration completed.\n");
    syslog(LOG_INFO, "GPIO Toggle demonstration completed");
    closelog();
    
    return 0;
}
