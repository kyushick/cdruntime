#!/bin/bash
DEBUG_ENABLED=1

CDPATH := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))/../..
PACKER_PATH=${CDPATH}/src/persistence
CC=icpc
#CC=g++

CFLAGS=-std=gnu++0x -fPIC -Wall -c -I. -I${PACKER_PATH} #-fmax-errors=3
LFLAGS=-fPIC -L${PACKER_PATH} -Wl,-rpath=${PACKER_PATH} -lpacker -ldl
ifeq (${DEBUG_ENABLED}, 1)
CFLAGS += -g -O3
else
CFLAGS += -O3
endif

OBJS=singleton.o libc_wrapper.o

TARGET=libwrapLibc.so

all: ${TARGET}

${TARGET}: ${OBJS}
	$(CC) -shared -Wl,-soname,libwrapLibc.so $^ -o ${TARGET} ${LFLAGS}

singleton.o: singleton.cc logging.h
	$(CC) ${CFLAGS} singleton.cc -o $@

libc_wrapper.o: libc_wrapper.cc logging.h
	$(CC) ${CFLAGS} libc_wrapper.cc -o $@



clean:
	rm *.o; rm ${TARGET}

