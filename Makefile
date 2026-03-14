# Gavin Lee - ECE 3220
# Project 1 Makefile

CC = gcc
CFLAGS = -Wall -g
PICFLAGS = -fPIC -shared
LDLIBS = -ldl

all: allocator

allocator: allocator.c
	$(CC) $(CFLAGS) $(PICFLAGS) allocator.c -o libmyalloc.so

clean:
	rm -f libmyalloc.so *.o

tar:
	tar cvzf project3.tgz README Makefile allocator.c

