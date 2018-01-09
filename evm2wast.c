#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "evm2wast.h"
#include "util.h"

int is_wast_ex(char *fname)
{
	char *ext;
	if((ext = strrchr(fname,'.')) != 0)
	{
		return !strcmp(ext,".csv");
	}
	return 0;
}
char *get_gadget(char *gadget)
{
	unsigned char i;
	for(i=0; i<256; i++) // TODO deps
	{
		if(!strcmp(gadget, gadgets[i].name)) return gadgets[i].code;
	}
	fprintf(stderr, "no gadget found for %s\n", gadget);
	return 0;
}
int load_gadgets(char **lgadgets, char *dir)
{
	DIR *gadget_dir;
	struct dirent *ent;
	if ((gadget_dir = opendir(dir)) == 0)
	{
		return -1;
	}
	unsigned char i = 0;
	while ((ent = readdir(gadget_dir)) != 0)
	{
		char *fname = ent->d_name;
		if(!is_wast_ex(fname)) continue;
		size_t fsize = 0;
		FILE *f = fopen(fname, "r");
		fseek(f, 0, SEEK_END);
		fsize = ftell(f);
		rewind(f);
		gadgets[i].name = malloc(strlen(fname));
		gadgets[i].name = strcpy(gadgets[i].name, strtok(fname, "."));
		gadgets[i].code = malloc(fsize);
		if (!fgets(gadgets[i++].code, (int)fsize, f))
		{
			return -2;
		}
		fclose(f);
	}
	closedir(gadget_dir);
	return 1;
}
int evm2wasm(char *evm_code, size_t len, char *wast_code)
{
	size_t i;
	unsigned int pushflag = 0;
	char *wast_chunk;
	for (i=0; i<len; i++)
	{
		if (pushflag)
		{
			wast_code[i] = evm_code[i];
			continue;
		}

		switch(evm_code[i])
		{
			// TODO try to keep all opcode-dependent stuff here
#define load_gadget(name) { \
	if((wast_chunk = get_gadget("STOP")) == 0) return -2; \
	wast_code = realloc(wast_code, strlen(wast_code+strlen(wast_chunk))); \
	wast_code = strcat(wast_code, wast_chunk); \
}
#define set_gadget(str) {  \
	wast_code[i] = malloc(strlen(str)); \
	wast_code[i] = strcpy(wast_code[i], str); \
}
			
			case 0x00:
				load_gadget("STOP");
				break;
			case 0x01:
				load_gadget("ADD");
				break;
			case 0x02:
				load_gadget("MUL");
				break;
			case 0x03:
				load_gadget("SUB");
				break;
			case 0x04:
				load_gadget("DIV");
				break;
			case 0x05:
				load_gadget("SDIV");
				break;
			case 0x06:
				load_gadget("MOD");
				break;
			case 0x07:
				load_gadget("SMOD");
				break;
			case 0x08:
				load_gadget("ADDMOD");
				break;
			case 0x09:
				load_gadget("MULMOD");
				break;
			case 0x0a:
				load_gadget("EXP");
				break;
			case 0x0b:
				load_gadget("SIGNEXTEND");
				break;
			case 0x10:
				load_gadget("LT");
				break;
			case 0x11:
				load_gadget("GT");
				break;
			case 0x12:
				load_gadget("SLT");
				break;
			case 0x13:
				load_gadget("SGT");
				break;
			case 0x14:
				load_gadget("EQ");
				break;
			case 0x15:
				load_gadget("ISZERO");
				break;
			case 0x16:
				load_gadget("AND");
				break;
			case 0x17:
				load_gadget("OR");
				break;
			case 0x18:
				load_gadget("XOR");
				break;
			case 0x19:
				load_gadget("NOT");
				break;
			case 0x1a:
				load_gadget("BYTE");
				break;
			case 0x20:
				load_gadget("SHA3");
				break;
			case 0x35:
				load_gadget("CALLDATALOAD");
				break;
			case 0x51:
				load_gadget("MLOAD");
				break;
			case 0x52:
				load_gadget("MSTORE");
				break;
			case 0x53:
				load_gadget("MSTORE8");
				break;
			case 0x59:
				load_gadget("MSIZE");
				break;
			default:
				return -1;
		}
	}
	return 0;
}
