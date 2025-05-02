#include <pigpio.h>
#include <iostream>
#include <unistd.h>  // for sleep

#define GPIO_PIN 18  // BCM GPIO18 (physical pin 12)

int main() {
    if (gpioInitialise() < 0) {
        std::cerr << "Failed to initialize pigpio!" << std::endl;
        return 1;
    }

    gpioSetMode(GPIO_PIN, PI_OUTPUT);

    while (true) {
        std::cerr << "Functional pigpio!" << std::endl;
        gpioWrite(GPIO_PIN, 1);  // Set pin HIGH
        gpioDelay(500000);       // Delay 500 milliseconds (500,000 microseconds)

        gpioWrite(GPIO_PIN, 0);  // Set pin LOW
        gpioDelay(500000);       // Delay 500 milliseconds
    }

    gpioTerminate();
    return 0;
}
