#include "ads1115.hpp"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

static int i2c_fd = -1;
static int ads_address = 0x48;

bool initADS1115(const char *device, int address) {
    ads_address = address;
    i2c_fd = open(device, O_RDWR);
    if (i2c_fd < 0) {
        std::cerr << "Error: Cannot open I2C device " << device << std::endl;
        return false;
    }

    if (ioctl(i2c_fd, I2C_SLAVE, ads_address) < 0) {
        std::cerr << "Error: Failed to set I2C address 0x" << std::hex << ads_address << std::endl;
        return false;
    }

    return true;
}

void closeADS1115() {
    if (i2c_fd >= 0) close(i2c_fd);
}

int16_t readADS1115Raw(uint8_t channel) {
    if (channel > 3) return 0;

    uint16_t mux = 0x4000 | (channel << 12); // Single-ended AINx
    uint16_t config = 0x8000               // Start single conversion
                    | mux                 // MUX bits
                    | 0x0200              // PGA ±4.096V
                    | 0x0100              // MODE = single-shot
                    | 0x0080              // Data rate = 1600 SPS
                    | 0x0003;             // Disable comparator

    uint8_t config_bytes[3] = {
        0x01,               // Config register
        static_cast<uint8_t>(config >> 8),
        static_cast<uint8_t>(config & 0xFF)
    };

    if (write(i2c_fd, config_bytes, 3) != 3) {
        std::cerr << "Failed to write config to ADS1115\n";
        return 0;
    }

    usleep(8500); // Wait for conversion

    uint8_t pointer = 0x00; // Conversion register
    write(i2c_fd, &pointer, 1);

    uint8_t buffer[2];
    if (read(i2c_fd, buffer, 2) != 2) {
        std::cerr << "Failed to read conversion\n";
        return 0;
    }

    return (buffer[0] << 8) | buffer[1]; // Signed 16-bit value
}

float convertToVoltage(int16_t raw_adc, float vref) {
    // Assuming PGA ±4.096V ? LSB = 125 µV
    return raw_adc * 0.000125f;
}
