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
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <errno.h>
#include "Sequencer.hpp"

// Global termination flag for signal handling
std::atomic<bool> terminateProgram{false};

// For Raspberry Pi 4, the GPIO base address
#define BCM2711_PERI_BASE       0xFE000000
#define GPIO_BASE               (BCM2711_PERI_BASE + 0x200000)
#define BLOCK_SIZE              (4 * 1024)

// GPIO register offsets (each is 4 bytes, so we divide by 4)
#define GPFSEL0                 0
#define GPFSEL1                 1
#define GPFSEL2                 2
#define GPSET0                  7
#define GPCLR0                  10

// Global pointer to mapped GPIO registers
static volatile uint32_t* gpio_map = nullptr;
static bool gpio_initialized = false;

void signalHandler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        terminateProgram = true;
    }
}

// Initialize GPIO23 using memory mapped I/O
int initGpioMmap() {
    // Open /dev/mem for direct memory access (requires root privileges)
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        syslog(LOG_ERR, "Failed to open /dev/mem: %s", strerror(errno));
        return -1;
    }

    // Map GPIO registers into our address space
    void* map = mmap(
        nullptr,                 // Let kernel choose the address
        BLOCK_SIZE,             // Size to map
        PROT_READ | PROT_WRITE, // Allow read/write access
        MAP_SHARED,            // Share with other processes
        fd,                    // File descriptor
        GPIO_BASE             // Physical address to map
    );

    // Close fd after mapping (no longer needed)
    close(fd);

    if (map == MAP_FAILED) {
        syslog(LOG_ERR, "mmap failed: %s", strerror(errno));
        return -1;
    }

    // Set global pointer to mapped memory
    gpio_map = static_cast<volatile uint32_t*>(map);

    // Configure GPIO23 as output
    // GPIO23 is controlled by GPFSEL2 register (handles GPIO 20-29)
    // Each GPIO uses 3 bits, so GPIO23 uses bits 9-11 in GPFSEL2
    uint32_t reg_value = gpio_map[GPFSEL2];
    reg_value &= ~(0b111 << 9);  // Clear bits 9-11
    reg_value |= (0b001 << 9);   // Set bits 9-11 to 001 (output mode)
    gpio_map[GPFSEL2] = reg_value;

    gpio_initialized = true;
    syslog(LOG_INFO, "GPIO23 initialized via memory mapping");
    return 0;
}

// Cleanup GPIO memory mapping
void cleanupGpioMmap() {
    if (gpio_map) {
        munmap((void*)gpio_map, BLOCK_SIZE);
        gpio_map = nullptr;
        gpio_initialized = false;
        syslog(LOG_INFO, "GPIO memory mapping released");
    }
}

// Toggle GPIO23 using direct register access
void toggleGpioMmap() {
    static bool state = false;
    
    if (!gpio_initialized) {
        syslog(LOG_ERR, "GPIO not initialized");
        return;
    }

    state = !state;
    
    if (state) {
        // Set GPIO23 high by writing to GPSET0 register
        gpio_map[GPSET0] = (1 << 23);
    } else {
        // Set GPIO23 low by writing to GPCLR0 register
        gpio_map[GPCLR0] = (1 << 23);
    }
}

// Fibonacci Load Generator (kept for reference)
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
    openlog("rt_gpio_mmap", LOG_PID | LOG_CONS, LOG_USER);
    syslog(LOG_INFO, "Starting GPIO toggle demonstration using memory mapping");

    // Get maximum priority for SCHED_FIFO
    int maxPriority = sched_get_priority_max(SCHED_FIFO);
    if (maxPriority == -1) {
        syslog(LOG_ERR, "Failed to get maximum priority for SCHED_FIFO");
        return 1;
    }

    // Initialize GPIO using memory mapping
    if (initGpioMmap() != 0) {
        syslog(LOG_ERR, "GPIO initialization failed");
        closelog();
        return 1;
    }

    // Create sequencer
    Sequencer sequencer{};

    // Add GPIO toggle service with Rate Monotonic priority assignment
    // GPIO service: Period = 100ms, Priority = maxPriority - 1 (highest priority)
    sequencer.addService(toggleGpioMmap, 0, maxPriority - 1, 100);

    std::printf("Starting GPIO Toggle Demo with Method 4 (Memory Mapping)\n");
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
    cleanupGpioMmap();

    std::printf("\nGPIO Toggle demonstration completed.\n");
    syslog(LOG_INFO, "GPIO Toggle demonstration completed");
    closelog();
    
    return 0;
}
