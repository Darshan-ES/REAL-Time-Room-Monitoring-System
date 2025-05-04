# Simple Makefile for RT Services Sequencer

CXX = g++
CXXFLAGS = --std=c++23 -Wall -Werror -pedantic -pthread
TARGET = rt_sequencer
SOURCES = Sequencer.cpp
HEADERS = Sequencer.hpp

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

run-sudo: $(TARGET)
	sudo ./$(TARGET)