#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "sym.h"
#include "symdesc.h"

#define nelts(arr) (sizeof arr / sizeof arr[0])

FILE *hf, *cf;

void
do_syms (char *name, struct sym *syms, int nsyms)
{
	int i;
	struct sym *sp;
	char namebuf[1000];
	char *p;

	fprintf (hf, "enum %s {\n", name);
	for (i = 0; i < nsyms; i++) {
		sp = &syms[i];
		strcpy (namebuf, sp->name);
		for (p = namebuf; *p; p++)
			if (*p == '-')
				*p = '_';
		fprintf (hf, "\t%s = %u, /* %#o %#x */\n",
			 namebuf, sp->val, sp->val, sp->val);
	}
	fprintf (hf, "};\n");
	fprintf (hf, "char *%s_name (enum %s val);\n", name, name);
	fprintf (hf, "\n");

	fprintf (cf, "char *\n");
	fprintf (cf, "%s_name (enum %s val)\n", name, name);
	fprintf (cf, "{\n");
	fprintf (cf, "\treturn (sym_lookup (%s_desc, %d, val));\n",
		 name, nsyms);
	fprintf (cf, "}\n");
	fprintf (cf, "\n");
}

void
rename_if_changed (char *oldname, char *newname)
{
	FILE *oldf, *newf;
	int oldc, newc;

	if ((newf = fopen (newname, "r")) != NULL) {
		if ((oldf = fopen (oldname, "r")) == NULL) {
			fprintf (stderr, "can't open %s\n", oldname);
			exit (1);
		}

		while (1) {
			oldc = getc (oldf);
			newc = getc (newf);
			if (oldc != newc)
				break;
			if (oldc == EOF)
				return;
		}
	}

	printf ("updated %s\n", newname);
	rename (oldname, newname);
}

void
do_usyms (char *filename)
{
	FILE *f;
	char buf[1000];
	char *p;
	char name[1000];
	int val;

	if ((f = fopen (filename, "r")) == NULL) {
		fprintf (stderr, "can't open %s\n", filename);
		exit (1);
	}

	fprintf (hf, "/* usyms for %s */\n", filename);

	while (fgets (buf, sizeof buf, f) != NULL) {
		p = buf;
		if (strncmp (p, "-2 ", 3) == 0)
			p += 3;
		if (sscanf (p, "%s A-MEM %o", name, &val) == 2) {
			for (p = name; *p; p++)
				if (*p == '-')
					*p = '_';
			fprintf (hf, "#define %s (vm[AMEM_ADDR + %#o])\n", name, val);
			fprintf (hf, "static const unsigned int _%s = (AMEM_ADDR + %#o);\n", name, val);
		}
	}

	fclose (f);
}



#define SYMS(name) do_syms (#name, name##_desc, nelts (name##_desc))

int
main (int argc, char **argv)
{
	remove ("TMP.h");
	if ((hf = fopen ("TMP.h", "w")) == NULL) {
		fprintf (stderr, "can't open TMP.h\n");
		exit (1);
	}
	fprintf (hf, "/* generated by mksyms */\n\n");
	fprintf (hf, "#ifndef _SYMS_H_\n");
	fprintf (hf, "#define _SYMS_H_\n");
	fprintf (hf, "\n");
	
	remove ("TMP.c");
	if ((cf = fopen ("TMP.c", "w")) == NULL) {
		fprintf (stderr, "can't open TMP.c\n");
		exit (1);
	}
	fprintf (cf, "/* generated by mksyms */\n\n");
	
	fprintf (cf, "#include \"mvm.h\"\n");
	fprintf (cf, "#include \"sym.h\"\n");
	fprintf (cf, "#include \"symdesc.h\"\n");
	fprintf (cf, "\n");

	SYMS (data_type_numbers);
	SYMS (sys_com);
	SYMS (array_header_fields);
	SYMS (stack_group_qs);
	SYMS (fefh_constant_values);
	SYMS (m_inst_buffer);
	SYMS (linear_pdl_call_state);
	SYMS (linear_pdl_exit_state);
	SYMS (linear_pdl_entry_state);
	SYMS (dests);
	SYMS (cdr_codes);
	SYMS (areas);
	SYMS (m_flags);
	SYMS (header_types);
	SYMS (fefhi);
	SYMS (fefhi2);
	SYMS (error_substatus);

	SYMS (fef_specialness);
	SYMS (fef_des_dt);
	SYMS (fef_quote_status);
	SYMS (fef_arg_syntax);
	SYMS (fef_init_option);
	SYMS (symbol);
	SYMS (arith_1arg);
	SYMS (instance_descriptor);
	SYMS (arith_2arg);
	SYMS (number_code);
	SYMS (array_types);
	SYMS (sg_states);
	SYMS (adi_fields);
	SYMS (adi_kinds);
	SYMS (region_space_type);
	SYMS (region_bits);
	SYMS (arg_desc);
	SYMS (numarg);

	do_usyms ("ucadr.sym.841");

	fprintf (hf, "#endif /* _SYMS_H_ */\n");

	fclose (hf);
	fclose (cf);

	rename_if_changed ("TMP.h", "syms.h");
	rename_if_changed ("TMP.c", "syms.c");

	return (0);
}
