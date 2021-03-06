#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <math.h>
#include <errno.h>
#include <stdarg.h>
#include "evm2wast.h"
#include "gadgets.h"

struct opcode {
	char *name;
	size_t fee;
	int off_stack;
	int on_stack;
	char *cb;
};

enum op_nums {
	STOP = 0x00,
	ADD, MUL, SUB, DIV, SDIV, MOD, SMOD,
	ADDMOD, MULMOD, EXP, SIGNEXTEND,
	LT = 0X10, GT, SLT, SGT, EQ, ISZERO,
	AND, OR, XOR, NOT, BYTE,
	SHA3 = 0X20,
	ADDRESS = 0x30, BALANCE, ORIGIN, CALLER,
	CALLVALUE, CALLDATALOAD, CALLDATASIZE,
	CALLDATACOPY, CODESIZE, CODECOPY, GASPRICE,
	EXTCODESIZE, EXTCODECOPY, RETURNDATASIZE,
	RETURNDATACOPY,
	BLOCKHASH = 0X40, COINBASE, TIMESTAMP, NUMBER,
	DIFFICULTY, GASLIMIT,
	POP = 0X50, MLOAD, MSTORE, MSTORE8, SLOAD,
	SSTORE, JUMP, JUMPI, PC, MSIZE, GAS, JUMPDEST,
	PUSH1 = 0X60, PUSH2, PUSH3, PUSH4, PUSH5, PUSH6,
	PUSH7, PUSH8, PUSH9, PUSH10, PUSH11, PUSH12,
	PUSH13, PUSH14, PUSH15, PUSH16, PUSH17, PUSH18,
	PUSH19, PUSH20, PUSH21, PUSH22, PUSH23, PUSH24,
	PUSH25, PUSH26, PUSH27, PUSH28, PUSH29, PUSH30,
	PUSH31, PUSH32,
	DUP1 = 0X80, DUP2, DUP3, DUP4, DUP5, DUP6, DUP7,
	DUP8, DUP9, DUP10, DUP11, DUP12, DUP13, DUP14,
	DUP15, DUP16, SWAP1, SWAP2, SWAP3, SWAP4, SWAP5,
	SWAP6, SWAP7, SWAP8, SWAP9, SWAP10, SWAP11,
	SWAP12, SWAP13, SWAP14, SWAP15, SWAP16, LOG1,
	LOG2, LOG3, LOG4, LOG5,
	CREATE = 0xf0, CALL, CALLCODE, RETURN, DELEGATECALL,
	STATICCALL, REVERT,
	INVALID = 0xfe, SELFDESTRUCT
};

struct opcode opcodes[] = {
	[STOP] = {"STOP", 0,  0, 0},
	[ADD] = {"ADD", 3, 2, 1},
	[MUL] = {"MUL", 5, 2, 1},
	[SUB] = {"SUB", 3, 2, 1},
	[DIV] = {"DIV", 5, 2, 1},
	[SDIV] = {"SDIV", 5, 2, 1},
	[MOD] = {"MOD", 5, 2, 1},
	[SMOD] = {"SMOD", 5, 2, 1},
	[ADDMOD] = {"ADDMOD", 8, 3, 1},
	[MULMOD] = {"MULMOD", 8, 3, 1},
	[EXP] = {"EXP", 10, 2, 1},
	[SIGNEXTEND] = {"SIGNEXTEND", 5, 2, 1},

	[LT] = {"LT", 3, 2, 1},
	[GT] = {"GT", 3, 2, 1},
	[SLT] = {"SLT", 3, 2, 1},
	[SGT] = {"SGT", 3, 2, 1},
	[EQ] = {"EQ", 3, 2, 1},
	[ISZERO] = {"ISZERO", 3, 1, 1},
	[AND] = {"AND", 3, 2, 1},
	[OR] = {"OR", 3, 2, 1},
	[XOR] = {"XOR", 3, 2, 1},
	[NOT] = {"NOT", 3, 1, 1},
	[BYTE] = {"BYTE", 3, 2, 1},

	// 0x20 range - crypto
	[SHA3] = {"SHA3", 30, 2, 1},

