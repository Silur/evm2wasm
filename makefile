.POSIX:
.SUFFIXES:
CC = gcc
CFLAGS = -Wall -Werror -O2 -pedantic
LDFLAGS = -shared -fPIC

all: libevm2wasm test

libevm2wasm: evm2wast.o
	$(CC) -o evm2wasm.so evm2wast.o $(LDFLAGS)
	@echo Built target "libevm2wasm"

test: test.o
	$(CC) -o test test.c
	@echo Built target "test"

evm2wast.o: evm2wast.c
	$(CC) -c -o evm2wast.o evm2wast.c $(CFLAGS) $(LDFLAGS)

test.o: test.c
	$(CC) $(CFLAGS) -o test.o test.c

lint:
	indent *.c
	indent *.h

clean:
	rm *.o *.so
