#include "bh1750.hpp"
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

#define BH1750_ADDR 0x23
#define BH1750_CMD_CONT_H_RES_MODE 0x10

bool init_bh1750(int i2c_fd) {
    if (ioctl(i2c_fd, I2C_SLAVE, BH1750_ADDR) < 0) {
        std::cerr << "Failed to connect to BH1750\n";
        return false;
    }

    char config[1] = { BH1750_CMD_CONT_H_RES_MODE };
    if (write(i2c_fd, config, 1) != 1) {
        std::cerr << "Failed to initialize BH1750\n";
        return false;
    }

    usleep(180000); // Wait for measurement
    return true;
}

float read_bh1750(int i2c_fd) {
    char data[2] = {0};
    if (read(i2c_fd, data, 2) != 2) {
        std::cerr << "Failed to read from BH1750\n";
        return -1.0f;
    }

    int raw = (data[0] << 8) | data[1];
    return raw / 1.2f;  // Convert to lux
}
