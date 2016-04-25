CC=gcc
CFLAGS=-Wall
SOURCE=hw3.c
TARGET=hw3

all:
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE)

clean:
	rm $(TARGET)
