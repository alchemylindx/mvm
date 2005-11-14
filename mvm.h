#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <memory.h>
#include <string.h>

#define AMEM_ADDR 076776000
#define AMEM_SIZE 02000

#include "syms.h"


#define LP_INITIAL_LOCAL_BLOCK_OFFSET 1
#define LP_FEF 0
#define LP_ENTRY_STATE -1
#define LP_EXIT_STATE -2
#define LP_CALL_STATE -3
#define LP_CALL_BLOCK_LENGTH 4

struct Q {
	unsigned int q_pointer : 24;
	unsigned int q_data_type : 5;
	unsigned int q_flag : 1;
	unsigned int q_cdr_code : 2;
};


struct sym;
char *sym_lookup (struct sym *syms, int n, unsigned int val);
