# makefile for computer network homework 1

CC := g++

CFLAGS := -std=c++11 -Wall -Os

.SUFFIXS =

.PHONY =
.PHONY = server client clean

all: server client

server:
	${CC} ${CFLAGS} -o $@ $@.cpp

client:
	${CC} ${CFLAGS} -o $@ $@.cpp

clean:
	-rm -f *.o server client
