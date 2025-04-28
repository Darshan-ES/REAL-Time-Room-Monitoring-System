#include "PIR.hpp"
#include <pigpio.h>
#include <iostream>

void setupPIR(int pin) {
    gpioSetMode(pin, PI_INPUT);
}

bool readPIR(int pin) {
    return gpioRead(pin);
}
