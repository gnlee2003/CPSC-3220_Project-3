# Gavin Lee - ECE 3220
# Project 3 Makefile

CC = gcc
CFLAGS = -Wall -g
PICFLAGS = -fPIC -shared
LDLIBS = -ldl

all: libmyalloc.so

libmyalloc.so: allocator.c
	$(CC) $(CFLAGS) $(PICFLAGS) allocator.c -o libmyalloc.so $(LDLIBS)

test: mytest.c allocator.c
	$(CC) $(CFLAGS) allocator.c stresstest.c -o mytest
	LD_PRELOAD=./libmyalloc.so ./mytest

clean:
	rm -f libmyalloc.so *.o mytest

tar:
	tar cvzf project3.tgz README Makefile allocator.c