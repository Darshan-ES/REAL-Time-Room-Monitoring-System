CXX = g++
CXXFLAGS = -std=c++20 -Wall -pthread
INCLUDES = -Isensors -Ithreads -Icommon

SOURCES = main.cpp \
          sensors/PIR.cpp sensors/ads1115.cpp sensors/mq135.cpp sensors/bh1750.cpp \
          threads/SensorSampler.cpp threads/EnvironmentMonitor.cpp threads/ControlService.cpp threads/UDPSender.cpp

OBJECTS = $(SOURCES:.cpp=.o)
TARGET = rtes_app

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OBJECTS) -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)
