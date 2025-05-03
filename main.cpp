#include <iostream>
#include <pthread.h>
#include <sched.h>
#include <cstring>

#include "threads/SensorSampler.hpp"
#include "threads/EnvironmentMonitor.hpp"
#include "threads/ControlService.hpp"
#include "threads/UDPSender.hpp"
#include "common/SensorData.hpp"

SensorData sensorData;  // Shared sensor data

void setupThread(pthread_t &thread, void *(*func)(void*), int priority) {
    pthread_attr_t attr;
    struct sched_param param;

    pthread_attr_init(&attr);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);

    param.sched_priority = priority;
    pthread_attr_setschedparam(&attr, &param);

    int ret = pthread_create(&thread, &attr, func, nullptr);
    if (ret != 0) {
        std::cerr << "Thread creation failed: " << std::strerror(ret) << std::endl;
    }

    pthread_attr_destroy(&attr);
}

int main() {
     if (!mmapGpioInit()) {
        std::cerr << "GPIO mmap init failed\n";
        return 1;
    }
    
    
    pthread_t samplerThread, monitorThread, controlThread, udpThread;

    setupThread(samplerThread, SensorSamplerThread, 80);     // 500ms
    setupThread(monitorThread, EnvironmentMonitorThread, 70); // 1s
    setupThread(controlThread, ControlServiceThread, 60);     // 500ms
    setupThread(udpThread, UDPSenderThread, 50);              // 2s

    pthread_join(samplerThread, nullptr);
    pthread_join(monitorThread, nullptr);
    pthread_join(controlThread, nullptr);
    pthread_join(udpThread, nullptr);

    mmapGpioClose();  


    return 0;
}