	// 0x30 range - closure state
	[ADDRESS] = {"ADDRESS", 2, 0, 1},
	[BALANCE] = {"BALANCE", 400, 1, 1, "$callback_128"},
	[ORIGIN] = {"ORIGIN", 2, 0, 1},
	[CALLER] = {"CALLER", 2, 0, 1},
	[CALLVALUE] = {"CALLVALUE", 2, 0, 1},
	[CALLDATALOAD] = {"CALLDATALOAD", 3, 1, 1},
	[CALLDATASIZE] = {"CALLDATASIZE", 2, 0, 1},
	[CALLDATACOPY] = {"CALLDATACOPY", 3, 3, 0},
	[CODESIZE] = {"CODESIZE", 2, 0, 1, "$callback_32"},
	[CODECOPY] = {"CODECOPY", 3, 3, 0, "$callback"},
	[GASPRICE] = {"GASPRICE", 2, 0, 1},
	[EXTCODESIZE] = {"EXTCODESIZE", 700, 1, 1, "$callback_32"},
	[EXTCODECOPY] = {"EXTCODECOPY", 700, 4, 0, "$callback"},
	[RETURNDATASIZE] = {"RETURNDATASIZE", 2, 0, 1},
	[RETURNDATACOPY] = {"RETURNDATACOPY", 3, 3, 0},

	// '0x40' range - block operations
	[BLOCKHASH] = {"BLOCKHASH", 20, 1, 1, "$callback_256"},
	[COINBASE] = {"COINBASE", 2, 0, 1},
	[TIMESTAMP] = {"TIMESTAMP", 2, 0, 1},
	[NUMBER] = {"NUMBER", 2, 0, 1},
	[DIFFICULTY] = {"DIFFICULTY", 2, 0, 1},
	[GASLIMIT] = {"GASLIMIT", 2, 0, 1},

	// 0x50 range - 'storage' and execution
	[POP] = {"POP", 2, 1, 0},
	[MLOAD] = {"MLOAD", 3, 1, 1},
	[MSTORE] = {"MSTORE", 3, 2, 0},
	[MSTORE8] = {"MSTORE8", 3, 2, 0},
	[SLOAD] = {"SLOAD", 200, 1, 1, "$callback_256"},
	[SSTORE] = {"SSTORE", 0, 2, 0, "$callback"},
	[JUMP] = {"JUMP", 8, 1, 0},
	[JUMPI] = {"JUMPI", 10, 2, 0},
	[PC] = {"PC", 2, 0, 1},
	[MSIZE] = {"MSIZE", 2, 0, 1},
	[GAS] = {"GAS", 2, 0, 1},
	[JUMPDEST] = {"JUMPDEST", 1, 0, 0},

	// 0x60, range
	[PUSH1] = {"PUSH", 3, 0, 1},
	[PUSH2] = {"PUSH", 3, 0, 1},
	[PUSH3] = {"PUSH", 3, 0, 1},
	[PUSH4] = {"PUSH", 3, 0, 1},
	[PUSH5] = {"PUSH", 3, 0, 1},
	[PUSH6] = {"PUSH", 3, 0, 1},
	[PUSH7] = {"PUSH", 3, 0, 1},
	[PUSH8] = {"PUSH", 3, 0, 1},
	[PUSH9] = {"PUSH", 3, 0, 1},
	[PUSH10] = {"PUSH", 3, 0, 1},
	[PUSH11] = {"PUSH", 3, 0, 1},
	[PUSH12] = {"PUSH", 3, 0, 1},
	[PUSH13] = {"PUSH", 3, 0, 1},
	[PUSH14] = {"PUSH", 3, 0, 1},
	[PUSH15] = {"PUSH", 3, 0, 1},
	[PUSH16] = {"PUSH", 3, 0, 1},
	[PUSH17] = {"PUSH", 3, 0, 1},
	[PUSH18] = {"PUSH", 3, 0, 1},
	[PUSH19] = {"PUSH", 3, 0, 1},
	[PUSH20] = {"PUSH", 3, 0, 1},
	[PUSH21] = {"PUSH", 3, 0, 1},
	[PUSH22] = {"PUSH", 3, 0, 1},
	[PUSH23] = {"PUSH", 3, 0, 1},
	[PUSH24] = {"PUSH", 3, 0, 1},
	[PUSH25] = {"PUSH", 3, 0, 1},
	[PUSH26] = {"PUSH", 3, 0, 1},
	[PUSH27] = {"PUSH", 3, 0, 1},
	[PUSH28] = {"PUSH", 3, 0, 1},
	[PUSH29] = {"PUSH", 3, 0, 1},
	[PUSH30] = {"PUSH", 3, 0, 1},
	[PUSH31] = {"PUSH", 3, 0, 1},
	[PUSH32] = {"PUSH", 3, 0, 1},
	
