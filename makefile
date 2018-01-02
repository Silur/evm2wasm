.POSIX:
.SUFFIXES:
CC = gcc
CFLAGS = -Wall -Werror -O2 -pedantic
LDFLAGS = -shared -fPIC

all: evm2wast.o
	$(CC) -o evm2wasm.so evm2wast.o $(LDFLAGS)

evm2wast.o: evm2wast.c
	$(CC) -o evm2wast.o evm2wast.c $(CFLAGS) $(LDFLAGS)

test: test.o
	$(CC) -o test test.c
	./test

lint:
	indent *.c
	indent *.h

clean:
	rm *.o *.so
