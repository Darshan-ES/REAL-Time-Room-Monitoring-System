#include "GpioMmap.hpp"
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstdio>

#define GPIO_BASE 0xFE200000  // Raspberry Pi 4 GPIO base
#define BLOCK_SIZE 4096

volatile uint32_t* gpio_base = nullptr;

bool mmapGpioInit() {
    int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) {
        perror("open /dev/mem failed");
        return false;
    }

    gpio_base = (volatile uint32_t*)mmap(nullptr, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, GPIO_BASE);
    
    close(mem_fd);

    if (gpio_base == MAP_FAILED) 
    {
        
        perror("mmap failed");
        gpio_base = nullptr;
        return false;
    }
    return true;
}

void mmapGpioClose() {
    if (gpio_base) munmap((void*)gpio_base, BLOCK_SIZE);
}

void setGpioOutput(int gpio) {
    int reg = gpio / 10;
    int shift = (gpio % 10) * 3;
    gpio_base[reg] = (gpio_base[reg] & ~(0b111 << shift)) | (0b001 << shift);  
}

void setGpioInput(int gpio) {
    int reg = gpio / 10;
    int shift = (gpio % 10) * 3;
gpio_base[reg] = gpio_base[reg] & ~(0b111 << shift);
}

void writeGpio(int gpio, bool value) {
    int reg = value ? 7 : 10;  // GPSET0=7, GPCLR0=10
    gpio_base[reg] = (1 << gpio);
}

bool readGpio(int gpio) {
    return (gpio_base[13] & (1 << gpio)) != 0;  // GPLEV0 = 13
}
