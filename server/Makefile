CC=gcc
CFLAGS=-Wall -Wextra -Werror -pedantic -I$(SRCDIR)/aesd-char-driver
TARGET=aesdsocket


all: $(TARGET)
default: all

$(TARGET): aesdsocket.c
	$(CC) $(CFLAGS) -o $(TARGET) aesdsocket.c

clean:
	rm -f $(TARGET)
