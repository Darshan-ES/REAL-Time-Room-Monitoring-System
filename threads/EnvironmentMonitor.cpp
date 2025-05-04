#include "EnvironmentMonitor.hpp"
#include "../common/SensorData.hpp"
#include "../common/ThreadStats.hpp"
#include <cmath>

#include <iostream>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#include <iomanip>
#include <chrono>
#include <syslog.h>

#define TIMER_SIGNAL (SIGRTMIN + 1)
extern ThreadStats envStats;
extern SensorData sensorData;

void sendEmailAlert(const std::string& subject, const std::string& body) {
    std::string command = "echo \"" + body + "\" | mail -s \"" + subject + "\" elg993@esimpleai.com";
    int ret = system(command.c_str());
    if (ret != 0) {
        syslog(LOG_ERR, "[ENV] Failed to send email alert");
    }
}

void* EnvironmentMonitorThread(void* arg)
{
    std::atomic<bool>* runningFlag = static_cast<std::atomic<bool>*>(arg);

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, TIMER_SIGNAL);
    pthread_sigmask(SIG_BLOCK, &mask, nullptr);

    // Open log file
    FILE* logFile = fopen("../envlog.txt", "a");
    if (!logFile) syslog(LOG_ERR, "[ENV] Failed to open envlog.txt");

    // Setup timer
    timer_t timerid;
    struct sigevent sev {};
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = TIMER_SIGNAL;
    timer_create(CLOCK_MONOTONIC, &sev, &timerid);

    struct itimerspec its{};
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = 80 * 1000000;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 220* 1000000;
    timer_settime(timerid, 0, &its, nullptr);

    struct timespec start{}, end{};
    int gasDriftCount = 0, luxDriftCount = 0;

    while (runningFlag->load())
     {
        siginfo_t info;
        
        if (sigwaitinfo(&mask, &info) == -1) 
        {
            perror("[ENV] sigwaitinfo");

            continue;
            
        }
        clock_gettime(CLOCK_MONOTONIC, &start);

        bool motion = sensorData.motion.load();
        float ppm = sensorData.gas_ppm.load();
        
        float lux = sensorData.lux.load();
        float gas_baseline = sensorData.baseline_ppm.load();
        float lux_baseline = sensorData.baseline_lux.load();
        
        float gas_threshold = gas_baseline * 1.15f;
        
        float lux_threshold = lux_baseline * 1.15f;

        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        std::tm* now_tm = std::localtime(&now_time);
        char timestampStr[32];
        std::strftime(timestampStr, sizeof(timestampStr), "%H:%M:%S", now_tm);

        // Logging logic
        std::string log_msg;

        if (motion && ppm > gas_threshold) {
            log_msg = "[ALERT] Motion + Gas Detected!";
            sendEmailAlert("URGENT: Motion + Gas Detected", "High gas level with motion at " + std::string(timestampStr));

        } else if (ppm > gas_threshold && lux > lux_threshold) {
            
            log_msg = "[ALERT] High Gas and Light bangg!";
            sendEmailAlert("URGENT: Gas + Lux Alert", "High gas and light detected at " + std::string(timestampStr));

        } else if (motion) {
    log_msg = "[INFO] Motion detected";
}
else if (lux > lux_threshold) {
    log_msg = "[ALERT] High Light Intensity!";
    sendEmailAlert("URGENT: Lux Alert", "High light detected at " + std::string(timestampStr));
}     } else if (ppm > gas_threshold) {
            log_msg = "[INFO] Gas Level High";
            sendEmailAlert("URGENT: Gas + Lux Alert", "High gas detected " + std::string(timestampStr));

        } else {
            log_msg = "[OK] Environment Normal";
        }

        std::cout << "[" << timestampStr << "] " << log_msg << "\n";
        if (logFile) fprintf(logFile, "[%s] %s\n", timestampStr, log_msg.c_str());

        // Check for baseline drift
        if (fabs(ppm - gas_baseline) / gas_baseline > 0.25f)
            gasDriftCount++;
        else
            gasDriftCount = 0;

        if (fabs(lux - lux_baseline) / lux_baseline > 0.25f)
            luxDriftCount++;
        else
            luxDriftCount = 0;

        if (gasDriftCount >= 5) {
            std::string drift_msg = "Suggest recalibration — Gas baseline outdated";
            syslog(LOG_INFO, "%s (Δ%.1f%%)", drift_msg.c_str(), 100.0f * fabs(ppm - gas_baseline) / gas_baseline);
            if (logFile) fprintf(logFile, "[%s] %s (Δ%.1f%%)\n", timestampStr, drift_msg.c_str(),
                                 100.0f * fabs(ppm - gas_baseline) / gas_baseline);
            sendEmailAlert("Gas Baseline Drift", "Drift: " + std::to_string(drift_msg) + "%");

            gasDriftCount = 0;
        }

        if (luxDriftCount >= 5) {
            std::string drift_msg = "Suggest recalibration — Lux baseline outdated";
            syslog(LOG_INFO, "%s (Δ%.1f%%)", drift_msg.c_str(), 100.0f * fabs(lux - lux_baseline) / lux_baseline);
            if (logFile) fprintf(logFile, "[%s] %s (Δ%.1f%%)\n", timestampStr, drift_msg.c_str(),
                                 100.0f * fabs(lux - lux_baseline) / lux_baseline);
            luxDriftCount = 0;
        }

        clock_gettime(CLOCK_MONOTONIC, &end);
        uint64_t exec_time = (end.tv_sec - start.tv_sec) * 1000000ULL +
                             (end.tv_nsec - start.tv_nsec) / 1000;
        envStats.update(exec_time);
    }

    if (logFile) fclose(logFile);
    
    pthread_exit(nullptr);
    
}
