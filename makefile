# makefile for computer network homework 1

CC := g++

CFLAGS := -std=c++11 -Wall -Os

OBJS := server.o client.o

.SUFFIXS =
.SUFFIXS = .cpp .o

.PHONY =
.PHONY = all server client clean

all: server client

server: server.o
	${CC} ${CFLAGS} -o $@ $<

client: client.o
	${CC} ${CFLAGS} -o $@ $<

.cpp.o:
	${CC} ${CFLAGS} -c $<

clean:
	-rm -f *.o server client
