# RTES_FINAL_PROJECT

g++ toggle_gpio18.cpp -o toggle_gpio18 -lpigpio -lrt -lpthread

sudo pigpiod      # (only if not already running)
sudo ./toggle_gpio18

DHT22

g++ main.cpp dht22.cpp -o dht22_reader -lpigpio -lrt -lpthread
