.PHONY: all clean

TARGET=stream_push

CC=gcc

TOP=..
CFLAGS=-I$(TOP) \
	   -I$(TOP)/3rd_party/log4c/include \
	   -g3 -std=gnu11

LDFLAGS=-L$(TOP)/3rd_party/log4c/lib \
	    -llog4c -lpthread

SRC=$(wildcard *.c \
	$(TOP)/ab_base/*.c \
	$(TOP)/ab_net/*.c \
	$(TOP)/ab_log/*.c )
OBJ=$(SRC:%.c=%.o)

all:$(TARGET)

$(TARGET):$(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o:%.c
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	rm -f $(TARGET) $(OBJ)
