#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <memory.h>
#include <string.h>

#define AMEM_ADDR 076776000
#define AMEM_SIZE 02000

#define BIT_FIELD(size, pos) (((pos) << 6) | size)
#define BIT_POS(ppss) (((ppss) >> 6) & 077)
#define BIT_SIZE(ppss) ((ppss) & 077)

#define STRUCTURE_TYPE_FIELD (BIT_FIELD(5, 023))

#define SIZE_OF_AREA_ARRAYS 377

#include "syms.h"


#define LP_INITIAL_LOCAL_BLOCK_OFFSET 1
#define LP_FEF 0
#define LP_ENTRY_STATE -1
#define LP_EXIT_STATE -2
#define LP_CALL_STATE -3
#define LP_CALL_BLOCK_LENGTH 4


struct sym;
char *sym_lookup (struct sym *syms, int n, unsigned int val);
