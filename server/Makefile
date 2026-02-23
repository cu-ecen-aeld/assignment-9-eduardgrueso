CC=gcc
CFLAGS=-Wall -Wextra -Werror -pedantic
TARGET=aesdsocket

CFLAGS += -I$(SRCDIR)/aesd-char-driver

all: $(TARGET)
default: all

$(TARGET): aesdsocket.c
	$(CC) $(CFLAGS) -o $(TARGET) aesdsocket.c

clean:
	rm -f $(TARGET)
