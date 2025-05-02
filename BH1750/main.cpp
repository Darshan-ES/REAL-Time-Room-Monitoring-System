#include "bh1750.hpp"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>

int main() {
    const char *i2c_dev = "/dev/i2c-3";  // Use new I2C bus on GPIO4 & GPIO5
    int i2c_fd = open(i2c_dev, O_RDWR);
    if (i2c_fd < 0) {
        std::cerr << "Failed to open I2C device\n";
        return 1;
    }

    if (!init_bh1750(i2c_fd)) {
        close(i2c_fd);
        return 1;
    }

    while (true) {
        float lux = read_bh1750(i2c_fd);
        if (lux >= 0)
            std::cout << "Ambient Light: " << lux << " lux" << std::endl;
        sleep(1);
    }

    close(i2c_fd);
    return 0;
}
