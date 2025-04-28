#include "dht22.hpp"
#include <pigpio.h>
#include <iostream>
#include <chrono>
#include <thread>

bool readDHT22(int gpioPin, float &temperature, float &humidity) {
    uint8_t data[5] = {0};
    uint8_t byteIndex = 0, bitIndex = 7;

    gpioSetMode(gpioPin, PI_OUTPUT);
    gpioWrite(gpioPin, PI_LOW);
    gpioDelay(20000); // 20ms start signal
    gpioWrite(gpioPin, PI_HIGH);
    gpioDelay(30);
    gpioSetMode(gpioPin, PI_INPUT);

    // Wait for DHT response
    int count = 0;
    while (gpioRead(gpioPin) == PI_HIGH && count++ < 1000) gpioDelay(1);
    count = 0;
    while (gpioRead(gpioPin) == PI_LOW && count++ < 1000) gpioDelay(1);
    count = 0;
    while (gpioRead(gpioPin) == PI_HIGH && count++ < 1000) gpioDelay(1);

    // Read 40 bits (5 bytes)
    for (int i = 0; i < 40; ++i) {
        while (gpioRead(gpioPin) == PI_LOW);
        auto start = std::chrono::high_resolution_clock::now();
        while (gpioRead(gpioPin) == PI_HIGH);
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - start
        ).count();

        if (duration > 50)  // 0: 26-28us, 1: 70us
            data[byteIndex] |= (1 << bitIndex);

        if (bitIndex == 0) {
            bitIndex = 7;
            byteIndex++;
        } else {
            bitIndex--;
        }
    }

    // Validate checksum
    uint8_t checksum = data[0] + data[1] + data[2] + data[3];
    if (data[4] != checksum) return false;

    humidity = ((data[0] << 8) + data[1]) * 0.1;
    temperature = (((data[2] & 0x7F) << 8) + data[3]) * 0.1;
    if (data[2] & 0x80) temperature = -temperature;

    return true;
}
