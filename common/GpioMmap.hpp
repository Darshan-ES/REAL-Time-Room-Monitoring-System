#ifndef GPIO_MMAP_HPP
#define GPIO_MMAP_HPP

#include <cstdint>

bool mmapGpioInit();
void setGpioOutput(int gpio);
void setGpioInput(int gpio);
void writeGpio(int gpio, bool value);
bool readGpio(int gpio);
void mmapGpioClose();

#endif
