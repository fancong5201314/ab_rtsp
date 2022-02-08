.PHONY: all clean

TARGET=stream_push

CC=gcc

CFLAGS=-I. \
	   -I3rd_party/log4c/include \
	   -g3 -std=gnu11

LDFLAGS=-L3rd_party/log4c/lib \
	    -llog4c -lpthread

SRC=$(wildcard *.c \
	ab_base/*.c \
	ab_net/*.c \
	ab_log/*.c )
OBJ=$(SRC:%.c=%.o)

all:$(TARGET)

$(TARGET):$(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o:%.c
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	rm -f $(TARGET) $(OBJ)
