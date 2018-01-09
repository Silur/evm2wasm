#ifndef __EVM2WAST_H
#define __EVM2WAST_H
#include <stdlib.h>
struct gadget {
	char *name;
	char *code;
};
struct gadget gadgets[256];
char *deps[256];
char *callbacks[12];
int load_gadgets(char **lgadgets, char *dir);
char * get_gadget(char *name);
int evm2wasm(char *evm_code, size_t len, char *wast_code);
#endif



