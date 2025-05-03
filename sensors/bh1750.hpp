#ifndef BH1750_HPP
#define BH1750_HPP

bool init_bh1750(int i2c_fd);
float read_bh1750(int i2c_fd);

#endif
