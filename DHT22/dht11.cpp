#include "dht11.hpp"
#include <pigpio.h>
#include <iostream>
#include <cstdint>

bool readDHT11(int gpioPin, int &temperature, int &humidity) {
    uint8_t data[5] = {0};
    gpioSetMode(gpioPin, PI_OUTPUT);
    gpioWrite(gpioPin, PI_LOW);
    gpioDelay(20000); // =18ms to trigger start
    gpioWrite(gpioPin, PI_HIGH);
    gpioDelay(30);
    gpioSetMode(gpioPin, PI_INPUT);

    int count = 0;
    while (gpioRead(gpioPin) == PI_HIGH && count++ < 1000) gpioDelay(1);
    count = 0;
    while (gpioRead(gpioPin) == PI_LOW && count++ < 1000) gpioDelay(1);
    count = 0;
    while (gpioRead(gpioPin) == PI_HIGH && count++ < 1000) gpioDelay(1);

    for (int i = 0; i < 40; ++i) {
        while (gpioRead(gpioPin) == PI_LOW);
        uint32_t startTick = gpioTick();
        while (gpioRead(gpioPin) == PI_HIGH);
        uint32_t duration = gpioTick() - startTick;

        data[i / 8] <<= 1;
        if (duration > 50)
            data[i / 8] |= 1;
    }

    uint8_t checksum = data[0] + data[1] + data[2] + data[3];
    if (checksum != data[4]) {
        std::cerr << "Checksum failed!" << std::endl;
        return false;
    }

    humidity = data[0];     // integral part only
    temperature = data[2];  // integral part only
    return true;
}
