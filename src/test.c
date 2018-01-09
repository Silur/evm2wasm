#include "evm2wast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXBUFFERLEN 8192
int
main(int argc, char *argv[])
{
	if (argc != 2 || argv[1] == NULL) {
		printf("Usage: test <bytecode>\n");
		exit(1);
	}
	
	size_t fsize = 0;
	FILE *evm_input = fopen(argv[1], "r");
	FILE *wast_output;
	char *evm_bytecode;
	char *wast_buffer = calloc(MAXBUFFERLEN, sizeof(char));

	fseek(evm_input, 0, SEEK_END); 
	fsize = ftell(evm_input);
	rewind(evm_input);
	
	evm_bytecode = calloc(fsize, sizeof(char));
	evm_bytecode = fgets(evm_bytecode, fsize, evm_input);
	fclose(evm_input);
	
	evm2wasm(evm_bytecode, fsize, wast_buffer);

	wast_output = fopen("out.wast", "w");
	fputs(wast_buffer, wast_output);
	fclose(wast_output);

	return 0;
}
