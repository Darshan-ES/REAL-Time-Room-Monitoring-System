#include "SensorSampler.hpp"
#include "../common/SensorData.hpp"
#include "../common/GpioMmap.hpp"
#include "../common/ThreadStats.hpp"

#include "../sensors/PIR.hpp"
#include "../sensors/ads1115.hpp"
#include "../sensors/mq135.hpp"

#include "../sensors/bh1750.hpp"


#include <signal.h>
#include <time.h>

#include <unistd.h>

#include <fcntl.h>
#include <syslog.h>

extern ThreadStats samplerStats;

#define PIR_GPIO 17
#define TIMER_SIGNAL SIGRTMIN
#define CLEAN_AIR_VOLTAGE 1.4f

extern SensorData sensorData;

void* SensorSamplerThread(void* arg) 
{
    std::atomic<bool>* runningFlag = static_cast<std::atomic<bool>*>(arg);

    openlog("SensorSampler", LOG_PID | LOG_CONS, LOG_USER);
    syslog(LOG_INFO, "Thread started");

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, TIMER_SIGNAL);
    pthread_sigmask(SIG_BLOCK, &mask, nullptr);

    setupPIR(PIR_GPIO);
    syslog(LOG_INFO, "PIR initialized");

    if (!initADS1115("/dev/i2c-1", 0x48)) {
        syslog(LOG_ERR, "ADS1115 init failed");
        pthread_exit(nullptr);
    }
    syslog(LOG_INFO, "ADS1115 ready");

    int i2c_fd = open("/dev/i2c-3", O_RDWR);
    if (i2c_fd < 0 || !init_bh1750(i2c_fd)) {
        syslog(LOG_ERR, "BH1750 init failed");
        pthread_exit(nullptr);
    }
    syslog(LOG_INFO, "BH1750 ready");

    // Calibration
    syslog(LOG_INFO, "Starting calibration for 5 seconds...");
    float total_ppm = 0.0, total_lux = 0.0;
    int motion_count = 0, samples = 0;
    const int calibration_duration_ms = 7200;
    const int sample_interval_ms = 100;

    for (int elapsed = 0; elapsed < calibration_duration_ms; elapsed += sample_interval_ms) {
        usleep(sample_interval_ms * 1000);

        bool motion = readPIR(PIR_GPIO);
        
        int16_t raw_adc = readADS1115Raw(0);
        
        float voltage = convertToVoltage(raw_adc, 3.3);
        
        float ppm = calculatePPM(voltage, CLEAN_AIR_VOLTAGE);
        
        float lux = read_bh1750(i2c_fd);

        motion_count += motion ? 1 : 0;
        total_ppm += ppm;
        total_lux += lux;
        samples++;
    }

    float baseline_ppm = total_ppm / samples;
    float baseline_lux = total_lux / samples;
    float motion_ratio = (float)motion_count / samples;

    syslog(LOG_INFO, "Calibration done.");
    syslog(LOG_INFO, "Baseline Gas: %.3f ppm, Lux: %.2f, Motion Rate: %.2f",
           baseline_ppm, baseline_lux, motion_ratio);

    // POSIX timer setup
    timer_t timerid;
    struct sigevent sev{};
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = TIMER_SIGNAL;

    if (timer_create(CLOCK_MONOTONIC, &sev, &timerid) != 0) 
    {
        syslog(LOG_ERR, "timer_create failed");
        pthread_exit(nullptr);
    }

struct itimerspec its{};
its.it_value.tv_sec = 0;
its.it_value.tv_nsec = 10* 1000000; // 0.4 seconds
its.it_interval.tv_sec = 0;
its.it_interval.tv_nsec = 160 * 1000000;

    if (timer_settime(timerid, 0, &its, nullptr) != 0) {
        syslog(LOG_ERR, "timer_settime failed");
        pthread_exit(nullptr);
    }

    syslog(LOG_INFO, "Waiting for signal...");
struct timespec start{}, end{};

    while (runningFlag->load()) {
        
        siginfo_t info;
        if (sigwaitinfo(&mask, &info) == -1) {
            syslog(LOG_ERR, "sigwaitinfo failed");
            break;
        }
clock_gettime(CLOCK_MONOTONIC, &start);

        bool motion = readPIR(PIR_GPIO);
        
        int16_t raw_adc = readADS1115Raw(0);
        
        float voltage = convertToVoltage(raw_adc, 3.3);
        
        float ppm = calculatePPM(voltage, CLEAN_AIR_VOLTAGE);
        
        float lux = read_bh1750(i2c_fd);

        sensorData.motion.store(motion);
        sensorData.gas_ppm.store(ppm);
        sensorData.lux.store(lux);


        syslog(LOG_INFO, "Sampled - Motion: %d, Gas: %.3f ppm, Lux: %.2f",
               motion, ppm, lux);
        syslog(LOG_INFO, " ");
               sensorData.baseline_ppm.store(baseline_ppm);
sensorData.baseline_lux.store(baseline_lux);
               clock_gettime(CLOCK_MONOTONIC, &end);
uint64_t exec_time = (end.tv_sec - start.tv_sec) * 1000000ULL +
                     (end.tv_nsec - start.tv_nsec) / 1000;
samplerStats.update(exec_time);
    }

    close(i2c_fd);
    
    closelog();
    
    pthread_exit(nullptr);
    
}