	[DUP1] = {"DUP", 3, 0, 1},
	[DUP2] = {"DUP", 3, 0, 1},
	[DUP3] = {"DUP", 3, 0, 1},
	[DUP4] = {"DUP", 3, 0, 1},
	[DUP5] = {"DUP", 3, 0, 1},
	[DUP6] = {"DUP", 3, 0, 1},
	[DUP7] = {"DUP", 3, 0, 1},
	[DUP8] = {"DUP", 3, 0, 1},
	[DUP9] = {"DUP", 3, 0, 1},
	[DUP10] = {"DUP", 3, 0, 1},
	[DUP11] = {"DUP", 3, 0, 1},
	[DUP12] = {"DUP", 3, 0, 1},
	[DUP13] = {"DUP", 3, 0, 1},
	[DUP14] = {"DUP", 3, 0, 1},
	[DUP15] = {"DUP", 3, 0, 1},
	[DUP16] = {"DUP", 3, 0, 1},

	[SWAP1] = {"SWAP", 3, 0, 0},
	[SWAP2] = {"SWAP", 3, 0, 0},
	[SWAP3] = {"SWAP", 3, 0, 0},
	[SWAP4] = {"SWAP", 3, 0, 0},
	[SWAP5] = {"SWAP", 3, 0, 0},
	[SWAP6] = {"SWAP", 3, 0, 0},
	[SWAP7] = {"SWAP", 3, 0, 0},
	[SWAP8] = {"SWAP", 3, 0, 0},
	[SWAP9] = {"SWAP", 3, 0, 0},
	[SWAP10] = {"SWAP", 3, 0, 0},
	[SWAP11] = {"SWAP", 3, 0, 0},
	[SWAP12] = {"SWAP", 3, 0, 0},
	[SWAP13] = {"SWAP", 3, 0, 0},
	[SWAP14] = {"SWAP", 3, 0, 0},
	[SWAP15] = {"SWAP", 3, 0, 0},
	[SWAP16] = {"SWAP", 3, 0, 0},

	[LOG1] = {"LOG", 375, 2, 0},
	[LOG2] = {"LOG", 375, 2, 0},
	[LOG3] = {"LOG", 375, 2, 0},
	[LOG4] = {"LOG", 375, 2, 0},
	[LOG5] = {"LOG", 375, 2, 0},

	// '0xf0' range - closures
	[CREATE] = {"CREATE", 32000, 3, 1, "$callback_160"},
	[CALL] = {"CALL", 700, 7, 1, "$callback_32"},
	[CALLCODE] = {"CALLCODE", 700, 7, 1, "$callback_32"},
	[RETURN] = {"RETURN", 0, 2, 0},
	[DELEGATECALL] = {"DELEGATECALL", 700, 6, 1, "$callback"},
	[STATICCALL] = {"STATICCALL", 700, 6, 1},
	[REVERT] = {"REVERT", 0, 2, 0},

	// '0x70', range - other
	[INVALID] = {"INVALID", 0, 0, 0},
	[SELFDESTRUCT] = {"SELFDESTRUCT", 5000, 1, 0}
};


static inline int digits(unsigned long x) { return (floor(log10(abs(x))) + 1); }
static int64_t bytes2long(char *bytes)
{
	int64_t ret = 0;
	int i;
	for(i = 0; i < 8; i++)
	{
		    ret = (ret << 8) | bytes[i];
	}
	return ret;
}
static char *padleft(char *bytes, int curr_len, int len)
{
	if(curr_len > len)
	{
		fprintf(stderr, "pad failed to prevent memory overlap\n");
		return 0;
	}
	char *ret = calloc(len, 1);
	if(!ret) return 0;
	ret = memcpy(ret+(len-curr_len)-1, bytes, curr_len);

	return ret;
}
static  char *slice(char *bytes, int len)
{
	char *ret = malloc(sizeof(len));
	ret = memcpy(ret, bytes, len);
	return ret;
}
static int index_of(char **table, char *elem, int len)
{
	int i;
	for(i=0; i<len; i++)
	{
		if(!strcmp(table[i], elem)) return i;
	}
	return -1;
}
static size_t stack_delta;
static size_t stack_high;
static size_t stack_low;
static size_t gas_count;
static char **cb_array;
static int cb_len = 0;
// TODO dependencies

