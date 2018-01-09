.POSIX:
.SUFFIXES:
CC = gcc
CFLAGS = -Wall -Werror -O2 -pedantic
LDFLAGS = -shared -fPIC

all: libevm2wasm test

libevm2wasm: evm2wast.o
	$(CC) $(LDFLAGS) -o bin/evm2wasm.so bin/evm2wast.o 
	@echo Built target "libevm2wasm"

test: test.o evm2wast.o
	$(CC) -o bin/test bin/test.o bin/evm2wast.o
	@echo Built target "test"

evm2wast.o: bin src/evm2wast.c
	$(CC) $(CFLAGS) -c -o bin/evm2wast.o src/evm2wast.c

test.o: bin src/test.c
	$(CC) $(CFLAGS) -o bin/test.o src/test.c

lint:
	indent *.c
	indent *.h

bin:
	mkdir bin

clean:
	rm -rf bin
