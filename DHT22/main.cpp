#include <iostream>
#include <pigpio.h>
#include <unistd.h>
#include "dht22.hpp"

#define DHT_PIN 4  // BCM GPIO pin 4 (physical pin 7)

int main() {
    if (gpioInitialise() < 0) {
        std::cerr << "pigpio initialization failed!" << std::endl;
        return 1;
    }

    float temperature = 0.0f;
    float humidity = 0.0f;

    while (true) {
        if (readDHT22(DHT_PIN, temperature, humidity)) {
            std::cout << "Temperature: " << temperature << " °C, "
                      << "Humidity: " << humidity << " %" << std::endl;
        } else {
            std::cout << "Failed to read DHT22 sensor." << std::endl;
        }
        sleep(2);
    }

    gpioTerminate();
    return 0;
}
