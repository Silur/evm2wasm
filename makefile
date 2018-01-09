.POSIX:
.SUFFIXES:
CC = gcc
CFLAGS = -Wall -Werror -O2 -pedantic
LDFLAGS = -shared -fPIC

all: libevm2wasm test

libevm2wasm: evm2wast.o
	$(CC) $(LDFLAGS) -o bin/evm2wasm.so bin/evm2wast.o 
	@echo Built target "libevm2wasm"

test: evm2wast.o test.o
	$(CC) -o bin/test bin/evm2wast.o bin/test.o
	@echo Built target "test"

evm2wast.o: bin src/evm2wast.c
	$(CC) $(CFLAGS) $(LDFLAGS) -c -o bin/evm2wast.o src/evm2wast.c

test.o: bin src/test.c
	$(CC) $(CFLAGS) -c -o bin/test.o src/test.c

lint:
	indent *.c
	indent *.h

bin:
	mkdir bin

clean:
	rm -rf bin
