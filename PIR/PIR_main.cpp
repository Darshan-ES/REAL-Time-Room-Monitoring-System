#include <iostream>
#include <pigpio.h>
#include <unistd.h>
#include "PIR.hpp"

#define PIR_PIN 17  // BCM GPIO17 (Physical Pin 11)

int main() {
    if (gpioInitialise() < 0) {
        std::cerr << "Failed to initialize pigpio!" << std::endl;
        return 1;
    }

    setupPIR(PIR_PIN);

    while (true) {
        bool motionDetected = readPIR(PIR_PIN);
        if (motionDetected) {
            std::cout << "Motion Detected!" << std::endl;
        } else {
            std::cout << "No Motion." << std::endl;
        }
        sleep(1);  // check every 1 second
    }

    gpioTerminate();
    return 0;
}
