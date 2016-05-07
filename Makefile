CC=gcc
CFLAGS=-Wall
SOURCE=hw3.c
TARGET=shell

all:
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE)

clean:
	rm -rf $(TARGET) a.out
