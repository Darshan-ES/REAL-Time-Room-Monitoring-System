/*
 * This is a C++ version of the canonical pthread service example. It intends
 * to abstract the service management functionality and sequencing for ease
 * of use. Much of the code is left to be implemented by the student.
 *
 * Build with g++ --std=c++23 -Wall -Werror -pedantic
 * Steve Rizor 3/16/2025
 */

#pragma once

#include <cstdint>
#include <functional>
#include <thread>
#include <vector>
#include <semaphore>
#include <atomic>
#include <sys/syslog.h>
#include <sched.h>
#include <unistd.h>
#include <pthread.h>
#include <algorithm>
#include <ctime>
#include <signal.h>
#include <cstring>
#include <limits>
#include <chrono>
#include <mutex>
#include <memory>

// The service class contains the service function and service parameters
// (priority, affinity, etc). It spawns a thread to run the service, configures
// the thread as required, and executes the service whenever it gets released.
class Service
{
public:
    struct Statistics {
        double minExecutionTime{std::numeric_limits<double>::max()};
        double maxExecutionTime{0.0};
        double totalExecutionTime{0.0};
        uint64_t executionCount{0};
        double minStartJitter{std::numeric_limits<double>::max()};
        double maxStartJitter{0.0};
        double totalStartJitter{0.0};
        uint64_t deadlineMisses{0};
        double maxLateness{0.0};
        std::chrono::steady_clock::time_point firstRelease;
        std::chrono::steady_clock::time_point lastExpectedRelease;
        bool firstReleaseSet{false};
    };

    template<typename T>
    Service(T&& doService, uint8_t affinity, uint8_t priority, uint32_t period) :
        _doService(doService),
        _affinity(affinity),
        _priority(priority),
        _period(period),
        _semaphore(0),
        _running(true)
    {
        // Start the service thread, which will begin running the given function immediately
        _service = std::jthread(&Service::_provideService, this);
    }

    // Delete copy operations
    Service(const Service&) = delete;
    Service& operator=(const Service&) = delete;

    // Delete move operations  
    Service(Service&&) = delete;
    Service& operator=(Service&&) = delete;
 
    void stop(){
        _running = false;
        // Release the semaphore one more time in case the service is waiting
        _semaphore.release();
    }
 
    void release(){
        _semaphore.release();
    }

    uint32_t getPeriod() const {
        return _period;
    }

    void setTimerId(timer_t timerId) {
        _timerId = timerId;
    }

    timer_t getTimerId() const {
        return _timerId;
    }

    void printStatistics() const {
        std::lock_guard<std::mutex> lock(_statsMutex);
        
        if (_stats.executionCount == 0) {
            printf("Service (period=%ums): No executions\n", _period);
            return;
        }

        double avgExecutionTime = _stats.totalExecutionTime / _stats.executionCount;
        double avgStartJitter = _stats.totalStartJitter / _stats.executionCount;
        
        // Calculate execution time jitter
        double executionTimeJitter = _stats.maxExecutionTime - _stats.minExecutionTime;
        
        // Calculate start time jitter
        double startTimeJitter = _stats.maxStartJitter - _stats.minStartJitter;
        
        // Calculate deadline miss rate
        double deadlineMissRate = (_stats.deadlineMisses * 100.0) / _stats.executionCount;

        printf("\n=== Service Statistics (Period: %u ms, Priority: %u) ===\n", _period, _priority);
        printf("Execution Count: %lu\n", _stats.executionCount);
        
        printf("Execution Time (ms):\n");
        printf("  Min: %.3f\n", _stats.minExecutionTime);
        printf("  Max: %.3f\n", _stats.maxExecutionTime);
        printf("  Avg: %.3f\n", avgExecutionTime);
        printf("  Jitter: %.3f\n", executionTimeJitter);
        
        printf("Start Time Jitter (ms):\n");
        printf("  Min: %.3f\n", _stats.minStartJitter);
        printf("  Max: %.3f\n", _stats.maxStartJitter);
        printf("  Avg: %.3f\n", avgStartJitter);
        printf("  Range: %.3f\n", startTimeJitter);
        
        printf("Deadline Analysis:\n");
        printf("  Deadline: %u ms\n", _period);
        printf("  Deadline Misses: %lu (%.2f%%)\n", _stats.deadlineMisses, deadlineMissRate);
        if (_stats.deadlineMisses > 0) {
            printf("  Max Lateness: %.3f ms\n", _stats.maxLateness);
        }
        
        printf("================================================\n");
    }
 
private:
    std::function<void(void)> _doService;
    std::jthread _service;
    uint8_t _affinity;
    uint8_t _priority;
    uint32_t _period;  // in milliseconds
    std::counting_semaphore<1> _semaphore;
    std::atomic<bool> _running;
    timer_t _timerId;
    
    // Statistics
    mutable std::mutex _statsMutex;
    Statistics _stats;

    void _initializeService()
    {
        // Set CPU affinity
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(_affinity, &cpuset);
        
        pthread_t currentThread = pthread_self();
        int result = pthread_setaffinity_np(currentThread, sizeof(cpu_set_t), &cpuset);
        if (result != 0) {
            syslog(LOG_ERR, "Failed to set thread affinity: %s", strerror(result));
        }

        // Set thread priority and scheduling policy
        struct sched_param param;
        param.sched_priority = _priority;
        
        result = pthread_setschedparam(currentThread, SCHED_FIFO, &param);
        if (result != 0) {
            syslog(LOG_ERR, "Failed to set thread scheduling parameters: %s", strerror(result));
        }
    }