static char *build_jump_map(size_t *segments, size_t len)
{
	size_t i;
	char *brtable = calloc(len*(2+digits(len)), 1);
	char *wasm = calloc(pow((67+2*(digits(len))), len), 1);
	if(brtable == 0 || wasm == 0) goto err;
	for(i=0; i<len; i++)
	{
		snprintf(brtable + strlen(brtable), 2+digits(len)+1, " $%ld", i);
		snprintf(wasm + strlen(wasm), 68+(2*(digits(len))),
		"(if(i32.eq(get_local $jump_dest)(i32.const $%ld))(then (br $%ld))(else %s))",
		segments[i], i, wasm);
	}
	size_t s = strlen(wasm) + 358;
	wasm = realloc(wasm, s);
	if(wasm == 0) goto err;
	snprintf(wasm + strlen(wasm), s,
	"(block $0 (if (i32.eqz (get_global $init)) (then (set_global $init (i32.const 1)) (br $0)) (else (if (i32.eq (get_global $cb_dest) (i32.const 0)) (then %s) (else $cb_dest get_global $cb_dest (set_global $cb_dest (i32.const 0)) (br_table $0 ${brTable}))))))", wasm);
	free(brtable);
	return wasm;
err:
	perror("Falied to allocate memory for assemble_segments");
	return 0;
}

static char *assemble_segments(size_t *segments, size_t len)
{
	char *wasm = build_jump_map(segments, len);
	char *ret = malloc(len*(8+digits(len)+strlen(wasm)+1) + 122);
	if(ret == 0) goto err;
	strcat(ret, "(func $main (export \"main\") (local $jump_dest i32) (set_local $jump_dest (i32.const -1)) (block $done (loop $loop ");
	unsigned int i;
	for(i=0; i<len; i++)
	{
		sprintf(ret+strlen(ret), "(block $%d %s", i+1, wasm);
	}
	free(wasm);
	free(ret);
	return ret;
err:
	perror("Falied to allocate memory for assemble_segments");
	return 0;
}

static char *add_stack_check(char *segment)
{
	char *check = malloc(1);
	char *template = malloc(0);
	if (stack_high != 0)
	{
    template = "(if (i32.gt_s (get_global $sp) (i32.const %d))\
						 (then (unreachable)))";
		check = realloc(check, strlen(template)+4);
		if(!check) goto err;
		snprintf(check, strlen(template)+4, template, (1023-stack_high)*32);
	}
	if (stack_low != 0)
	{
    template = "(if (i32.lt_s (get_global $sp) (i32.const %d))\
			       (then (unreachable)))";
		check = realloc(check, strlen(template)+4);
		if(!check) goto err;
		snprintf(check, strlen(template)+4, template,  (-stack_low*32)-32);
	}
	segment = realloc(segment, strlen(check) + strlen(segment)+1);
	sprintf(segment, "%s%s", check, segment);
	stack_high = 0;
	stack_low = 0;
	stack_delta = 0;
	return segment;
err:
	free(template);
	free(check);
	perror("cannot alloc memory for stack check templates ");
	return 0;
}

static inline void add_metering(char *wast, char *segment)
{
	wast = realloc(wast, strlen(wast)+49+strlen(segment));
	if(!wast)
	{
		perror("alloc error while metering wast segment");
		return;
	}
	sprintf(wast, "(call $useGas (i64.const %ld)) %s", gas_count, segment);
	segment = "";
	gas_count = 0;
}

static inline void end_segment(char *wast, char *segment)
{
	segment = realloc(segment, strlen(segment)+2);
	segment = strcat(segment, ")");
	segment = add_stack_check(segment);
	add_metering(wast, segment);
}

