#include "dht11.hpp"
#include <iostream>
#include <pigpio.h>
#include <unistd.h>

#define DHT11_PIN 4  // GPIO4 (physical pin 7)

int main() {
     std::cerr << "Process Start.\n";
    if (gpioInitialise() < 0) {
        std::cerr << "Failed to initialize pigpio.\n";
        return 1;
    }

    int temperature = 0, humidity = 0;
     std::cerr << "Process 2\n";

    while (true) {
        std::cerr << "Process 3\n";
        if (readDHT11(DHT11_PIN, temperature, humidity)) {
            std::cout << "Temperature: " << temperature << " °C, "
                      << "Humidity: " << humidity << " %\n";
        } else {
            std::cerr << "Failed to read DHT11 sensor.\n";
        }
        std::cerr << "Process 4\n";
        sleep(2);  // DHT11 requires at least 1s delay between reads
    }

    gpioTerminate();
    return 0;
}