    void _provideService()
    {
        _initializeService();
        
        while (_running) {
            // Wait for release
            _semaphore.acquire();
            
            if (_running) {
                auto releaseTime = std::chrono::steady_clock::now();
                
                // Record release statistics
                {
                    std::lock_guard<std::mutex> lock(_statsMutex);
                    
                    if (!_stats.firstReleaseSet) {
                        _stats.firstRelease = releaseTime;
                        _stats.lastExpectedRelease = releaseTime;
                        _stats.firstReleaseSet = true;
                    } else {
                        // Calculate start time jitter
                        auto expectedRelease = _stats.lastExpectedRelease + std::chrono::milliseconds(_period);
                        auto jitter = std::chrono::duration_cast<std::chrono::microseconds>(
                            releaseTime - expectedRelease).count() / 1000.0; // Convert to ms
                        
                        _stats.minStartJitter = std::min(_stats.minStartJitter, std::abs(jitter));
                        _stats.maxStartJitter = std::max(_stats.maxStartJitter, std::abs(jitter));
                        _stats.totalStartJitter += std::abs(jitter);
                        
                        _stats.lastExpectedRelease = expectedRelease;
                    }
                }
                
                // Execute the service
                auto startTime = std::chrono::steady_clock::now();
                _doService();
                auto endTime = std::chrono::steady_clock::now();
                
                // Record execution time statistics
                {
                    std::lock_guard<std::mutex> lock(_statsMutex);
                    
                    double executionTime = std::chrono::duration_cast<std::chrono::microseconds>(
                        endTime - startTime).count() / 1000.0; // Convert to ms
                    
                    _stats.minExecutionTime = std::min(_stats.minExecutionTime, executionTime);
                    _stats.maxExecutionTime = std::max(_stats.maxExecutionTime, executionTime);
                    _stats.totalExecutionTime += executionTime;
                    
                    // Check for deadline miss (deadline = period)
                    double responseTime = std::chrono::duration_cast<std::chrono::microseconds>(
                        endTime - releaseTime).count() / 1000.0; // Convert to ms
                    
                    if (responseTime > _period) {
                        _stats.deadlineMisses++;
                        double lateness = responseTime - _period;
                        _stats.maxLateness = std::max(_stats.maxLateness, lateness);
                    }
                    
                    _stats.executionCount++;
                }
            }
        }
    }
};
 
// The sequencer class contains the services set and manages
// starting/stopping the services. While the services are running,
// the sequencer releases each service at the requisite timepoint.
class Sequencer
{
public:
    Sequencer() {
        // Initialize syslog
        openlog("rt_services", LOG_PID | LOG_CONS, LOG_USER);
    }

    ~Sequencer() {
        closelog();
    }

    template<typename... Args>
    void addService(Args&&... args)
    {
        // Add the new service to the services list,
        // We use push_back with a unique_ptr to avoid moving Service objects
        _services.push_back(std::make_unique<Service>(std::forward<Args>(args)...));
    }

    void startServices()
    {
        syslog(LOG_INFO, "Sequencer starting services");
        
        // Create and start timers for each service
        for (size_t i = 0; i < _services.size(); ++i) {
            timer_t timerId;
            struct sigevent sev;
            struct itimerspec its;
            
            // Configure timer to send signal when it expires
            std::memset(&sev, 0, sizeof(sev));
            sev.sigev_notify = SIGEV_THREAD;
            sev.sigev_notify_function = Sequencer::timerHandler;
            sev.sigev_value.sival_ptr = _services[i].get();
            
            // Create the timer
            if (timer_create(CLOCK_REALTIME, &sev, &timerId) == -1) {
                syslog(LOG_ERR, "Failed to create timer for service %zu", i);
                continue;
            }
            
            _services[i]->setTimerId(timerId);
            
            // Configure the timer period
            uint32_t period_ms = _services[i]->getPeriod();
            its.it_value.tv_sec = period_ms / 1000;
            its.it_value.tv_nsec = (period_ms % 1000) * 1000000;
            its.it_interval = its.it_value;
            
            // Start the timer
            if (timer_settime(timerId, 0, &its, nullptr) == -1) {
                syslog(LOG_ERR, "Failed to start timer for service %zu", i);
            }
        }
    }

    void stopServices()
    {
        syslog(LOG_INFO, "Sequencer stopping services");
        
        // Stop and delete all timers
        for (auto& service : _services) {
            timer_delete(service->getTimerId());
            service->stop();
        }
        
        // Wait for all services to finish
        // (The jthread destructor will join automatically)
        
        // Print statistics for all services
        printf("\n=== FINAL SERVICE STATISTICS SUMMARY ===\n");
        for (const auto& service : _services) {
            service->printStatistics();
        }
    }

private:
    std::vector<std::unique_ptr<Service>> _services;
    
    static void timerHandler(union sigval sv) {
        auto* service = static_cast<Service*>(sv.sival_ptr);
        service->release();
    }
};