static size_t next_jump_dest(char *evm_code, size_t len, size_t i)
{
	for(; i<len; i++)
	{
		if(evm_code[i] >= PUSH1 && evm_code[i] <= PUSH32)
		{
			i+=evm_code[i]-0x59;
			break;
		}
	}
	return --i;
}
char *build_module(char *wast)
{
	int i;
	char *ret = malloc(819200);
	if(!ret) return 0;
	ret = strcat(ret, "(module ");
	for(i=0; i<gadget_count; i++)
	{
		ret = strcat(ret, gadgets[i]);
	}
	ret = strcat(ret, "(global $cb_dest (mut i32) (i32.const 0))\
			(global $sp (mut i32) (i32.const -32))\
			(global $init (mut i32) (i32.const 0))\
\
			(global $memstart i32  (i32.const 33832))\
			(global $wordCount (mut i64) (i64.const 0))\
			(global $prevMemCost (mut i64) (i64.const 0))\
\
			(memory 500)\
			(export \"memory\" (memory 0))\
			(table\
      (export \"callback\")\
      anyfunc\
	    (elem ");
	for(i=0; i<cb_len; i++)
	{
		ret = strcat(ret, cb_array[i]);
	}
	ret = strcat(ret, "))\n");
	ret = realloc(ret, strlen(ret+strlen(wast)));
	if(!ret) return 0;
	ret = strcat(ret, wast);
	return ret;
}
int evm2wast(char *evm_code, size_t len, char **wast_ret, size_t *wast_size)
{
	size_t i;
	char *wast_code = malloc(1);
	char *segment = malloc(1);
	unsigned char has_jump = 0;
	size_t *jumps = 0;
	size_t jumps_len = 0;
	cb_array = malloc(13*(sizeof(char*)));
	for(i=0; i<13; i++) cb_array[i] = malloc(20);
	for (i=0; i<len; i++)
	{
		struct opcode op = opcodes[(int)evm_code[i]];
#define append_segment(template) { \
				segment = realloc(segment, strlen(segment)+strlen(template)+1);\
				if(!segment) goto err;\
				segment = strcat(segment, template);\
}
#define format_segment(template, maxlen, ...) { \
	{ \
	char *formatted = malloc(maxlen);\
	if(!formatted) goto err;\
	snprintf(formatted, maxlen, template, __VA_ARGS__);\
	append_segment(formatted);\
	}\
}
		gas_count += op.fee;
		stack_delta = op.on_stack;
		if(stack_delta > stack_high) stack_high = stack_delta;
		stack_delta = op.on_stack;
		if(stack_delta < stack_low) stack_low = stack_delta;

		switch(evm_code[i])
		{
			case JUMP:
				has_jump=1;
				append_segment("(set_local $jump_dest (call $check_overflow \
                          (i64.load (get_global $sp)) \
                          (i64.load (i32.add (get_global $sp) (i32.const 8)))\
                          (i64.load (i32.add (get_global $sp) (i32.const 16)))\
													(i64.load (i32.add (get_global $sp) (i32.const 24)))))\
                      (set_global $sp (i32.sub (get_global $sp) (i32.const 32)))\
                      (br $loop)");
				i = next_jump_dest(evm_code, len, i);
				break;

			case JUMPI:
				has_jump=1;
				append_segment("(set_local $jump_dest (call $check_overflow \
                          (i64.load (get_global $sp)) \
                          (i64.load (i32.add (get_global $sp) (i32.const 8))) \
                          (i64.load (i32.add (get_global $sp) (i32.const 16))) \
                          (i64.load (i32.add (get_global $sp) (i32.const 24))))) \
                    (set_global $sp (i32.sub (get_global $sp) (i32.const 64))) \
                    (br_if $loop (i32.eqz (i64.eqz (i64.or \
                      (i64.load (i32.add (get_global $sp) (i 32.const 32))) \
                      (i64.or \
                        (i64.load (i32.add (get_global $sp) (i32.const 40))) \
                        (i64.or \
                          (i64.load (i32.add (get_global $sp) (i32.const 48))) \
                          (i64.load (i32.add (get_global $sp) (i32.const 56))) \
                        ) \
                      ) \
                    ))))");
				add_stack_check(segment);
				add_metering(wast_code, segment);
				break;

			case JUMPDEST:
				end_segment(wast_code, segment);
				jumps = realloc(jumps, jumps_len);
				jumps[jumps_len++] = i;
				gas_count = 1;
				break;
			
			case GAS:
				append_segment("(call $GAS)\n");
				add_metering(wast_code, segment);
				break;

			case LOG1:
			case LOG2:
			case LOG3:
			case LOG4:
			case LOG5:
				format_segment("(call $LOG (i32.const %d))", 36, (int)evm_code[i]);
				break;

			case DUP1:
			case DUP2:
			case DUP3:
			case DUP4:
			case DUP5:
			case DUP6:
			case DUP7:
			case DUP8:
			case DUP9:
			case DUP10:
			case DUP11:
			case DUP12:
			case DUP13:
			case DUP14:
			case DUP15:
			case DUP16:
			case SWAP1:
			case SWAP2:
			case SWAP3:
			case SWAP4:
			case SWAP5:
			case SWAP6:
			case SWAP7:
			case SWAP8:
			case SWAP9:
			case SWAP10:
			case SWAP11:
			case SWAP12:
			case SWAP13:
			case SWAP14:
			case SWAP15:
			case SWAP16:
				format_segment("(call $%s (i32.const $%d))\n", 60,
						op.name, (int)evm_code[i]);
				break;

			case PC:
				format_segment("(call $PC (i32.const %ld", 40, i);
				break;

			case PUSH1:
			case PUSH2:
			case PUSH3:
			case PUSH4:
			case PUSH5:
			case PUSH6:
			case PUSH7:
			case PUSH8:
			case PUSH9:
			case PUSH10:
			case PUSH11:
			case PUSH12:
			case PUSH13:
			case PUSH14:
			case PUSH15:
			case PUSH16:
			case PUSH17:
			case PUSH18:
			case PUSH19:
			case PUSH20:
			case PUSH21:
			case PUSH22:
			case PUSH23:
			case PUSH24:
			case PUSH25:
			case PUSH26:
			case PUSH27:
			case PUSH28:
			case PUSH29:
			case PUSH30:
			case PUSH31:
			case PUSH32:
				{
					char *bytes = padleft(evm_code+i+1, evm_code[i]-0x59, 32);
					i+=(size_t)evm_code[i]-0x59;
					if(!bytes) goto err;
					int bytes_rounded = (int)ceil((double)(evm_code[i]/8));
					char *push = 0;
					int q;
					for(q=0; q<4-bytes_rounded; q++)
					{
						push = realloc(push, (q+1)*32); // FIXME segfault
						if(!push) goto err;
						sprintf(push, "(i64.const 0)%s", push);
					}
					for(; q<4; q++)
					{
						int64_t i64 = bytes2long(slice(bytes+(q*8), q*8+8));
						push = realloc(push, (q+1)*32);
						if(!push) goto err;
						sprintf(push, "%s (i64.const $%ld)", push, i64);

					}
					format_segment("(call $PUSH %s)", 13+strlen(push), push);
					i--;
					//free(bytes);
					//free(push);
				}
				break;

			case POP:
				// do nothing
				break;

			case STOP:
				append_segment("(br $done)");
				if(has_jump)
				{
					i = next_jump_dest(evm_code, len, i);
				}
				else
				{
					i = len;
				}
				break;

			//case SUICIDE:
			case RETURN:
				format_segment("(call $%s) (br $done)\n", 56, op.name);
				if(has_jump)
				{
					i = next_jump_dest(evm_code, len, i);
				}
				else
				{
					i = len;
				}
				break;
			
			case INVALID:
				append_segment("(unreachable)");
				i = next_jump_dest(evm_code, len, i);
				break;

			default:
				printf("found cb for op %s\n", op.name);
				if(op.cb != 0)
				{
					int cb_index = index_of(cb_array, op.name, cb_len);
					if(cb_index == -1)
					{
						cb_array[cb_len++] = op.cb;
					}
					format_segment("(call $%s (i32.const %d))", 72, op.name, cb_index);
				}
				format_segment("(call $%s)", 32, op.name);
				break;
		}
		
		stack_delta = op.on_stack - op.off_stack;
		if(stack_delta != 0)
		{
			format_segment("(set_global $sp (i32.add (get_global $sp) (i32.const %ld)))\n", 
					72, stack_delta);
		}

		//TODO stacktrace
		if(op.cb != 0)
		{
			format_segment("(set_global $cb_dest (i32.const %ld)) (br $done))", 72, jumps_len+1);
			jumps = realloc(jumps, (jumps_len+1)*sizeof(size_t));
			if(!jumps) goto err;
			jumps[jumps_len++] = i;
		}
	}
	end_segment(wast_code, segment);
	char *with_jumps = assemble_segments(jumps, jumps_len);
	wast_code = realloc(wast_code, strlen(wast_code) + strlen(with_jumps));
	if(wast_code == 0) goto err;
	wast_code = strcat(with_jumps, wast_code);
	wast_code = strcat(wast_code, "))");
	wast_code = build_module(wast_code);
	free(segment);
	free(with_jumps);
	free(jumps);
	*wast_ret = wast_code;
	*wast_size = strlen(wast_code);
	return 1;
#undef append_segment
err:
	// TODO optimize this double-branch
	if(segment) free(segment);
	if(wast_code) free(wast_code);
	//if(with_jumps) free(with_jumps);
	if(jumps) free(jumps);
	return 0;
}
