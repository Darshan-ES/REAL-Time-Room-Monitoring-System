#include "PIR.hpp"
#include "../common/GpioMmap.hpp"  
#include <iostream>

int setupPIR(int gpio) {
    setGpioInput(gpio);  // Set the GPIO pin as input
    return 0;            // Success
}

bool readPIR(int gpio) {
    return readGpio(gpio);  // Return 1 if motion detected, else 0
}
