# Makefile for simple_capture.c

CC = gcc
CFLAGS = -Wall -O2
TARGET = simple_capture
SRC = simple_capture.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
