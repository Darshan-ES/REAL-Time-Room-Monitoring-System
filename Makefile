CXX = g++
CXXFLAGS = -std=c++20 -Wall -Werror -pthread

# Source files
SRCS = main.cpp \
       threads/SensorSampler.cpp \
       threads/EnvironmentMonitor.cpp \
       threads/ControlService.cpp \
       threads/UDPSender.cpp \
       common/GpioMmap.cpp \
       sensors/PIR.cpp \
       sensors/ads1115.cpp \
       sensors/mq135.cpp \
       sensors/bh1750.cpp


# Object files
OBJS = $(SRCS:.cpp=.o)

# Output executable
TARGET = rtes_app

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
