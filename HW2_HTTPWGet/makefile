CC=gcc
CFLAGS=-g -Wall -pedantic

all: mywget

mywget: arraylist.h mywget.c
	$(CC) $(CFLAGS) -o mywget mywget.c -lws2_32

clean:
	rm mywget