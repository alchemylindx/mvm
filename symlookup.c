#include "mvm.h"

#include "sym.h"

char *
sym_lookup (struct sym *syms, int n, unsigned int val)
{
	static char buf[100];
	int i;

	for (i = 0; i < n; i++) {
		if (syms[i].val == val)
			return (syms[i].name);
	}
	
	sprintf (buf, "unk#%#o", val);
	return (buf);
}
