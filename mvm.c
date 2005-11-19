/*
 * in function call stuff M-S is really A_IPMARK - see start of QMRCL in ucadr
 */

#include "mvm.h"

int dflags;
#define DBG_QMLP 1
#define DBG_QIMOVE 2
#define DBG_QAQT 4
#define DBG_ARR 8
#define DBG_QAFE 0x10
#define DBG_MISC 0x20
#define DBG_QIEQL 0x40


typedef unsigned int Q;


void QADCM2 (void);
void QLLV (void);
void SG_ENTER (void);
void SGENT (void);
void QMDDR (int QMDDR0_flag);
void QBND4 (Q addr);
void D_QMRCL (void (*leave)(void));
void XMEMQ1 (Q needle, Q haystack);
void QIGRP (void);

Q macro_inst;

void QMRCL (void (*leavefunc)(void));
void QCAR (void);
void QCDR (void);

int macro_count;

void fetch_macro_inst (void);



Q mcr_amem[AMEM_SIZE];

#define LOWEST_A_MEM_VIRTUAL_ADDRESS 76776000	// MUST BE 0 MODULO SIZE OF A-MEM
#define LOWEST_IO_SPACE_VIRTUAL_ADDRESS 077000000 // BEGINING OF X-BUS IO SPACE


Q mcr_micro_code_symbol_area[256 * 2];

#undef A_LOCALP
int A_LOCALP; /* may be set to -1, so must be signed */

#undef A_IPMARK
int A_IPMARK;

#undef A_AP
int A_AP;


void cc (void) __attribute__ ((noreturn));
void cc (void);

int disassemble (char *buf, Q fef, int pc);

int i_arg;

Q vma, md;


#define POINTER_BITS 24
#define ALL_BUT_POINTER_BITS 8
#define ALL_BUT_POINTER_LOW_MASK ((1 << ALL_BUT_POINTER_BITS) - 1)

#define CH_CHAR 0xff

#define Q_POINTER   0x00ffffff
#define Q_DATA_TYPE 0x1f000000
#define Q_FLAG_BIT  0x20000000
#define Q_CDR_CODE  0xc0000000

#define Q_TYPED_POINTER 0x1fffffff

#define Q_ALL_BUT_POINTER 0xff000000
#define Q_ALL_BUT_CDR_CODE 0x3fffffff

#define BOXED_SIGN_BIT 0x00800000


Q LC_fef;
Q LC_pc;

void print_q_verbose (Q);

void gdb_print_q (Q);
void gdb_print_q_verbose (Q);



void ILLOP (char *str) __attribute__ ((noreturn));
void
ILLOP (char *str)
{
	printf ("*** ILLOP *** %s\n", str);
	printf ("Current function: ");
	print_q_verbose (LC_fef);
	printf ("\n");

	exit (0);

	cc ();
}

void trap (char *str) __attribute__ ((noreturn));
void
trap (char *str)
{
	ILLOP ("trap");
}

void
dump (void *buf, int n)
{
	int i;
	int j;
	int c;

	for (i = 0; i < n; i += 16) {
		printf ("%04x: ", i);
		for (j = 0; j < 16; j++) {
			if (i+j < n)
				printf ("%02x ", ((unsigned char *)buf)[i+j]);
			else
				printf ("   ");
		}
		printf ("  ");
		for (j = 0; j < 16; j++) {
			c = ((unsigned char *)buf)[i+j] & 0x7f;
			if (i+j >= n)
				putchar (' ');
			else if (c < ' ' || c == 0x7f)
				putchar ('.');
			else
				putchar (c);
		}
		printf ("\n");

	}
}

void
usage (void)
{
	fprintf (stderr, "usage: mvm\n");
	exit (1);
}

#define VM_WORDS (16 * 1024 * 1024)
unsigned int vm[VM_WORDS];

#define MCR_WORDS (148 * 1024 / 4)
unsigned int mcr[MCR_WORDS];

unsigned int
readq (unsigned int waddr)
{
	return (vm[waddr]);
}

enum data_type_numbers
q_data_type (unsigned int q)
{
	return ((q >> 24) & 0x1f);
}

Q
q_pointer (unsigned int q)
{
	return (q & Q_POINTER);
}

Q
q_typed_pointer (unsigned int q)
{
	return (q & (Q_POINTER | Q_DATA_TYPE));
}

Q
q_all_but_typed_pointer (unsigned int q)
{
	return (q & (Q_CDR_CODE | Q_FLAG_BIT));
}

Q
q_cdr_code (Q q)
{
	return ((q >> 30) & 3);
}

Q
q_flag_bit (Q q)
{
	return ((q >> 29) & 1);
}

void
print_q (Q q)
{
	printf ("#<");
	printf ("%d:%d ", q_cdr_code (q), q_flag_bit (q));
	printf ("%s %#o>",
		data_type_numbers_name (q_data_type (q)),
		q_pointer (q));
}

#define FIX_ZERO 0x05000000
#define SYM_NIL  0x03000000
#define SYM_T    0x03000005


#define M_CAR_SYM_MODE 0 /* value? */
#define M_CDR_SYM_MODE 0 /* value? */

void
vmwrite (Q addr, Q val)
{
	vma = addr;
	md = val;
	vm[vma & Q_POINTER] = md;
}

Q
vmread (Q addr)
{
	vma = addr;
	md = vm[vma & Q_POINTER];
	return (md);
}

int
trans_trap (void)
{
	if ((i_arg & 020) == 0)
		trap ("trans-trap");
	return (1); /* drop through */
}

int
trans_oqf (void)
{
	Q addr;

	// IGNORE OQF IF JUST CHECKING CDR CODE
	if (i_arg & 010)
		return (1); /* drop through */

	if (i_arg & 4)
		ILLOP ("trans oqf"); // BARF IF TRANSPORT-HEADER

	// TRANS-INVZ
	addr = q_pointer (md);
	if (addr >= LOWEST_IO_SPACE_VIRTUAL_ADDRESS)
		ILLOP ("trans_oqf io space");

	vma = (vma & Q_ALL_BUT_POINTER) | addr;
	md = vmread (vma);
	return (0);
}

void
trans_hfwd (void)
{
	ILLOP ("trans_hfwd unimp");
}

void
trans_bfwd (void)
{
	ILLOP ("trans_bfwd unimp");
}

int
trans_evcp (void)
{
	if ((i_arg & 1) == 0) {
		// TRANS-EVCP-1
		if (i_arg & 4)
			ILLOP ("trans evcp"); // BARF IF TRANSPORT-HEADER
		return (1);
	}

	// TRANS-INVZ
	vma = (vma & Q_ALL_BUT_POINTER) | (md & Q_POINTER);
	md = vmread (vma);
	return (0);
}

Q
vmread_transport_guts (Q q, int arg)
{
	i_arg = arg;

	vma = q;
	while (1) {
		md = vm[vma & Q_POINTER];
		switch (q_data_type (md)) {
		case dtp_trap: if (trans_trap ())return (md); goto next;
		case dtp_null: if (trans_trap ())return (md); goto next;
		case dtp_free: if (trans_trap ())return (md); goto next;
		case dtp_symbol: return (md);
		case dtp_symbol_header: return (md);
		case dtp_fix: return (md);
		case dtp_extended_number: return (md);
		case dtp_header: return (md);
		case dtp_gc_forward: ILLOP ("transport gc_forward"); goto next;
		case dtp_external_value_cell_pointer: if (trans_evcp())return(md); goto next;
		case dtp_one_q_forward: if(trans_oqf())return(md); goto next;
		case dtp_header_forward: trans_hfwd (); goto next;
		case dtp_body_forward: trans_bfwd (); goto next;
		case dtp_locative: return (md); 
		case dtp_list: return (md);
		case dtp_u_code_entry: return (md);
		case dtp_fef_pointer: return (md);
		case dtp_array_pointer: return (md);
		case dtp_array_header: return (md);
		case dtp_stack_group: return (md);
		case dtp_closure: return (md);
		case dtp_small_flonum: return (md);
		case dtp_select_method: return (md);
		case dtp_instance: return (md);
		case dtp_instance_header: return (md);
		case dtp_entity: return (md);
		case dtp_stack_closure: return (md);
		}

		trap ("unhandled dtp in transport guts");
	next:;
	}
}

void
trans_old0 (void)
{
	/* XXX no old space yet... */
}

Q
vmread_transport (Q q)
{
	return (vmread_transport_guts (q, 1));
}

Q
vmread_transport_no_trap (Q q)
{
	return (vmread_transport_guts (q, 021));
}

Q
vmread_transport_write (Q q)
{
	return (vmread_transport_guts (q, 023));
}

Q
vmread_transport_cdr (Q q)
{
	return (vmread_transport_guts (q, 032));
}

Q
vmread_transport_no_evcp (Q q)
{
	return (vmread_transport_guts (q, 020));
}

Q
vmread_transport_header (Q q)
{
	return (vmread_transport_guts (q, 4));
}

Q
vmread_transport_ac (Q q)
{
	return (vmread_transport_guts (q, 020));
}

void
read_transport_header (void)
{
	i_arg = 4;

	md = vm[vma & Q_POINTER];
}

void
STACK_CLOSURE_TRAP (void)
{
	ILLOP ("stack closure trap");
}

void
vmwrite_gc_write_test (Q addr, Q val)
{
	vmwrite (addr, val);
	switch (q_data_type (val)) {
	case dtp_stack_closure:
		STACK_CLOSURE_TRAP ();
		break;
	default:
		break;
	}
}

void
print_q_dbg (char *str, Q val)
{
	printf ("%s ", str);
	print_q (val);
	printf ("\n");
}

Q arr_base; /* M-A */
Q arr_hdr; /* M-B */
Q arr_ndim; /* M-D */
Q arr_data; /* M-E */
Q arr_nelts; /* M-S */
Q arr_idx; /* M-Q element number */

#define get_field(val,len,pos) (((val) >> (pos)) & ((1 << (len)) - 1))
#define put_field(val,len,pos) (((val) & ((1 << (len)) - 1)) << (pos))

Q
ldb (Q ppss, Q val)
{
	Q pp, ss;

	if (ppss > 07777)
		ILLOP ("bad ldb");
	pp = BIT_POS (ppss);
	ss = BIT_SIZE (ppss);
	return (((val) >> pp) & ((1 << ss) - 1));
}

Q
dpb (Q val, Q ppss, Q into)
{
	Q pp, ss, mask;

	if (ppss > 07777)
		ILLOP ("bad dpb");
	pp = BIT_POS (ppss);
	ss = BIT_SIZE (ppss);
	
	mask = ((1 << ss) - 1) << pp;
	return ((into & ~mask) | ((val << pp) & mask));
}

void
gahd1 (void)
{
	/* GAHD1 */
	vma = arr_base;
	read_transport_header ();
	arr_base = vma; /* may have forwarded */
	if (q_data_type (md) != dtp_array_header)
		ILLOP ("not array header");
	arr_hdr = md;

	arr_ndim = ldb (ARRAY_NUMBER_OF_DIMENSIONS, arr_hdr);

	if (dflags & DBG_ARR)
		printf ("ndim = %d\n", arr_ndim);

	arr_data = arr_base + arr_ndim;

	if (ldb (ARRAY_LONG_LENGTH_FLAG, arr_hdr) == 0) {
		if (dflags & DBG_ARR)
			printf ("short\n");
		arr_nelts = ldb (ARRAY_INDEX_LENGTH_IF_SHORT, arr_hdr);
	} else {
		arr_data++; /* skip size word */
		arr_nelts = vmread (arr_base + 1); /* no transport, since just touched header */
	}

	if (dflags & DBG_ARR) {
		print_q_dbg ("arr base", arr_base);
		print_q_dbg ("arr hdr", arr_hdr);
		print_q_dbg ("ndim", arr_ndim);
		print_q_dbg ("data", arr_data);
		print_q_dbg ("nelts", arr_nelts);
	}
}		

Q pdl_data_base;
Q pdl_top;
Q pdl_limit;

/* stack group defined in lispm2/sgdefs.lisp */
void
sg_load_static_state (void)
{
	Q base;

	if (q_data_type (A_QCSTKG) != dtp_stack_group)
		ILLOP ("not stack group");

	base = vmread_transport (A_QCSTKG - (2 + SG_REGULAR_PDL));
	printf ("sg reg pdl ");
	print_q (base);
	printf ("\n");

	arr_base = base;
	gahd1 ();

	pdl_data_base = arr_data;
	/* hi and lo pdl limits */
	A_QLPDLO = arr_data;
	A_QLPDLH = arr_data + arr_nelts;

	unsigned int lim = q_pointer (vmread (A_QCSTKG-(2+SG_REGULAR_PDL_LIMIT)));
	if (lim < arr_nelts)
		A_QLPDLH = arr_data + lim;

	pdl_limit = A_QLPDLH - A_QLPDLO;

	arr_base = vmread_transport (A_QCSTKG - (2 + SG_SPECIAL_PDL));
	print_q_dbg ("spdl", arr_base);
	gahd1 ();

	A_QLBNDO = q_pointer (arr_data);
	A_QLBNDRH = arr_data + arr_nelts;
	A_QLBNDH = A_QLBNDRH;	

	lim = q_pointer (vmread (A_QCSTKG - (2 + SG_SPECIAL_PDL_LIMIT)));
	if (lim < arr_nelts)
		A_QLBNDH = arr_data + lim;
}

void
pdl_push (Q val)
{
	if (pdl_top + 1 >= pdl_limit)
		ILLOP ("pdl overflow");

	pdl_top++;
	vm[q_pointer (pdl_data_base + pdl_top)] = val;
}

Q
pdl_pop (void)
{
	Q val;

	if (pdl_top == 0)
		ILLOP ("pdl underflow");

	val = vm[q_pointer (pdl_data_base + pdl_top)];
	pdl_top--;
	return (val);
}

Q
pdl_peek (void)
{
	return (vm[q_pointer (pdl_data_base + pdl_top)]);
}

Q
pdl_ref (Q off)
{
	if (off >= pdl_limit)
		ILLOP ("pdl_ref");

	return (vm[q_pointer (pdl_data_base + off)]);
}

void
pdl_store (Q off, Q val)
{
	if (off >= pdl_limit)
		ILLOP ("pdl_store");

	vm[q_pointer (pdl_data_base + off)] = val;
}

void
get_area_origins (void)
{
	Q in, out, end;
	int nareas;
	int areanum;

	nareas = &A_V_FIRST_UNFIXED_AREA - &A_V_RESIDENT_SYMBOL_AREA;

	in = vmread (0400 + SYS_COM_AREA_ORIGIN_PNTR);
	out = &A_V_RESIDENT_SYMBOL_AREA - vm;

	for (areanum = 0; areanum < nareas; areanum++) {
		vmwrite (out, vmread (in));
		in++;
		out++;
	}

	areanum--;
	end = A_V_INIT_LIST_AREA + vmread (A_V_REGION_LENGTH + areanum);
	
	end += (040000 - 1);
	end &= ~037777;
	
	A_V_FIRST_UNFIXED_AREA = end;
}


#define PDL_BUFFER_SLOP 40 /* decimal! */
#define PDL_BUFFER_HIGH_LIMIT (02000 - (0400 + PDL_BUFFER_SLOP))


void
beg0000 (void)
{
	A_FLAGS = FIX_ZERO
		| M_CAR_SYM_MODE
		| M_CDR_SYM_MODE;

	A_SB_SOURCE_ENABLE = SYM_NIL;
	A_TV_CURRENT_SHEET = SYM_NIL;
	A_SCAV_COUNT = 0;
	A_INHIBIT_SCHEDULING_FLAG = SYM_T;
	A_INHIBIT_SCAVENGING_FLAG = SYM_T;

	/* fetch miscellaneous scratchpad locs */
	A_AMCENT = vm[01031]; /* number of active microcode entries */
	A_CNSADF = vm[01021]; /* default area for consing */
	A_NUM_CNSADF = A_CNSADF; /* DONT REALLY HACK EXTRA-PDL INITIALLY. */

	get_area_origins ();

	/* FIRST 200 MICRO ENTRIES ARE NOT IN TABLE */
	A_V_MISC_BASE = A_V_MICRO_CODE_SYMBOL_AREA - 0200;

	A_INITIAL_FEF = vmread_transport (01000);
	A_QTRSTKG = vmread_transport (01001);
	A_QCSTKG = vmread_transport (01002);
	A_QISTKG = vmread_transport (01003);

	printf ("initial fef ");
	print_q (A_INITIAL_FEF);
	printf ("\n");

	A_INITIAL_FEF = vmread_transport (A_INITIAL_FEF);
	printf ("initial fef ");
	print_q (A_INITIAL_FEF);
	printf ("\n");

	printf ("initial sg ");
	print_q (A_QISTKG);
	printf ("\n");
	A_QCSTKG = A_QISTKG;

	sg_load_static_state ();

	/*
	 * INITIALIZE BINDING PDL POINTER
	 * POINTS AT VALID LOCATION, OF WHICH THERE ARENT ANY YET.
	 */
	A_QLBNDP = A_QLBNDO - 1;

	pdl_top = -1;
	pdl_push (FIX_ZERO);
	pdl_push (FIX_ZERO);
	pdl_push (FIX_ZERO);
	pdl_push (A_INITIAL_FEF);

	if (q_data_type (A_INITIAL_FEF) != dtp_fef_pointer)
		ILLOP ("bad initial fef");

	A_AP = pdl_top;

	A_ERROR_SUBSTATUS = 0;
	A_IPMARK = A_AP; /* NO OPEN CALL BLOCK YET */

	LC_fef = A_INITIAL_FEF;

	vma = LC_fef;
	read_transport_header ();
	LC_pc = ldb (FEFH_PC, md);
}

enum reg {
	reg_fef,
	reg_fef100,
	reg_fef200,
	reg_fef300,
	reg_const,
	reg_local,
	reg_arg,
	reg_pdl,
};


// QLLOCB
void
set_local_block (int pdl_idx)
{
	A_LOCALP = pdl_idx;
	pdl_top = pdl_idx - 1; // push pre-increments
}

#define make_pointer(type,addr) (((type) << 24) | ((addr) & Q_POINTER))
#define make_pointer_cdr_next(type,addr) (((type) << 24) | ((addr) & Q_POINTER) | (CDR_NEXT<<30))

Q
convert_pdl_buffer_address (int pdl_idx)
{
	return (make_pointer (dtp_locative, pdl_data_base + pdl_idx));
}

/*
 * EFFECTIVE ADDRESS COMPUTATION ROUTINES.
 * THESE ARE ENTERED FROM QADCM1, THEY PUT THE OPERAND INTO M-T AND POPJ.
 * Q-ALL-BUT-TYPED-POINTER bits should be zero.
 */
void
QAFE (void)
{
	Q fef;
	int offset;

	offset = ldb (M_INST_ADR, macro_inst);
	fef = pdl_ref (A_AP); /* 0(AP) -> FEF (LC_fef may be safe here) */

	A_T = q_typed_pointer (vmread_transport (fef + offset));

	if (dflags & DBG_QAFE) {
		printf ("QAFE ");
		print_q_verbose (A_T);
		printf ("\n");
	}
}

void
QAQT (void)
{
	int offset;

	offset = ldb (M_INST_DELTA, macro_inst);
	md = vmread_transport (A_V_CONSTANTS_AREA + offset);
	A_T = q_typed_pointer (md);

	if (dflags & DBG_QAQT)
		print_q_dbg ("QAQT", A_T);
}

void
QADARG (void)
{
	// REF ARGUMENT BLOCK.  CANNOT BE INVISIBLE POINTER.
	// QADARG
	A_T = q_typed_pointer (pdl_ref (A_AP
					+ ldb (M_INST_DELTA, macro_inst)
					+ LP_INITIAL_LOCAL_BLOCK_OFFSET));
}

// REF LOCAL BLOCK.  CANNOT BE INVISIBLE POINTER.
// IF THIS IS EVER CHANGED TO ALLOW INVISIBLE POINTERS, GOT TO FOOL AROUND
// WITH THE QADCM3 DISPATCH AND THE THINGS THAT USE IT, AND DECIDE WHICH
// FLAVORS OF INVISIBILITY DO WHAT WITH RESPECT TO VALUE-CELL-LOCATION.
void
QADLOC (void)
{
	A_T = q_typed_pointer (pdl_ref (A_LOCALP
					+ ldb (M_INST_DELTA, macro_inst)));
}

// REF PDL.  CANNOT BE INVISIBLE POINTER.
// WE ONLY SUPPORT (SP)+ TYPE PDL ADDRESSING (PDL 77) 
void
QADPDL (void) 
{
	A_T = pdl_pop ();
	if (ldb (M_INST_DELTA, macro_inst) != 077)
		trap ("QADPDL bad offset");
}

// dispatch tables QADCM1   QADCM5
// replace (DISPATCH-CALL M-INST-REGISTER QADCM1) with fetch_arg ()
// get arg into A_T
void
fetch_arg (void)
{
	switch (ldb (M_INST_REGISTER, macro_inst)) {
	case reg_fef: QAFE (); break;
	case reg_fef100: QAFE (); break;
	case reg_fef200: QAFE (); break;
	case reg_fef300: QAFE (); break;
	case reg_const: QAQT (); break;
	case reg_local: QADLOC (); break;
	case reg_arg: QADARG (); break;
	case reg_pdl: QADPDL (); break;
	}
}

void
open_call_block (int dest)
{
	Q lpcls;
	int new_ipmark;

	new_ipmark = pdl_top + LP_CALL_BLOCK_LENGTH;	

	lpcls = FIX_ZERO;
	lpcls = dpb (new_ipmark - A_IPMARK, LP_CLS_DELTA_TO_OPEN_BLOCK, lpcls);
	lpcls = dpb (new_ipmark - A_AP, LP_CLS_DELTA_TO_ACTIVE_BLOCK, lpcls);
	lpcls = dpb (dest, LP_CLS_DESTINATION, lpcls);

	pdl_push (lpcls);

	// QBNEAF QBALM WOULD GO HERE IF EVER REVIVED

	pdl_push (FIX_ZERO); // Push LPEXS Q
	pdl_push (FIX_ZERO); // Push LPENS Q
	pdl_push (A_T); // Push LPFEF Q

	A_IPMARK = new_ipmark;  // A_IPMARK -> new open block
}

/* CALL WITH ARGS.  JUST OPEN A macro to macro CALL BLOCK. */
void
QICALL(void) 
{
	fetch_arg ();
	open_call_block (ldb (M_INST_DEST, macro_inst));
}

// CALL WITH NO ARGS
void
QICAL0 (void)
{
	// QICAL0
	fetch_arg (); // FEF -> A_T
	open_call_block (ldb (M_INST_DEST, macro_inst));
	QMRCL (QLLV);
}

// POP A BLOCK OF BINDINGS
// POP A BINDING (MUSTN'T BASH M-T, M-J, M-R, M-D, M-C)

// microcode call to BBLK -> BBLKP (0)
// microcode call to QUNBND  -> BBLKP (1)
void
BBLKP (int just_one_flag)
{
	Q cell, val, cell_bits;
	while (1) {
		// Get pntr to bound cell
		cell = vmread_transport_no_evcp (A_QLBNDP); 
		A_QLBNDP -= 2;
		// Previous contents
		val = vmread_transport_no_evcp (A_QLBNDP + 1); 

		if (q_data_type (cell) != dtp_locative)
			ILLOP ("BBLKP");
	
		// get cdr_code and flag bit from bound cell
		cell_bits = vmread (cell);
		vmwrite (cell,
			 q_all_but_typed_pointer (cell_bits)
			 | q_typed_pointer (val));

		// ucadr had fancy check-page-write for a mem

		if (just_one_flag || q_flag_bit (val))
			break;
	}

	cell = FIX_ZERO; /* don't leave EVCP in reg */

	if (just_one_flag == 0)
		A_FLAGS = dpb (0, M_FLAGS_QBBFL, A_FLAGS);
	
	if (ldb (M_FLAGS_DEFERRED_SEQUENCE_BREAK, A_FLAGS) != 0
	    && q_typed_pointer (A_INHIBIT_SCHEDULING_FLAG) == SYM_NIL) {
		A_FLAGS = dpb (0, M_FLAGS_DEFERRED_SEQUENCE_BREAK, A_FLAGS);
		ILLOP ("sequence break unimp");
	}
}

void STACK_CLOSURE_RETURN_TRAP (void) { ILLOP ("unimp"); }

void MVR (void) {ILLOP("mvr unimp"); }
void QMMPOP(void) {ILLOP("QMMPOP unimp"); }
void QMXSG (void) { ILLOP ("QMXSG"); }

// Push a micro-to-macro call block (just the first 3 words, not the function)
void
P3ZERO (void)
{
	pdl_push (FIX_ZERO);

	// P3ZER1
	A_ZR = pdl_top + LP_CALL_BLOCK_LENGTH - 1; // 1- because of push just done
	
	A_TEM1 = dpb (D_MICRO, LP_CLS_DESTINATION, 0);
	A_TEM1 = dpb (A_ZR - A_IPMARK, LP_CLS_DELTA_TO_OPEN_BLOCK, A_TEM1);
	A_TEM1 = dpb (A_ZR - A_AP, LP_CLS_DELTA_TO_ACTIVE_BLOCK, A_TEM1);

	pdl_store (pdl_top, pdl_ref (pdl_top) | A_TEM1); // IOR with LPCLS Q already pushed
	pdl_push (FIX_ZERO); // Push LPEXS Q
	pdl_push (FIX_ZERO); // Push LPENS Q
	A_IPMARK = A_ZR;
	// Caller must push LPFEF Q
}

void MLLV (void) { ILLOP ("MLLV"); }

// Activate a pending micro-to-macro call block.
// ((ARG-CALL MMCALL) (I-ARG number-args-pushed)) if you want to get back the
// result of the function.  You can receive multiple values if you opened
// the call by pushing ADI and calling P3ADI rather than P3ZERO.
void
MMCALL (int i_arg)
{
	Q idx;

	A_R = i_arg;

	// MMCAL4
	idx = pdl_top - A_R; // Address of new frame
	A_S = idx; // Must be in both M-S and PDL-BUFFER-INDEX
	if (A_S != (Q)A_IPMARK)
		ILLOP ("MMCALL"); // Frame not where it should be.  M-R lied?
	A_A = pdl_ref (idx); // M-A := FUNCTION TO CALL
	QMRCL (MLLV);
}


// MUST KNOW ABOUT THESE WHEN GRUBBLING STACK.. 
// NOTE U-CODE ENTRY #'S ARE NORMALLY 
// UNCONSTRAINED AND DETERMINE ONLY POSITION
// IN MICRO-CODE-ENTRY-AREA, ETC. 
#define CATCH_U_CODE_ENTRY 0

void
QMDDR_THROW (void)
{
	Q idx, val;

	// Here from QMDDR if there are open call blocks in this frame.  It could
	// be an UNWIND-PROTECT, so we come here to check it out by doing a throw
	// of the value being returned, to the tag 0.
	// QMDDR-THROW
	A_CATCH_MARK = make_pointer (dtp_u_code_entry, CATCH_U_CODE_ENTRY);
	A_CATCH_ACTION = A_V_NIL;
	A_CATCH_COUNT = A_V_NIL;
	A_CATCH_TAG = FIX_ZERO;

	// drop into XTHRW7

	// This is the main throw loop.  Come here for each frame.
XTHRW7:

	A_R = A_V_NIL; // GET NIL ON THE M SIDE FOR LATER
	if (A_AP == A_IPMARK) // LAST FRAME ACTIVE, UNWIND IT
		goto XTHRW1;

	// LAST FRAME OPEN, NOTE IT MUST ALREADY BE IN
	// PDL BUFFER, SINCE ENTIRE ACTIVE FRAME IS.
	A_I = A_IPMARK;

	A_A = q_typed_pointer (pdl_ref (A_IPMARK));

	if (A_A != A_CATCH_MARK) // THATS NOT WHAT LOOKING FOR
		goto XTHRW2;

	A_A = q_typed_pointer (A_IPMARK + 1);

	if (A_A == A_V_TRUE) // FOUND UNWIND-PROTECT, RESUME IT
		goto XTHRW4;

	A_1 = A_V_TRUE;
	if (A_CATCH_TAG == A_V_TRUE) // IF UNWINDING ALL THE WAY, KEEP LOOKING
		goto XTHRW2;

	if (A_A == A_V_NIL) // FOUND CATCH-ALL, RESUME IT
		goto XTHRW4;

	if (A_A != A_CATCH_TAG) // DIDN'T FIND RIGHT TAG, KEEP LOOKING
		goto XTHRW2;


	// FOUND FRAME TO RESUME
	// XTHRW4	
XTHRW4:
	A_B = q_typed_pointer (pdl_ref (A_I + LP_CALL_STATE)); // PRESERVE FOR USE BELOW

	if (ldb (LP_CLS_ADI_PRESENT, A_B) == 0) // NO ADI, HAD BETTER BE DESTINATION RETURN
		goto XTHRW9;

	A_D = A_I - LP_CALL_BLOCK_LENGTH;

XTHRW3:
	if (q_data_type (pdl_ref (A_D)) != dtp_fix)
		trap ("DATA-TYPE-SCREWUP ADI");

	A_A = ldb (ADI_TYPE, pdl_ref (A_D));
	if (A_A != ADI_RESTART_PC)
		goto XTHRW8;

	A_J = ldb (ADI_RPC_MICRO_STACK_LEVEL, pdl_ref (A_D));
			 
	A_E = q_pointer (A_D - 1);

	A_TEM = q_data_type (pdl_ref (A_AP));

	// To make *CATCH in a micro-compiled function work will require more hair
	if (A_TEM != dtp_fef_pointer)
		ILLOP ("bad catch");

	// Change frame's return PC to restart PC
	A_TEM = pdl_ref (A_AP + LP_EXIT_STATE);
	pdl_store (A_AP + LP_EXIT_STATE,
		   dpb (A_E, LP_EXS_EXIT_PC, A_TEM));

XTHRW5:
	ILLOP ("micro stack stuff");
#if 0
	// Pop micro-stack back to specified level
	A_ZR = MICRO_STACK_POINTER;
	if (A_ZR == A_J)
		goto XTHRW6;
	if (A_ZR < A_J) // Already popped more than that?
		ILLOP ("micro stack");

	A_ZR = MICRO_STACK_DATA_POP ();

	if (M_ZR & PPBSPC)
		BBLKP ();
	goto XTHRW5;
#endif


XTHRW6A:
	BBLKP (1); // POP BINDING AND DROP THRU. CLOBBERS M-C.

//XTHRW6:
	// ON ENTRY HERE, M-D HAS PDL-BUFFER INDEX OF ADI-RESTART-PC ADI, OR -1 IF NONE.
	if (A_D & 0x80000000) {
		// IF ENCOUNTERED *CATCH W/O ADI-RESTART-PC ADI,
		// DONT TRY TO HACK BIND STACK.  THIS CAN HAPPEN VIA INTERPRETED 
		// *CATCH S.  SINCE FRAME DESTINATION MUST BE D-RETURN,
		// NO NEED TO HACK BIND STACK ANYWAY.
		goto XTHRW6B;
	}

	// MOVE BACK TO THE DATA Q 
	// PREVIOUS ADI BLOCK WHICH HAD BETTER BE AN ADI-BIND-STACK-LEVEL BLOCK
	// GET BIND-STACK-LEVEL
	A_B = q_pointer (pdl_ref (A_D - 3));

	// SIGN EXTEND SINCE EMPTY STACK IS LEVEL OF -1
	if (A_B & BOXED_SIGN_BIT)
		A_B |= -1 << 24;
	A_J = A_QLBNDP - A_QLBNDO; // COMPUTE CURRENT RELATIVE STACK LEVEL
	
	if (A_J < A_B)
		ILLOP ("XTHRW6"); // ALREADY OVERPOPPED?
	if (A_J != A_B) // EVIDENTLY A BIND WAS DONE WITHIN THIS BLOCK ..
		goto XTHRW6A;

	// STORE BACK QBBFL WHICH MAY HAVE BEEN CLEARED
	val = pdl_ref (A_AP + LP_EXIT_STATE);
	if (ldb (M_FLAGS_QBBFL, A_FLAGS))
		val = dpb (1, LP_EXS_PC_STATUS, val);
	else
		val = dpb (0, LP_EXS_PC_STATUS, val);
	pdl_store (A_AP + LP_EXIT_STATE, val);

XTHRW6B:
	idx = A_I - A_AP; // THIS EFFECTIVELY CANCELS WHAT WILL BE DONE AT QMEX1
	A_PDL_BUFFER_ACTIVE_QS += idx;

	A_AP = A_I; // SIMULATE ACTIVATING CATCH FRAME

	// IF THROWING OUT TOP, DON'T STOP ON UNWIND-PROTECT, GO WHOLE WAY
	if (A_CATCH_TAG == A_V_TRUE) 
		goto XTHRW6D;

	// ACTION NON-NIL => DONT REALLY RESUME EXECUTION, CALL FUNCTION INSTEAD.
	if (A_CATCH_ACTION != A_R)
		goto XUWR2;

XTHRW6D:
	ILLOP ("micro stack stuff");
#if 0
	MICRO_STACK_DATA_PUSH (QMEX1);
	A_S = 0;
	// FIRST VALUE IS VALUE THROWN (STILL IN M-T)
	XRNVR (); 

	MICRO_STACK_DATA_PUSH (QMEX1);
	A_S = 0;
	A_T = A_CATCH_TAG; // SECOND VALUE IS TAG
	XRNVR (); 

	MICRO-STACK-DATA-PUSH (QMEX1);
	A-S =  0;
	M_T = A_CATCH_COUNT;// THIRD VALUE IS COUNT
	XRNVR (); 

	A_T = A_CATCH_ACTION; // FOURTH VALUE IS ACTION
	goto QMEX1;
#endif

XTHRW8:
	idx = A_D;
	if ((pdl_ref (A_D) & Q_FLAG_BIT) == 0)
		ILLOP ("XTHRW8");

	A_D--;
	if ((pdl_ref (A_D) & Q_FLAG_BIT) == 0)
		goto XTHRW9;

	A_D--;

	goto XTHRW3;


XTHRW9:
	// RAN OUT OF ADI.  THE SAVED DESTINATION HAD BETTER BE D-RETURN OR ERROR.  THIS
	// CAN HAPPEN MAINLY THRU INTERPRETED CALLS TO *CATCH.

	idx = A_I + LP_CALL_STATE;
	A_C = ldb (LP_CLS_DESTINATION, pdl_ref (idx));
	if (A_C != D_RETURN)
		ILLOP ("XTHRW9");

	// SET FLAG THAT RESTART-PC ADI NOT FOUND, SO
	// BIND PDL HACKERY NOT ATTEMPTED.
	A_D = -1;

	A_S = ldb (LP_CLS_DELTA_TO_ACTIVE_BLOCK, pdl_ref (idx));
	A_I -= A_S;

	A_J = 0; // Flush whole micro-stack
	goto XTHRW5;

XTHRW2:
	// Skip this open frame
	A_IPMARK = A_I - ldb (LP_CLS_DELTA_TO_OPEN_BLOCK, pdl_ref (A_I + LP_CALL_STATE));
	A_IPMARK = q_pointer (A_IPMARK); // ASSURE NO GARBAGE IN A-IPMARK
	goto XTHRW7;

XTHRW1:
#if 0
	// XXX unwind micro stack here
	while (MICRO_STACK_POINTER == 0) {
		A_TEM = MICRO_STACK_DATA_POP ();
		if (A_TEM & PPBSPC)
			BBLKP ();
	}
#endif

//XTHRW1A:
	// CHECK FOR THROW TAG OF 0
	if (A_CATCH_TAG == FIX_ZERO) {
		QMDDR (1); // YES, RETURN FROM THIS FRAME
		return;
	}

	if (A_R == A_CATCH_COUNT)
		goto XTHRW1B; // JUMP IF NO COUNT

	A_CATCH_COUNT--;
	if (A_CATCH_COUNT & BOXED_SIGN_BIT)
		goto XUWR1; // REACHED MAGIC COUNT, RESUME BY RETURNING

XTHRW1B:
	if (ldb (M_FLAGS_QBBFL, A_FLAGS))
		BBLKP (0); // POP BINDING-BLOCK IF FRAME HAS ONE

	idx = A_AP + LP_CALL_STATE;
	A_TEM1 = ldb (LP_CLS_DELTA_TO_OPEN_BLOCK, pdl_ref (idx));
	A_IPMARK = A_AP - A_TEM1; // COMPUTE PREV A-IPMARK

	A_TEM1 = ldb  (LP_CLS_DELTA_TO_ACTIVE_BLOCK, pdl_ref (idx));
	
	// FLUSH PDL OFF THE BOTTOM OF THE STACK, GO CALL THE ACTION,
	// HAVING THROWN ALL THE WAY

	pdl_top = A_AP - LP_CALL_BLOCK_LENGTH;
	if (A_TEM1 == 0)
		goto XUWR2;

	if (ldb (LP_CLS_ADI_PRESENT, pdl_ref (idx))) {
		// FLUSH ADDTL INFO
		// QRAD1R
		// FLUSH ADI FROM PDL
		// no effect?? - pace 
		A_K = A_AP - (1 + LP_CALL_BLOCK_LENGTH);
		while (1) {
			if ((pdl_ref (A_K) & Q_FLAG_BIT) == 0) {
				break;
			}
			A_K -= 2;
		}
	}

	A_AP -= A_TEM1; // RESTORE M-AP

	idx = A_AP + LP_EXIT_STATE;
	if (ldb (LP_EXS_PC_STATUS, pdl_ref (idx)))
		A_FLAGS = dpb (1, M_FLAGS_QBBFL, A_FLAGS);
	else
		A_FLAGS = dpb (0, M_FLAGS_QBBFL, A_FLAGS);

	if (ldb (LP_EXS_MICRO_STACK_SAVED, pdl_ref (idx)))
		QMMPOP (); // RESTORE USTACK FROM BINDING STACK
	goto XTHRW7;

XUWR1:
	// HERE WHEN THE COUNT RUNS OUT
	if (A_CATCH_ACTION != A_R) {
		// XUWR2
		P3ZERO ();
		pdl_push (A_CATCH_ACTION);
		pdl_push (q_typed_pointer (A_T) | (CDR_NIL<<30));
		MMCALL (1);
	}
	QMDDR (1); // CAUSE ACTIVE FRAME TO RETURN VALUE
	return;

XUWR2:
	// HERE WHEN ACTION NOT NIL, IT IS A FUNCTION TO BE CALLED.
	P3ZERO ();
	pdl_push (A_CATCH_ACTION);
	pdl_push (q_typed_pointer (A_T) | (CDR_NIL<<30));
	MMCALL (1);

	// IF THROWING OUT WHOLE WAY, SHOULDN'T RETURN.  MICROSTACK MUST BE CLEAR
	// IN THIS CASE OR MLLV WILL STORE IT IN THE WRONG FRAME, BECAUSE OF THE
	// ANOMALOUS CASE OF M-AP = M-S.  IF NOT THROWING OUT WHOLE WAY, FUNCTION
	// MAY RETURN AND ITS VALUE WILL BE RETURNED FROM THE *CATCH BY THE EXIT
	// TO QMDDR0 AT XUWR1.
	
}


// DESTINATION RETURN  value in M-T.  Q-ALL-BUT-TYPED-POINTER bits must be 0.
void
QMDDR (int QMDDR0_flag)
{
QMDDR:
	if (QMDDR0_flag == 0) {
		// QMDDR
		A_TEM = q_data_type (A_T);
		
		if (A_TEM == dtp_stack_closure)
			STACK_CLOSURE_RETURN_TRAP ();
		
		if (A_AP != A_IPMARK) {
			QMDDR_THROW (); // CHECK FOR UNWIND-PROTECT
			return;
		}
	}

	// QMDDR0

	// POP BINDING BLOCK (IF STORED ONE)
	if (ldb (M_FLAGS_QBBFL, A_FLAGS))
		BBLKP (0); 

	// QMEX1

	// Save returning function for metering
	A_A = pdl_ref (A_AP);

	A_C = pdl_ref (A_AP + LP_CALL_STATE);
#if 0
	if (A_C & LP_CLS_TRAP_ON_EXIT)
		QMEX1_TRAP ();
	if (pdl_ref (A_AP + LP_ENTRY_STATE) & LP_ENS_ENVIRONMENT_POINTER_POINTS_HERE)
		QMEX1_COPY ();
#endif

	pdl_top = A_AP - LP_CALL_BLOCK_LENGTH;
	
	if (ldb (LP_CLS_ADI_PRESENT, A_C)) {
		Q idx;

		// STORE LAST VALUE IN ADI CALL, FLUSH ADI FROM PDL
		// MAY CLOBBER ALL REGISTERS EXCEPT M-C and M-A
		// QRAD1
											      
		// IN CASE WE SWITCH STACK GROUPS INSIDE MVR
		pdl_top = A_AP;
		A_K = pdl_data_base + A_AP + LP_CALL_STATE;
		A_S = 0;
		MVR (); // STORE THE LAST VALUE INTO MV IF ANY

		// QRAD1R

		// FLUSH ADI FROM PDL
		idx = A_AP - (LP_CALL_BLOCK_LENGTH + 1);
		
		// QRAD2
		while (q_flag_bit (pdl_ref (idx)) != 0) {
			ILLOP ("check this ... maybe shoudl be flag == 0");
			idx -= 2;
		}

		pdl_top = idx - 1;
	}

	A_TEM1 = ldb (LP_CLS_DELTA_TO_OPEN_BLOCK, A_C);

	if (A_TEM1 == 0) {
		QMXSG (); // RETURNING OUT TOP OF STACK-GROUP
		return;
	}

	A_TEM = A_AP - A_TEM1; // COMPUTE PREV A-IPMARK
	A_IPMARK = A_TEM; // RESTORE THAT

	A_TEM1 = ldb (LP_CLS_DELTA_TO_ACTIVE_BLOCK, A_C);
	A_AP -= A_TEM1;

	// Now restore the state of the frame being returned to.  We will restore
	// the FEF stuff even if it's not a FEF frame, at the cost of a slight
	// amount of time.

	// if (M-METER-ENABLES) METER_FUNCTION_EXIT ()
	
	A_A = pdl_ref (A_AP); // FUNCTION RETURNING TO

	A_LOCALP = A_AP + ldb (LP_ENS_MACRO_LOCAL_BLOCK_ORIGIN,
			       pdl_ref (A_AP + LP_ENTRY_STATE));

	if (ldb (LP_EXS_BINDING_BLOCK_PUSHED,
		 pdl_ref (A_AP + LP_EXIT_STATE)))
		A_FLAGS = dpb (1, M_FLAGS_QBBFL, A_FLAGS);
	else
		A_FLAGS = dpb (0, M_FLAGS_QBBFL, A_FLAGS);

	if (ldb (LP_EXS_MICRO_STACK_SAVED, pdl_ref (A_AP + LP_EXIT_STATE)))
		QMMPOP ();

	LC_fef = A_A;
	LC_pc = ldb (LP_EXS_EXIT_PC, pdl_ref (A_AP + LP_EXIT_STATE));

	if (q_data_type (LC_fef) == dtp_fef_pointer) {
		printf ("#%d: Return to function: ", macro_count);
		print_q_verbose (LC_fef);
		printf (" value = ");
		print_q_verbose (A_T);
		printf ("\n");
	} else {
		print_q_dbg ("Return to non-fef: ", LC_fef);
	}

	// QIMOVE-EXIT
	// Store into destination in M-C.  Could be D-MICRO
	// pdl_push ((A_T & Q_ALL_BUT_CDR_CODE) | (CDR_NEXT<<30));

	// QMDTBD (dest could be d micro)
	switch (ldb (LP_CLS_DESTINATION, A_C)) {
	case D_IGNORE:
		break;
	case D_PDL:
	case D_NEXT:
		pdl_push ((A_T & Q_ALL_BUT_CDR_CODE) | (CDR_NEXT<<30));
		break;

	case D_LAST:
		pdl_push ((A_T & Q_ALL_BUT_CDR_CODE) | (CDR_NIL<<30)); 
		QMRCL (QLLV);
		break;

	case D_RETURN:
		goto QMDDR;

	case D_NEXT_LIST:
		ILLOP ("D-NEXT-LIST");
		break;

	default:
		ILLOP ("bad dest");
	}
}

void
QQARY (void)
{
	A_T = q_typed_pointer (vmread_transport (arr_data + arr_idx));
	if (dflags & DBG_ARR)
		print_q_dbg ("QQARY result", A_T);
}

void QB1RY (void) {ILLOP ("unimp"); }
void QB2RY (void) {ILLOP ("unimp"); }
void QB4RY (void) {ILLOP ("unimp"); }
void QB8RY (void) {ILLOP ("unimp"); }
void QB16RY (void) {ILLOP ("unimp"); }
void QB32RY (void) {ILLOP ("unimp"); }
void QB16SRY (void) {ILLOP ("unimp"); }
void QFARY (void) {ILLOP ("unimp"); }
void QFFARY (void) {ILLOP ("unimp"); }

void
QBARY (void)
{
	// QBARY  BYTE ARRAY
	A_J = get_field (arr_idx, 2, 0);
	A_TEM1 = get_field (arr_idx, 026, 2);

	A_T = FIX_ZERO;

	md = vmread (arr_data + A_TEM1);
	A_T |= get_field (md, 8, A_J * 8);
}

// DISPATCH ON ARRAY TYPE WHEN REF ING ARRAY
void
ARRAY_TYPE_REF_DISPATCH (int array_type)
{
	switch (array_type) {
	case ART_ERROR: trap ("array ref");
	case ART_1B: QB1RY (); break;
	case ART_2B: QB2RY (); break;
	case ART_4B: QB4RY (); break;
	case ART_8B: QB8RY (); break;
	case ART_16B: QB16RY (); break;
	case ART_32B: QB32RY (); break;
	case ART_Q: QQARY (); break;
	case ART_Q_LIST: QQARY (); break;
	case ART_STRING: QBARY (); break;
	case ART_STACK_GROUP_HEAD: QQARY (); break;
	case ART_SPECIAL_PDL: QQARY (); break;
	case ART_HALF_FIX: QB16SRY (); break;
	case ART_REGULAR_PDL: QQARY (); break;
	case ART_FLOAT: QFARY (); break;
	case ART_FPS_FLOAT: QFFARY (); break;
	case ART_FAT_STRING: QB16RY (); break;
	default: trap ("array ref");
	}
}
		
void QS1RY (void) {ILLOP ("unimp");}
void QS2RY (void) {ILLOP ("unimp");}
void QS4RY (void) {ILLOP ("unimp");}
void QSBARY (void) {ILLOP ("unimp");}
void QS32RY (void) {ILLOP ("unimp");}
void QSFARY (void) {ILLOP ("unimp");}
void QSFFARY (void) {ILLOP ("unimp");}


void
QS16RY (void) 
{
	Q addr;

	if (q_data_type (A_T) != dtp_fix)
		trap ("fix");

	addr = arr_data + arr_idx / 2;
	vmread (addr);

	if ((arr_idx & 1) == 0)
		md = (md & 0xffff0000) | (A_T & 0xffff);
	else
		md = (md & 0xffff) | ((A_T << 16) & 0xffff0000);

	vmwrite (addr, md);
}

void
QSQARY (void)
{
	// QSQARY
	vmwrite_gc_write_test (arr_data + arr_idx, A_T); // Q ARRAY
}

void
QSLQRY (void)
{
	// QSLQRY
	// Q-LIST ARRAY
	// NO TRANSPORT SINCE STORING AND JUST
	// TOUCHED HEADER AND DON'T ALLOW ONE-Q-FORWARD

	vmread (arr_data + arr_idx);
	md = q_all_but_typed_pointer (md) | q_typed_pointer (A_T);
	vmwrite_gc_write_test (arr_data + arr_idx, md);
}

// ARRAY-TYPE-STORE-DISPATCH
void
ARRAY_TYPE_STORE_DISPATCH (int array_type)
{
	switch (array_type) {
	case ART_ERROR: trap ("array ref");
	case ART_1B: QS1RY (); break;
	case ART_2B: QS2RY (); break;
	case ART_4B: QS4RY (); break;
	case ART_8B: QSBARY (); break;
	case ART_16B: QS16RY (); break;
	case ART_32B: QS32RY (); break;
	case ART_Q: QSQARY (); break;
	case ART_Q_LIST: QSLQRY (); break;
	case ART_STRING: QSBARY (); break;
	case ART_STACK_GROUP_HEAD: QSQARY (); break;
	case ART_SPECIAL_PDL: QSQARY (); break;
	case ART_HALF_FIX: QS16RY (); break;
	case ART_REGULAR_PDL: QSQARY (); break;
	case ART_FLOAT: QSFARY (); break;
	case ART_FPS_FLOAT: QSFFARY (); break;
	case ART_FAT_STRING: QS16RY (); break;
	default: trap ("array ref");
	}
}

void QARY_MULTI (void) {ILLOP ("QARY_MULTI unimp");}

void QRAD1 (void) {ILLOP ("QRAD1");}

void
DSP_ARRAY_SETUP (void)
{
	Q orig_array_type, last_nelts;

	// CALL WITH ARRAY POINTER IN M-A, HEADER IN M-B, 
	// FIRST DATA ELEM IN M-E, DESIRED ELEMENT NUMBER IN M-Q.
	// RETURNS WITH DATA ORIGIN IN M-E, M-S CHANGED TO REFLECT ARRAY
	// BEING REF'ED AND POSSIBLY ADJUSTED M-Q.

	// GET NEW DATA LENGTH LIMIT
	arr_nelts = q_pointer (vmread_transport (arr_data + 1));
	arr_data = vmread_transport (arr_data); // TRANSPORT IN CASE POINTS TO OLDSPACE
	if (dflags & DBG_ARR) {
		printf ("displaced: nelts %#o; data ", arr_nelts);
		print_q (arr_data);
		printf ("\n");
	}

	if (q_data_type (arr_data) != dtp_array_pointer)
		return;

	/* else, indirect array */

	// Operation of QDACMP:
	// The word just read from memory is the array-pointer to indirect to
	// M-Q has entry number desired
	// QDACMP pushes the info relative to the indirect array (M-A, M-B, M-D).
	// M-E eventually gets the data base of the pointed-to array.
	// M-S gets MIN(M-S from indirect array + index offset, index length of pointed-to array).
	// In the process, M-Q will be adjusted if an index offset is encountered.
	// After the final data base is determined, M-A, M-B, and M-D are restored.
	// QDACMP
	pdl_push (arr_base);
	pdl_push (arr_hdr);
	pdl_push (arr_ndim);

	// SAVE ARRAY-TYPE OF ORIGINALLY REF'ED ARRAY THIS MUST BE IN 0@PP BELOW
	
	orig_array_type = ldb (ARRAY_TYPE_FIELD, arr_hdr);

	while (1) {
		arr_base = arr_data; // POINTED-TO ARRAY
	
		if (arr_base & Q_FLAG_BIT) {
			// index offset
			arr_ndim = vmread_transport (arr_data + 2);
			if (q_data_type (arr_ndim) != dtp_fix)
				trap ("DATA-TYPE-SCREWUP ARRAY");
			arr_ndim = q_pointer (arr_ndim); // FETCH INDEX OFFSET
			arr_nelts += arr_ndim; // ADJUST INDEX LIMIT
			arr_idx += arr_ndim; // ADJUST CURRENT INDEX
		}
		
		last_nelts = arr_nelts; // SAVE POINTER'S INDEX LENGTH
		gahd1 ();
		
		// NOW TAKE MINIMUM OF THE TWO LENGTHS
		if (ldb (ARRAY_DISPLACED_BIT, arr_hdr)) {
			// DOUBLE DISPLACE, GET CORRECT LENGTH
			arr_nelts = q_pointer (vmread (arr_data + 1));
		}

		// CHECK IF SAME ARRAY-TYPE AS ORIG REF. if not, ORIG MUST CONTROL
		if (ldb (ARRAY_TYPE_FIELD, arr_hdr) != orig_array_type
		    || last_nelts < arr_nelts)
			arr_nelts = last_nelts;

		if (ldb (ARRAY_DISPLACED_BIT, arr_hdr) == 0)
			break;

		// FURTHER INDIR
		A_TEM = vmread_transport (arr_data);
		if (q_data_type (A_TEM) != dtp_array_pointer) {
			arr_data = q_pointer (A_TEM); // JUST DISPLACED
			break;
		}
	}

	// GOT ALL INFO, RESTORE 
	arr_ndim = pdl_pop ();
	arr_hdr = pdl_pop ();
	arr_base = pdl_pop ();
}


// REFERENCE ARRAY
void
QARYR (void) 
{
	Q call_state, delta, dest;

	arr_base = A_A;
	gahd1 ();
	if (arr_ndim != A_R) // A_R is number of args from QMRCL
		trap ("ARRAY-NUMBER-DIMENSIONS M-D M-R M-A");

	if (q_data_type (pdl_peek ()) != dtp_fix)
		trap ("ARGTYP FIXNUM PP NIL NIL AREF");

	arr_idx = q_pointer (pdl_pop ());

	if (dflags & DBG_ARR)
		printf ("arr_idx = %d\n", arr_idx);

	if (arr_ndim != 1) {
		QARY_MULTI ();
		return;
	}

	// QARY-M1 QARYR1

	if (ldb (ARRAY_DISPLACED_BIT, arr_hdr))
		DSP_ARRAY_SETUP ();

	if (arr_idx >= arr_nelts)
		trap ("SUBSCRIPT-OOB M-Q M-S");

	// LAST ELEMENT # REF ED
	A_QLARYL = make_pointer (dtp_fix, arr_idx); 

	// PNTR TO HEADER OF LAST ARRAY REF ED
	A_QLARYH = q_typed_pointer (arr_base); 

	ARRAY_TYPE_REF_DISPATCH (ldb (ARRAY_TYPE_FIELD, arr_hdr));
	
	// QARYR5

	// get data from (old) open call block
	call_state = pdl_ref (A_IPMARK + LP_CALL_STATE); 
	pdl_top = A_IPMARK - LP_CALL_BLOCK_LENGTH;

	delta = ldb (LP_CLS_DELTA_TO_OPEN_BLOCK, call_state);
	A_IPMARK -= delta;

	if (ldb (LP_CLS_ADI_PRESENT, call_state)) {
		// multi value call, store last value in right place, etc
		QRAD1 ();
	}
	// Store into destination in M-C.  Could be D-MICRO.
	// Duplicates QIMOVE-EXIT

	// QMDTBD (note that D_MICRO needs to be added when doing return)
	dest = ldb (LP_CLS_DESTINATION, call_state);
	switch (dest) {
	case D_IGNORE:
		break;
	case D_PDL:
	case D_NEXT:
		pdl_push ((A_T & Q_ALL_BUT_CDR_CODE) | (CDR_NEXT<<30));
		break;

	case D_LAST:
		pdl_push ((A_T & Q_ALL_BUT_CDR_CODE) | (CDR_NIL<<30)); 
		QMRCL (QLLV);
		break;

	case D_RETURN:
		QMDDR (0);
		break;

	case D_NEXT_LIST:
		ILLOP ("D-NEXT-LIST");
		break;

	default:
		ILLOP ("bad dest");
	}
}

void QMDDNL (Q val) { ILLOP ("QMDDNL unimp"); }

void INTP1 (void) { ILLOP ("INTP1 unimp"); }
void QME1 (void) { ILLOP ("QME1 unimp"); }
void QCLS (void) { ILLOP ("QCLS unimp"); }
void CALL_ENTITY (void) { ILLOP ("CALL_ENTITY unimp"); }
void NUMBER_CALLED_AS_FUNCTION (void) { ILLOP ("NUMBER_CALLED_AS_FUNCTION unimp"); }

// DON'T CALL QBND4 TO AVOID REFERENCING A-SELF VIA SLOW VIRTUAL-MEMORY PATH
// Bind SELF to M-A
void
BIND_SELF (void)
{
	if (A_QLBNDP + 23 > A_QLBNDH)
		trap ("PDL-OVERFLOW SPECIAL");

	A_TEM = q_typed_pointer (A_SELF);

	if (ldb (M_FLAGS_QBBFL, A_FLAGS) == 0) {
		// START NEW BINDING BLOCK
		A_FLAGS = dpb (1, M_FLAGS_QBBFL, A_FLAGS);
		A_TEM |= Q_FLAG_BIT; 
	}

	A_QLBNDP++;
	vmwrite (A_QLBNDP, A_TEM); // STORE PREVIOUS CONTENTS
	// NO SEQ BRK HERE

	A_QLBNDP++;
	A_SELF = A_A;

	vmwrite (A_QLBNDP, make_pointer (dtp_locative, &A_SELF - vm));
}

void
CALL_INSTANCE (void)
{
	// Calling an instance as a function.  Bind SELF to it, bind its instance-variables
	// to its value slots, then call its handler function.
	// CALL-INSTANCE
	BIND_SELF ();
	vmread_transport_header (A_A); // Get instance header
	if (q_data_type (md) != dtp_instance_header)
		trap ("DATA-TYPE-SCREWUP DTP-INSTANCE-HEADER");

	A_A = vma; // Possibly-forwarded instance is where inst vars are
	A_C = make_pointer (dtp_locative, md); // Get address of instance-descriptor
	vmread_transport (A_C + INSTANCE_DESCRIPTOR_BINDINGS);
	A_T = q_typed_pointer (md);
	if (A_T == A_V_NIL)
		goto CALL_INSTANCE_3; // () => no bindings

	if (q_data_type (A_T) != dtp_list)
		trap ("DATA-TYPE-SCREWUP %INSTANCE-DESCRIPTOR-BINDINGS");

	A_D = make_pointer (dtp_external_value_cell_pointer, A_A);

	// This loop depends on the fact that the bindings list is cdr-coded,
	// and saves time and register-shuffling by not calling CAR and CDR.
	// However, it does check to make sure that this assumption is true.
	while (1) {
		// CALL_INSTANCE_1

		// Bind them up
		A_B = vmread_transport (A_T); // Get locative to location to bind
		vmread_transport_no_evcp (A_B); // Get current binding
		A_D++; // Points to next value slot
		A_TEM = q_typed_pointer (md);

		// if already there, avoid re-binding
		if (A_D != A_TEM) {
			QBND4 (vma); // Bind it up (sets A_B, A_E)
			md = (A_D & Q_TYPED_POINTER) | (A_E & ~Q_TYPED_POINTER);
			vmwrite (A_B, md);
		}
		
		// CALL_INSTANCE_2
		switch (q_cdr_code (A_B)) {
		case CDR_NORMAL:
		case CDR_ERROR:
			trap ("DATA-TYPE-SCREWUP CDR-CODE-IN-INSTANCE-BINDINGS");
			break;
		case CDR_NIL:
			A_T++;
			goto CALL_INSTANCE_3;
		case CDR_NEXT:
			A_T++;
			break;
		}
	}

CALL_INSTANCE_3:
	A_T = A_V_NIL; // remove possible garbage
	A_A = vmread_transport (A_C + INSTANCE_DESCRIPTOR_FUNCTION);

	D_QMRCL (NULL); // LEAVE, IF ANY, ALREADY DONE
}

void
CALL_SELECT_METHOD (void)
{
	Q idx;

	if (A_R == 0)
		trap ("ZERO-ARGS-TO-SELECT-METHOD M-A");

	A_C = q_typed_pointer (pdl_ref (A_IPMARK + 1)); // FETCH MESSAGE KEY
	A_B = A_V_NIL; // HOLDS CONSTANT ON M-SIDE, FOR EASY COMPARISON

	A_T = make_pointer (dtp_list, A_A);

	if (A_T == make_pointer (dtp_list, 0))
		goto CSM_R; // RESUME
	
	// "SUBROUTINE" CONTINUATION POINT, OR NIL IF AT TOP LEVEL.
	A_METHOD_SUBROUTINE_POINTER = A_V_NIL;

CSM_3:
	pdl_push (A_T);
	QCAR ();
	
	// M-T HAS ASSQ-LIST ELEMENT
	if (q_data_type (A_T) != dtp_list)
		goto CSM_1; // NOT METHOD-KEY, METHOD PAIR

	A_J = A_T;
	QCAR ();

	printf ("CSM check ");
	print_q_verbose (A_T);
	printf ("\n");

	if (A_T == A_C)
		goto CSM_2; // FOUND IT

	if (q_data_type (A_T) == dtp_list)
		goto CSM_7; // ASSQ KEY A LIST, DO MEMQ ON IT

CSM_5:
	A_T = pdl_pop ();
	QCDR ();

	if (q_data_type (A_T) == dtp_list)
		goto CSM_3;

	if (A_B != A_METHOD_SUBROUTINE_POINTER)
		goto CSM_8A; // IF IN SUBROUTINE, RETURN.

	// NON-NIL TERMINATION IS SUPERCLASS POINTER.  USE IT TO
	// REPLACE DTP-SELECT-METHOD AND REINVOKE. THE TWO COMMON
	// CASES ARE (1) THIS SYMBOL IS A SUPERCLASS POINTER AND IT'S
	// FUNCTION CELL CONTAINS A DTP-SELECT-METHOD.  THE SEARCH
	// WILL CONTINUE. (2) THIS SYMBOL IS A LISP FUNCTION AND WILL
	// GET CALLED IN THE USUAL WAY.  THIS SERVES AS AN "OTHERWISE"
	// CLAUSE.
	if (A_T != A_V_NIL)
		goto CSM_6;

	trap ("SELECTED-METHOD-NOT-FOUND M-A M-C"); // SELECTED METHOD NOT FOUND

CSM_R:
	// RESUME SEARCH AT SAVED POINT
	pdl_push (A_METHOD_SEARCH_POINTER);
	goto CSM_5;

CSM_7:
	pdl_push (A_A); // ASSQ KEY A LIST, DO MEMQ ON IT

	A_A = A_C;
	XMEMQ1 (A_A, A_T);

	A_A = pdl_pop (); // RESTORE M-A
	if (A_T == A_V_NIL)
		goto CSM_5;

CSM_2:
	A_METHOD_SEARCH_POINTER = pdl_pop (); // SAVE IN CASE METHOD SEARCH IS RESUMED.

	// FOUND DESIRED METHOD KEY.  GET ASSOC FCTN FROM ASSQ ELEMENT.
	A_T = A_J;
	QCDR ();

CSM_6:
	idx = A_IPMARK;

	// CLOBBER INTO LP-FEF SLOT, REPLACING DTP-SELECT-METHOD
	pdl_store (idx, (A_T & Q_TYPED_POINTER) | (pdl_ref (idx) & ~Q_TYPED_POINTER));
	A_A = A_T;
	D_QMRCL (NULL);
	return;

CSM_1:
	// GET HERE IF SELECT-METHOD LIST-ELEMENT NOT A CONS.
	if (q_data_type (A_T) != dtp_symbol)
		trap ("SELECT-METHOD-GARBAGE-IN-SELECT-METHOD-LIST M-T");

	// DO A ONE LEVEL "SUBROUTINE" CALL.  SAVE CONTINUATION POINTER IN M-B.

	if (A_B != A_METHOD_SUBROUTINE_POINTER)
		goto CSM_8; // ALREADY IN A SUBROUTINE, RETURN

	A_METHOD_SUBROUTINE_POINTER = pdl_pop (); // SAVE CONTINUATION POINT.

	vmread_transport (A_T + 2);
	if (q_data_type (md) == dtp_symbol)
		goto CSM_8A; // NO METHODS IN THIS CLASS, IMMEDIATELY RETURN.

	if (q_data_type (md) != dtp_select_method)
		trap ("SELECT-METHOD-BAD-SUBROUTINE-CALL M-A");

	A_T = make_pointer (dtp_list, md);
	goto CSM_3;

CSM_8:
	// HERE IF IN A SUBROUTINE, BUT DIDNT FIND IT.  RETURN FROM SUBROUTINE AND CONTINUE.
	pdl_pop ();
CSM_8A:
	pdl_push (A_METHOD_SUBROUTINE_POINTER); // PUT CONTINUATION WHERE IT IS EXPECTED.
	A_METHOD_SUBROUTINE_POINTER = A_V_NIL; // AT TOP LEVEL AGAIN.
	goto CSM_5;
}

void QLEAI1 (void) {ILLOP ("QLEAI1 unimp");}
void QRENT (void);

void
TRAP_ON_BAD_SG_STATE (Q current_state)
{
	switch (current_state) {
	case SG_STATE_RESUMABLE:
	case SG_STATE_AWAITING_RETURN:
	case SG_STATE_INVOKE_CALL_ON_RETURN:
	case SG_STATE_AWAITING_CALL:
	case SG_STATE_AWAITING_INITIAL_CALL:
		break;

	case SG_STATE_ACTIVE:
	case SG_STATE_AWAITING_ERROR_RECOVERY:
	case SG_STATE_EXHAUSTED:
		trap ("sg state");
		break;

	default:
		ILLOP ("sg state");
	}
}

// This code is also duplicated at QLENTR and QME1 to save time.
// Make the new frame current, maintain pdl buffer, store arg count into frame.
void
FINISH_ENTERED_FRAME (void)
{
	Q entry_state;

	A_AP = A_IPMARK;
	entry_state = dpb (A_R, LP_ENS_NUM_ARGS_SUPPLIED, FIX_ZERO);
	pdl_store (A_AP + LP_ENTRY_STATE, entry_state);
}

//STACK-GROUP STUFF
// THE STACK GROUP QS MAY BE CONSIDERED TO BE DIVIDED INTO THREE GROUPS:
//   STATIC POINTERS, DYNAMIC STATE, AND LINKAGE QS.
// STATIC POINTERS ARE THINGS LIKE PDL ORIGINS.  THEY ARE LOADED, BUT NEVER STORED
//  BY STACK GROUP HACKING ROUTINES.
// DYNAMIC STATE ARE THINGS WHICH ARE CHANGED DURING THE OPERATION OF THE MACHINE.
//  THEY ARE BOTH LOADED AND STORED.
// LINKAGE QS ARE THINGS LIKE SG-STATE, SG-PREVIOUS-STACK-GROUP, ETC.  THEY ARE NOT
//  LOADED AND UNLOADED FROM A-MEMORY BY THE LOW LEVEL ROUTINES, BUT NOT "UPDATED".
//EACH OF THESE GROUPS IS ALLOCATED A CONTIGIOUS AREA WITHIN THE STACK-GROUP-HEADER.

// WHEN SAVING STATE, THINGS ARE FIRST SAVED IN THE PDL-BUFFER.  THE ENTIRE BLOCK
//IS THEN WRITTEN TO MAIN MEMORY.  WHEN RESTORING, THE ENTIRE BLOCK IS READ INTO
//THE PDL BUFFER, THEN RESTORED TO THE APPROPRIATE PLACES.  SINCE GENERALLY THE MOST
//"VOLATILE" THINGS WANT TO BE SAVED FIRST AND RESTORED LAST, A SORT OF PUSH DOWN LIKE
//OPERATION IS APPROPRIATE.  THUS THE VMA PUSHED ONTO THE PDL-BUFFER FIRST.
//ON THE STORE INTO MAIN MEMORY, IT IS STORED LAST.  THE STORING PROCEEDS IN LEADER
//INDEX ORDER (IE COUNTING DOWN IN MEMORY).  THUS THE VMA WINDS UP IN THE LOWEST
//Q OF THE ARRAY LEADER (JUST BEYOND THE LEADER-HEADER).  ON THE RELOAD,
//THE VMA IS THE FIRST THING READ FROM MEMORY.  IT THUS BECOMES DEEPEST ON THE PDL-BUFFER
//STACK, AND IS THE LAST THING RESTORED TO THE REAL MACHINE.  (ACTUALLY, THE VERY FIRST
//THING SAVED IS THE PDL-BUFFER-PHASING Q, WHICH IS SOMEWHAT SPECIAL SINCE IT IS ACTUALLY
//"USED" WHEN FIRST READ ON THE RELOAD).

//STORE DYNAMIC STATE OF MACHINE IN CURRENT STACK GROUP
//  MUST NOT CLOBBER M-A ON IMMEDIATE RETURN (THAT CAN HAVE SG-GOING-TO)
void
SGLV (void)
{
	//STORE EVERYTHING IN PDL-BUFFER IN REVERSE ORDER ITS TO BE WRITTEN TO MEMORY
	//M-TEM HAS DESIRED NEW STATE FOR CURRENT STACK GROUP.  SWAP L-B-P OF
	//CURRENT STACK GROUP UNLESS 1.7 OF M-TEM IS 1.

	A_LAST_STACK_GROUP = A_QCSTKG;

	A_LAST_STACK_GROUP &= ~Q_FLAG_BIT;
	if (ldb (M_FLAGS_METER_STACK_GROUP_ENABLE, A_FLAGS))
		A_LAST_STACK_GROUP |= Q_FLAG_BIT;

	A_FLAGS = dpb (1, M_FLAGS_STACK_GROUP_SWITCH, A_FLAGS); // SHUT OFF TRAPS, ETC.

	if (q_data_type (A_QCSTKG) != dtp_stack_group)
		ILLOP ("SGLV not stack group");

	A_SG_STATE = dpb (A_TEM, SG_ST_CURRENT_STATE, A_SG_STATE);

	A_4 = pdl_top; // SAVE ORIGINAL PDL LVL

	// SAVE PDL-BUFFER-POINTER FOR PHASING.
	pdl_push (make_pointer (dtp_fix, A_4));

	pdl_push (make_pointer (dtp_locative, vma)); // SAVE VMA AS A LOCATIVE

	// SAVE TAGS OF VMA, M-1, AND M-2 AS FIXNUM
	// XXX pointer size
	A_3 = FIX_ZERO;
	A_3 |= (vma >> 24) & 0xff;
	A_3 |= (A_1 >> 16) & 0xff00;
	A_3 |= (A_2 >> 8) & 0xff0000;
	pdl_push (A_3);

	// SAVE POINTERS OF M-1, M-2 AS FIXNUMS
	pdl_push (make_pointer (dtp_fix, A_1));
	pdl_push (make_pointer (dtp_fix, A_2));

	pdl_push (A_ZR);
	pdl_push (A_A);
	pdl_push (A_B);
	pdl_push (A_C);
	pdl_push (A_D);
	pdl_push (A_E);
	pdl_push (A_T);
	pdl_push (A_R);
	pdl_push (A_Q);
	pdl_push (A_I);
	pdl_push (A_J);
	pdl_push (A_S);
	pdl_push (A_K);
	
	pdl_push (make_pointer (dtp_fix, A_FLAGS));

	if (ldb (SG_ST_FOOTHOLD_EXECUTING_FLAG, A_TEM))
		goto SGLV2;

	A_J = A_QLBNDP;
SGLV3:
	while (A_J > A_QLBNDO) {
		if (q_data_type (vmread (A_J)) != dtp_locative) {
			// Space past, down to Q with flag bit
			while (1) {
				vmread (A_J);
				A_J--;
				if (q_flag_bit (md))
					goto SGLV3;

				if (A_J <= A_QLBNDO)
					goto SGLV4;
			}
		}

		// In this direction, no need to check flag bits
		// Since things must remain paired as long as in bindings
		A_C = vmread_transport_no_evcp (A_J); // Pointer to cell bound
		A_J--;
		A_D = vmread_transport_no_evcp (A_J); // Old binding to be restored
		
		A_ZR = vmread_transport_no_evcp (A_C); // New binding to be saved

		vmwrite_gc_write_test  (A_C,
					(A_D & Q_TYPED_POINTER)
					| (A_ZR & ~Q_TYPED_POINTER));
		
		vmwrite_gc_write_test (A_J,
				       (A_ZR & Q_TYPED_POINTER)
				       | (A_D & ~Q_TYPED_POINTER));
		
		A_J--;
	}

SGLV4:
	A_SG_STATE = dpb (1, SG_ST_IN_SWAPPED_STATE, A_SG_STATE);

SGLV2:
	pdl_push (A_QLARYL);
	pdl_push (A_QLARYH);
	pdl_push (A_TRAP_MICRO_PC);


	QLLV (); // actually, should be MLLV 

	pdl_push (make_pointer (dtp_fix, A_IPMARK));
	pdl_push (make_pointer (dtp_fix, A_AP));

	pdl_push (make_pointer (dtp_fix, A_QLBNDP - A_QLBNDO)); // SAVE L-B-P LEVEL

	pdl_push (make_pointer (dtp_fix, A_4)); // SAVE PDL LEVEL

	pdl_push (A_TRAP_AP_LEVEL);
	// pdl_push (A_SG_FOLLOWING_STACK_GROUP);
	pdl_push (A_SG_CALLING_ARGS_NUMBER);
	pdl_push (A_SG_CALLING_ARGS_POINTER);
	pdl_push (A_SG_PREVIOUS_STACK_GROUP);
	pdl_push (A_SG_STATE);

	vma = q_pointer (A_QCSTKG) - (2 + SG_STATE); // 2 FOR LEADER HEADER

	Q count = (SG_PDL_PHASE - SG_STATE) + 1;
	Q i;

	for (i = 0; i < count; i++) {
		Q x;
		x = pdl_pop ();
		vmwrite_gc_write_test (vma, x);
		printf ("sglv %d %#o: ", i, q_pointer (vma));
		print_q (x);
		printf ("\n");
		vma--;
	}
		
	A_FLAGS = dpb (0, M_FLAGS_STACK_GROUP_SWITCH, A_FLAGS);
}

void
SG_CALL (void) 
{
	Q called_sg_state, called_current_state;

	// This routine handles a stack group's being called as a function;
	// it is reached from the D-QMRCL dispatch.  Thus, M-A contains the new stack group.
	// First, error checking: if both SG's are SAFE, then the called one has to be
	// in the AWAITING-CALL or AWAITING-INITIAL-CALL state.
	// SG-CALL
	FINISH_ENTERED_FRAME ();
	A_2 = A_A;

	// GET STATE OF SG GOING TO
	// GET-SG-STATE
	called_sg_state = vmread (A_2 - (2 + SG_STATE));
	called_current_state = ldb (SG_ST_CURRENT_STATE, called_sg_state);
	TRAP_ON_BAD_SG_STATE (called_current_state);

	if (ldb (SG_ST_SAFE, called_sg_state) != 0
	    && ldb (SG_ST_SAFE, A_SG_STATE) != 0
	    && called_current_state != SG_STATE_AWAITING_CALL
	    && called_current_state != SG_STATE_AWAITING_INITIAL_CALL) {
		trap ("sg state");
	}

	// SG-CALL-1
	A_B = called_sg_state; // Save SG-STATE of SG going to

	// Set up the argument list.  This doesn't handle LEXPR/FEXPR calls!
	if (A_R == 0) { /* num args */
		A_SG_TEM = A_V_NIL;
		A_SG_TEM1 = A_V_NIL;
	} else {
		A_SG_TEM1 = convert_pdl_buffer_address (A_AP + 1); // List pointer to arg list.
		A_SG_TEM = q_typed_pointer (vmread (A_SG_TEM1));
	}

	// SG-CALL-2
	// Leave old SG in awaiting-return, and don't swap if both of these bits are off.
	A_2 = SG_STATE_AWAITING_RETURN;
	if ((ldb (SG_ST_SWAP_SV_ON_CALL_OUT, A_SG_STATE)) == 0
	    && (ldb (SG_ST_SWAP_SV_OF_SG_THAT_CALLS_ME, A_B)) == 0) {
		A_2 = dpb (1, SG_ST_FOOTHOLD_EXECUTING_FLAG, A_2); // Set 100 bit; don't swap L-B-P
	}

	// SG-CALL-3

	// VMA NOT IMPORTANT IN THIS PATH, FLUSH ANY GARBAGE.  CRUFT
	// POSSIBLE VIA PATH FROM XUWR2, AT LEAST.
	vma = FIX_ZERO; 

	A_TEM = A_2; // M-TEM has the new state, plus 100 bit says to not swap L-B-P.
	SGLV ();
	SG_ENTER ();
}

void SG_ENTER_1 (void);

void
SG_ENTER (void)
{
	// This is the common routine for activating a new stack group.  It takes the following
	// things:  the new stack group itself in M-A, the transmitted value in A-SG-TEM,
	// the argument list in A-SG-TEM1, and the argument count in M-R.

	A_SG_TEM2 = A_QCSTKG;
	A_QCSTKG = A_A;
	SGENT ();
	A_SG_PREVIOUS_STACK_GROUP = A_SG_TEM2;
	SG_ENTER_1 ();
}

void
SG_ENTER_1 (void)
{
	// SG-ENTER-1

	A_SG_CALLING_ARGS_POINTER = A_SG_TEM1;
	A_SG_CALLING_ARGS_NUMBER = make_pointer (dtp_fix, A_R);

	// Now dispatch to separate routines, based on what state the new SG is in.
	// SGENT left that state in M-TEM.  It only dispatches on the low four bits
	// of the state because there are only 10. states implemented, and although
	// the state is a 6 bit field, it would waste lot of D-MEM to make the table
	// that large.
	// D-SG-ENTER
	switch (get_field (A_TEM, 4, 0)) {
	case SG_STATE_RESUMABLE:
		break;

	case SG_STATE_AWAITING_RETURN:
		A_T = A_SG_TEM;
		QMDDR (1);
		break;

	case SG_STATE_AWAITING_CALL:
		A_T = A_SG_TEM;
		break;

	case SG_STATE_INVOKE_CALL_ON_RETURN:
	case SG_STATE_AWAITING_INITIAL_CALL:
		// SG-ENTER-CALL
		QMRCL (NULL);
		break;

	default:
		ILLOP ("sg enter");
	}
}

void QLLENT (void) {ILLOP ("QLLENT");}

void
SB_REINSTATE (void) 
{
	// SB-REINSTATE SB deferred.  Take it now?
	if (q_typed_pointer (A_INHIBIT_SCHEDULING_FLAG) != A_V_NIL)
		return;

	A_FLAGS = dpb (0, M_FLAGS_DEFERRED_SEQUENCE_BREAK, A_FLAGS);
	ILLOP ("do SB");
}


// CHANGE STACK-GROUP STATE TO ACTIVE.  RETURN IN M-TEM PREVIOUS STATE.  IF L-B-P WAS
// SWAPPED, SWAP IT BACK.
void
SGENT (void)
{
	Q i;

	A_FLAGS = dpb (1, M_FLAGS_STACK_GROUP_SWITCH, A_FLAGS);
	sg_load_static_state ();

	// NO TRANSPORT SINCE IT'S A FIXNUM
	Q restore_ptr = A_QCSTKG - (2 + SG_PDL_PHASE);

	pdl_top = q_pointer (vmread (restore_ptr));
	restore_ptr++;

	Q count = (SG_PDL_PHASE - SG_STATE);
	for (i = 0; i < count; i++) {
		Q x;
		x = vmread_transport_ac (restore_ptr);
		printf ("sg restore %d %#o: ", i, q_pointer (restore_ptr));
		print_q (x);
		printf ("\n");
		pdl_push (x);
		restore_ptr++;
	}

	A_SG_STATE = make_pointer (dtp_fix, pdl_pop ());
	A_SG_PREVIOUS_STACK_GROUP = pdl_pop ();
	A_SG_CALLING_ARGS_POINTER = pdl_pop ();
	A_SG_CALLING_ARGS_NUMBER = pdl_pop ();
	// A_SG_FOLLOWING_STACK_GROUP = pdl_pop ();
	A_TRAP_AP_LEVEL = pdl_pop ();

	// GET PDL-BUFFER RELOAD POINTER BACK INTO PHASE
	pdl_pop (); // discard extra copy of pdl_top
	
	A_QLBNDP = q_pointer (A_QLBNDO + pdl_pop ());

	A_AP = q_pointer (pdl_pop ());
	A_IPMARK = q_pointer (pdl_pop ());

	// XXX here would reverse MLLV

	if (q_data_type (pdl_ref (A_AP)) == dtp_fef_pointer)
		QLLENT (); // IF RUNNING MACRO-CODE, RESTORE MACRO PC

	// NOW SET UP THE CORRECT BASE OF THE MICRO-STACK
	// XXX restore SG-ST-INST-DISP SG_ST_INST_DISP 

	// RESTORE THE REST OF THE SG'S MICRO-STACK
	if (ldb (LP_EXS_MICRO_STACK_SAVED, pdl_ref (A_AP + LP_EXIT_STATE)))
		QMMPOP ();

	A_TRAP_MICRO_PC = pdl_pop ();
	A_QLARYH = pdl_pop ();
	A_QLARYL = pdl_pop ();

	A_FLAGS = pdl_pop ();

	if (ldb (SG_ST_IN_SWAPPED_STATE, A_SG_STATE) != 0) {
		A_A = A_QLBNDO; // POINTS TO FIRST WD OF FIRST BLOCK.
		
	SGENT3:
		while (1) {
			// IS 2ND WD OF BLOCK PNTR TO VALUE CELL?
			A_A++;
			if (A_A > A_QLBNDP)
				break;
			vmread (A_A);
			if (md & Q_FLAG_BIT)
				goto SGENT3;
			
			if (q_data_type (md) != dtp_locative) {
				// SGENT6
				while (1) {
					A_A++;
					if (vmread (A_A) & Q_FLAG_BIT)
						goto SGENT3;
					
					if (A_A >= A_QLBNDP)
						goto SGENT4;
				}
			}
			
			
			// SGENT5
			vmread_transport_no_evcp (vma);
			A_C = md; // M-C HAS POINTER TO INTERNAL V.C.
			A_D = vmread_transport_no_evcp (vma - 1);
			
			A_ZR = vmread_transport_no_evcp (A_C);
			vmwrite_gc_write_test (vma,
					       (A_D & Q_TYPED_POINTER)
					       | (A_ZR & ~Q_TYPED_POINTER));

			vmwrite_gc_write_test (A_A - 1,
					       (A_ZR & Q_TYPED_POINTER)
					       | (A_D & ~Q_TYPED_POINTER));
			
			A_A++;
		}
		
	SGENT4:
		A_SG_STATE = dpb (0, SG_ST_IN_SWAPPED_STATE, A_SG_STATE);
	}

	// SGENT2

	A_K = pdl_pop ();
	A_S = pdl_pop ();
	A_J = pdl_pop ();
	A_I = pdl_pop ();
	A_Q = pdl_pop ();
	A_R = pdl_pop ();
	A_T = pdl_pop ();
	A_E = pdl_pop ();
	A_D = pdl_pop ();
	A_C = pdl_pop ();
	A_B = pdl_pop ();
	A_A = pdl_pop ();
	A_ZR = pdl_pop ();

	if (ldb (M_FLAGS_METER_STACK_GROUP_ENABLE, A_FLAGS)) {
		ILLOP ("METER-SG-ENTER");
	}

	A_2 = q_pointer (pdl_pop ()); // RESTORE POINTER FIELDS OF M-1,M-2
	A_1 = q_pointer (pdl_pop ());
	Q flags = pdl_pop ();
	Q restored_vma = q_pointer (pdl_pop ());

	Q tag = flags & 0xff;
	restored_vma |= (tag << 24);

	tag = (flags >> 8) & 0xff;
	A_1 |= (tag << 24);

	tag = (flags >> 16) & 0xff;
	A_2 |= (tag << 24);
	

	A_4 = A_QCSTKG;
	vma = A_4 - (2 + SG_STATE);
	A_4 = SG_STATE_ACTIVE;
	A_4 = dpb (A_4, SG_ST_CURRENT_STATE, A_SG_STATE);
	vmwrite (vma, A_4);

	vmread (restored_vma); // RESTORE VMA AND MD

	A_FLAGS = dpb (0, M_FLAGS_STACK_GROUP_SWITCH, A_FLAGS);

	if (ldb (M_FLAGS_DEFERRED_SEQUENCE_BREAK, A_FLAGS))
		SB_REINSTATE ();

	// XXX knows that current state is right aligned
	A_TEM = ldb (SG_ST_CURRENT_STATE, A_SG_STATE);
	A_SG_STATE = make_pointer (dtp_fix, A_4);
}

int new_pc;

void QBSPCL (int idx) {ILLOP ("QBD0 unimp");}

// FRAME BIND. BIND S-V S FROM FRAME FAST ENTERED USING S.V. MAP
void
FRMBN1 (void)
{
	int idx;

	/* original code used M_D to shadow the pdl-buffer-index */

	vmread (A_A + FEFHI_SV_BITMAP);
	idx = A_AP;
	A_T = A_A + FEFHI_SPECIAL_VALUE_CELL_PNTRS;

	if (ldb (FEFHI_SVM_ACTIVE, md) == 0) {
		// FOO FAST OPT SHOULD NOT BE ON UNLESS SVM IS. (IT
		// ISNT WORTH IT TO HAVE ALL THE HAIRY MICROCODE TO
		// SPEED THIS CASE UP A TAD.)
		ILLOP ("FRMBN1"); 
	}

	A_C = ldb (FEFHI_SVM_BITS, md);

	// FRMBN2
	while (A_C != 0) {
		idx++;
		if (ldb (FEFHI_SVM_HIGH_BIT, A_C)) {
			QBSPCL (idx);
			A_C = dpb (0, FEFHI_SVM_HIGH_BIT, A_C);
		}
		A_C <<= 1;
	}
}

/*
;;; "LINEAR" ENTER
;   M-A HAS PNTR TO FEF TO CALL
;   M-S HAS EVENTUAL NEW ARG POINTER (M-AP)
;   M-R HAS NUMBER OF ARGUMENTS
;WE DON'T SUPPORT USER COPYING AND FORWARDING OF FEFS,
;SO IT'S NOT NECESSARY TO CALL THE TRANSPORTER EVERYWHERE.
;CAN SEQUENCE BREAK ONCE WE GET PAST THE ARGUMENTS AND START DOING VARIABLE
;INITIALIZATIONS, WHICH CAN CAUSE ERRORS.  THIS WILL INVALIDATE A-LCTYP BUT
;PRESERVE THE LETTERED M-REGISTERS.
;*** WE STILL HAVE A PROBLEM WITH M-ERROR-SUBSTATUS NOT BEING PRESERVED
*/
void
QLENTR (void)
{
	Q hdr; /* A-D A_D */
	Q numarg;
	Q max_args;
	unsigned int i;

	/* FUTURE: if metering, call meter-function-entry */

	A_AP = A_IPMARK;
	A_ERROR_SUBSTATUS = 0;
	A_LCTYP = 0;		// CLEAR OUT LINEAR-CALL-TYPE 

	hdr = vmread (A_A);
	if (ldb (STRUCTURE_TYPE_FIELD, hdr) != HEADER_TYPE_FEF)
		ILLOP ("not fef header type");

	/* M-J */
	new_pc = ldb (FEFH_PC, hdr); /* may change due to optional args */

	if (ldb (LP_CLS_ADI_PRESENT, pdl_ref (A_AP + LP_CALL_STATE))) {
		QLEAI1 ();
		return;
	}

	/* QLEAI2: */

	if (ldb (FEFH_FAST_ARG, hdr) == 0) {
		QRENT ();
		return;
	}

	numarg = vmread (A_A + FEFHI_FAST_ARG_OPT);
	if (q_data_type (numarg)  != dtp_fix)
		trap ("DATA-TYPE-SCREWUP FEF");
		
	if (A_R < ldb (ARG_DESC_MIN_ARGS, numarg))
		A_ERROR_SUBSTATUS = dpb (1, M_ESUBS_TOO_FEW_ARGS,
					 A_ERROR_SUBSTATUS);

	if (ldb (ARG_DESC_ANY_REST, numarg)) {
		Q n;
		Q local_block_origin;

		// Here if the function takes a rest arg.  
		// "numarg" (M-E) has # reg+opt args.
		// ADL not being used, fast-arg option is active.
		// QLFOA1

		// check for LEXPR/FEXPR
		if (A_LCTYP != 0) 
			ILLOP ("goto QLFRA1");

		if (A_R <= ldb (ARG_DESC_MAX_ARGS, numarg)) {
			// Called with just spread arguments.
			// If the rest arg will be NIL, push NILs for it and any 
			// missing optionals.
			
			n = (ldb (ARG_DESC_MAX_ARGS, numarg) - A_R) + 1;
			for (i = 0; i < n; i++)
				pdl_push (A_V_NIL);
			
			local_block_origin = LP_INITIAL_LOCAL_BLOCK_OFFSET
				+ ldb (ARG_DESC_MAX_ARGS, numarg);
		} else {
			// Called with enough spread args to get into the rest arg
			// QLFSA2

			// First of rest
			n = convert_pdl_buffer_address (A_AP
							+ ldb (ARG_DESC_MAX_ARGS, numarg)
							+ LP_INITIAL_LOCAL_BLOCK_OFFSET);
			pdl_push (make_pointer (dtp_list, n)); // Push the rest-arg

			// Put the local block after the supplied args
			local_block_origin = LP_INITIAL_LOCAL_BLOCK_OFFSET
				+ A_R;
		}

		// QLFOA5
		// Args set up.  Set up entry-state and local-block
		A_LOCALP = A_AP + local_block_origin;

		n = FIX_ZERO;
		n = dpb (A_R, LP_ENS_NUM_ARGS_SUPPLIED, n);
		n = dpb (local_block_origin, LP_ENS_MACRO_LOCAL_BLOCK_ORIGIN, n);
		pdl_store (A_AP + LP_ENTRY_STATE, n);
		
		A_T = ldb (FEFHI_MS_LOCAL_BLOCK_LENGTH,
			   vmread (A_A + FEFHI_MISC));
		A_T--; // First local (rest arg) already pushed
		goto QFL1C;

	}

	max_args = ldb (ARG_DESC_MAX_ARGS, numarg);
	if (A_R > max_args)
		A_ERROR_SUBSTATUS = dpb (1, M_ESUBS_TOO_MANY_ARGS,
					 A_ERROR_SUBSTATUS);

	A_LOCALP = A_AP + LP_INITIAL_LOCAL_BLOCK_OFFSET + max_args;

	A_C = FIX_ZERO;
	A_C = dpb (LP_INITIAL_LOCAL_BLOCK_OFFSET + max_args,
		   LP_ENS_MACRO_LOCAL_BLOCK_ORIGIN,
		   A_C);
	A_C = dpb (A_R,
		   LP_ENS_NUM_ARGS_SUPPLIED,
		   A_C);

	/* QLEAI5 */

	pdl_store (A_AP + LP_ENTRY_STATE, A_C);

	/* default extra args to NIL */
	for (i = A_R; i < max_args; i++)
		pdl_push (SYM_NIL);

	// QFL1
	// HAVE SET UP ARGS
	A_T = ldb (FEFHI_MS_LOCAL_BLOCK_LENGTH, vmread (A_A + FEFHI_MISC));

QFL1C:

	// INIT LOCAL BLOCK TO NIL
	for (i = 0; i < A_T; i++)
		pdl_push (SYM_NIL);

	if (ldb (FEFH_SV_BIND, hdr)) {
		// MOVE S-V BINDINGS TO S-V-CELLS AND PUSH PREVIOUS
		// BINDINGS ON BINDING PDL
		FRMBN1 ();
	}

	// FINISH LINEARLY ENTERING
	// QLENX
	LC_fef = A_A;
	LC_pc = new_pc;

	printf ("#%d: Enter function: ", macro_count);
	print_q_verbose (LC_fef);
	printf ("\n");

	A_IPMARK = A_AP; // redundant with first of QLENTR? 
	if (A_ERROR_SUBSTATUS != 0) {
		pdl_push (make_pointer (dtp_fix, A_ERROR_SUBSTATUS));
		printf ("trap function entry: error_substatus %#o %s\n",
			q_pointer (A_ERROR_SUBSTATUS),
			error_substatus_name (q_pointer (A_ERROR_SUBSTATUS)));
		trap ("FUNCTION-ENTRY");
	}
}



/*
;LINEAR ENTER WITHOUT FAST OPTION
; M-A FEF			
; M-B flags/temp		
; M-C flags/temp		
; M-D pdl index of arg (argidx)	
; M-E count of bind descs	bind_desc_to_go
; M-T address of sv slot	sv_slot

M-R number of args called with	
M-Q bind desc Q "arg_desc"	
M-I address of bind desc	arg_desc_ptr
M-J start PC of FEF	
M-S pdl index of frame = M-AP	
M-K temp                        

*/

void QBD0 (void) {ILLOP ("QBD0 unimp");}
void QBDL1 (void) {ILLOP ("QBD0 unimp");}

void QBLSPCL (void) {ILLOP ("QBLSPCL unimp"); }

void
QRENT (void)
{
	int args_left;
	Q arg_desc;
	int argidx;
	Q arg;
	Q tmp;
	Q bind_desc_to_go;
	Q arg_desc_ptr;
	Q sv_slot;

	argidx = A_AP + LP_INITIAL_LOCAL_BLOCK_OFFSET; // -> FIRST ARG
	sv_slot = A_A + FEFHI_SPECIAL_VALUE_CELL_PNTRS; // -> S-V SLOTS

	args_left = A_R; // # ARGS YET TO DO

	Q misc = vmread (A_A + FEFHI_MISC);
	arg_desc_ptr = A_A + ldb (FEFHI_MS_ARG_DESC_ORG, misc); // -> FIRST BIND DESC
	bind_desc_to_go = ldb (FEFHI_MS_BIND_DESC_LENGTH, misc);
	A_LOCALP = -1; // SIGNAL LOCAL BLOCK NOT YET LOCATED

	if (A_LCTYP != 0) {
		// WAS FEXPR OR LEXPR CALL
		// FLUSH NO-SPREAD-ARG AND PROCESS ANY SPREAD ARGS
		args_left--;
	}

	/* QBINDL */
	// BIND LOOP USED WHILE ARGS REMAIN TO BE PROCESSED
	while (args_left > 0) {
		if (bind_desc_to_go == 0) {
			// OUT OF BIND DESC, TOO MANY ARGS
			// QBTMA1
			A_ERROR_SUBSTATUS = dpb (1, M_ESUBS_TOO_MANY_ARGS,
						 A_ERROR_SUBSTATUS);
			argidx += args_left; // ADVANCE LCL PNTR PAST THE EXTRA ARGS
			goto done;
		}

		bind_desc_to_go--;

		arg = pdl_ref (argidx);

		arg_desc = vmread (arg_desc_ptr); // ACCESS WORD OF BINDING OPTIONS
		if (ldb (FEF_NAME_PRESENT, arg_desc))
			arg_desc_ptr++;

		// QREDT1
		switch (ldb (FEF_ARG_SYNTAX, arg_desc)) {
		case FEF_ARG_OPT:
			/* QBROP1 */
			// OPTIONAL ARG IS PRESENT, 
			// SPACE PAST INITIALIZATION INFO IF ANY
			// ucadr dispatch table is only 3 bits?
			/* dispatch QBOPNP */
			switch (ldb (FEF_INIT_OPTION, arg_desc) & 7) {
			case FEF_INI_NONE:
			case FEF_INI_NIL:
			case FEF_INI_COMP_C:
			case FEF_INI_SELF:
				break;

			case FEF_INI_PNTR:
			case FEF_INI_C_PNTR:
			case FEF_INI_EFF_ADR:
				// QBOSP 
				arg_desc_ptr++;
				break;

			case FEF_INI_OPT_SA:
				// QBOASA
				// START LATER TO AVOID CLOBBERING
				arg_desc_ptr++;
				new_pc = q_pointer (vmread (arg_desc_ptr));
				break;
			}

			/* fall in (all jump to QBRQA) */

		case FEF_ARG_REQ:
			// QBRQA
			if (ldb (FEF_SPECIAL_BIT, arg_desc))
				QBSPCL (0); /* XXX pdl-buffer-idx */

			switch (ldb (FEF_DES_DT, arg_desc)) {
			case FEF_DT_DONTCARE: /* no data type chekcing */
				break;

			case FEF_DT_NUMBER:
				switch (q_data_type (arg)) {
				case dtp_fix:
				case dtp_extended_number:
					break;
				default:
					A_ERROR_SUBSTATUS
						= dpb (1,
						       M_ESUBS_BAD_DT,
						       A_ERROR_SUBSTATUS);
					break;
				}
				break;

			case FEF_DT_FIXNUM:
				if (q_data_type (arg) != dtp_fix)
					A_ERROR_SUBSTATUS
						= dpb (1,
						       M_ESUBS_BAD_DT,
						       A_ERROR_SUBSTATUS);
				break;

			case FEF_DT_SYM:
				if(q_data_type (arg) != dtp_symbol)
					A_ERROR_SUBSTATUS 
						= dpb (1,
						       M_ESUBS_BAD_DT,
						       A_ERROR_SUBSTATUS);
				break;

			case FEF_DT_ATOM:
				switch (q_data_type (arg)) {
				case dtp_symbol:
				case dtp_fix:
				case dtp_extended_number:
					break;
				default:
					A_ERROR_SUBSTATUS
						= dpb (1,
						       M_ESUBS_BAD_DT,
						       A_ERROR_SUBSTATUS);
					break;
				}
				break;

			case FEF_DT_LIST:
				if (q_typed_pointer (arg) != SYM_NIL
				    && q_data_type (arg) != dtp_list)
					A_ERROR_SUBSTATUS
						= dpb (1,
						       M_ESUBS_BAD_DT,
						       A_ERROR_SUBSTATUS);
				break;

			case FEF_DT_FRAME:
				if (q_data_type (arg) != dtp_fef_pointer)
					A_ERROR_SUBSTATUS 
						= dpb (1,
						       M_ESUBS_BAD_DT,
						       A_ERROR_SUBSTATUS);
				break;
			}
			break;

		case FEF_ARG_AUX:
		case FEF_ARG_FREE:
		case FEF_ARG_INTERNAL:
		case FEF_ARG_INTERNAL_AUX:
			// TOO MANY ARGS
			// QBTMA2
			A_ERROR_SUBSTATUS 
				= dpb (1, M_ESUBS_TOO_MANY_ARGS,
				       A_ERROR_SUBSTATUS);
			argidx += args_left; // ADVANCING LCL PNTR PAST THE EXTRA ARGS
			goto QBDT2; // FINISH BIND DESCS

		case FEF_ARG_REST: 
			// QBRA

			// REST ARG - FOR NOW I ASSUME MICRO-COMPILED
			// FUNCTIONS DO STORE CDR CODES

			// IF A NON-SPREAD ARG, SHOULD NOT
			// GET TO REST ARG HERE.
			if (A_LCTYP != 0)
				ILLOP ("QBRA");

			// LOCATE LOCAL BLOCK AFTER LAST ARG
			argidx += args_left;
			if (A_LOCALP == -1)
				set_local_block (argidx);

			// STORE REST ARG AS FIRST LOCAL
			tmp = convert_pdl_buffer_address (argidx);
			pdl_push (make_pointer (dtp_list, tmp));

			if (ldb (FEF_SPECIAL_BIT, arg_desc))
				QBLSPCL ();
			arg_desc_ptr++; // ADVANCE TO NEXT BIND DESC
			goto QBD0;

		default: ILLOP ("arg syntax");
		}


		// QBDDT1 QBEQC1
		argidx++; // NEXT ARG SLOT
		arg_desc_ptr++; // NEXT BIND DESC ENTRY
		args_left--;
	}

	/* ================================================================ */
QBD0:
	if (A_LCTYP != 0) {
		// QREW1
		if (bind_desc_to_go == 0)
			ILLOP ("QBD0");
		bind_desc_to_go--;

		arg_desc = vmread (arg_desc_ptr); // ACCESS WORD OF BINDING OPTIONS
		if (ldb (FEF_NAME_PRESENT, arg_desc))
			arg_desc_ptr++;

		if (ldb (FEF_ARG_SYNTAX, arg_desc) != FEF_ARG_REST)
			ILLOP ("not rest arg");
		
		if (A_LOCALP == -1)
			set_local_block (argidx); // SET UP LOCAL BLOCK OVER ARG
		
		pdl_top = argidx; // SO DONT STORE LOCALS OVER ARG
		
		if (ldb (FEF_SPECIAL_BIT, arg_desc))
			QBLSPCL ();
		
		arg_desc_ptr++;
	}

	// QBD1   BINDING LOOP FOR WHEN ALL ARGS HAVE BEEN USED UP
	while (bind_desc_to_go > 0) {
		bind_desc_to_go--;

		arg_desc = vmread (arg_desc_ptr); // GET NEXT BINDING DESC Q
		if (ldb (FEF_NAME_PRESENT, arg_desc))
			arg_desc_ptr++;
	
		/* dispatch QBDT2 */
	QBDT2:
		switch (ldb (FEF_ARG_SYNTAX, arg_desc)) { /* 3 bits */
		case FEF_ARG_REQ:
			// GOT ARG DESCRIPTOR WHEN OUT OF ARGS
			// QBTFA1
			A_ERROR_SUBSTATUS = dpb (1, M_ESUBS_TOO_FEW_ARGS,
						 A_ERROR_SUBSTATUS);
			pdl_push (SYM_NIL);
			break;
		
		case FEF_ARG_AUX:
			// QBDAUX
			if (A_LOCALP == -1)
				set_local_block (argidx);
			goto QBOPT4;
			

		case FEF_ARG_OPT:
			// QBOPT1
			if (A_LOCALP != -1)
				ILLOP ("QBOPT1"); // SHOULDN'T HAVE ARGS AFTER LOCAL BLOCK IS LOCATED

		QBOPT4:
			// dispatch QBOPTT
			switch (ldb (FEF_INIT_OPTION, arg_desc) & 7) {
			case FEF_INI_OPT_SA:
				// QBOPT5
				// OPTIONAL ARGUMENT INIT VIA ALTERNATE STARTING ADDRESS AND NOT PRESENT
				// LEAVE STARTING ADDRESS ALONE AND INIT TO SELF, COMPILED CODE WILL
				// RE-INIT.  BUT DON'T FORGET TO SKIP OVER THE START ADDRESS.
				arg_desc_ptr++;

				/* fall in */

			case FEF_INI_NONE:
			case FEF_INI_COMP_C:
			case FEF_INI_SELF:     
				// QBOPT3
				// OPTIONAL OR AUX, INIT TO SELF OR
				// NONE, LATER MAY BE REINITED BY
				// COMPILED CODE
			
				if (ldb (FEF_SPECIAL_BIT, arg_desc)) {
					// SPECIAL, GET POINTER TO VALUE CELL
					
					// FETCH EXTERNAL VALUE CELL.
					// MUST GET CURRENT VALUE, BUT NOT BARF
					// IF DTP-NULL.  MUST NOT LEAVE AN EVCP
					// SINCE THAT WOULD SCREW PREVIOUS
					// BINDING IF IT WAS SETQ'ED.
				
					pdl_push (vmread_transport_no_trap (sv_slot));
			
					// THIS IS LIKE QBD1A (just breaking from the switch statement), 
					// EXCEPT THAT THE THING WE ARE BINDING IT TO
					// MAY BE DTP-NULL, WHICH IS ILLEGAL TO LEAVE ON THE PDL BUFFER.
					// ALSO, THE VARIABLE IS KNOWN NOT TO BE AN ARGUMENT THAT WAS SUPPLIED,
					// SO THERE'S NO DANGER OF CLOBBERING USEFUL DEBUGGING INFORMATION
					argidx++;
					QBLSPCL ();
					pdl_store (pdl_top, SYM_NIL); // STORE NIL OVER POSSIBLE GARBAGE
					arg_desc_ptr++;
					goto next;
				}

				pdl_push (SYM_NIL); // LOCAL, INIT TO NIL
				break;
			
			case FEF_INI_NIL:
				pdl_push (SYM_NIL);
				break;
				
			case FEF_INI_PNTR:
				//INIT TO POINTER
				// QBOPNR
				arg_desc_ptr++;
				md = vmread_transport (arg_desc_ptr); // FETCH THING TO INIT TOO, TRANSPORT IT
				pdl_push (md);
				break;
				
			case FEF_INI_C_PNTR:
				// INIT TO C(POINTER)
				// QBOCPT
				arg_desc_ptr++;
				md = vmread_transport (arg_desc_ptr);
				md = vmread_transport (md);
				pdl_push (md);
				break;
				
			case FEF_INI_EFF_ADR:
				// INIT TO CONTENTS OF "EFFECTIVE ADDRESS"
				// QBOEFF
				arg_desc_ptr++;
				vmread (arg_desc_ptr);
				A_1 = get_field (md, 6, 0); // PICK UP DELTA FIELD
				
				/* dispatch QBOFDT */
				// DISPATCH ON REG
				switch (get_field (md, 3, 6)) {
				case 0: 
				case 1: 
				case 2: 
				case 3: 
					// QBFE
					// FETCH FROM FEF OF FCN ENTERING
					A_1 = get_field (md, 8, 0); // FULL DELTA
					md = vmread_transport (A_A + A_1); 
					pdl_push (md);
					break;
					
				case 4: 
					// QBQT
					// FETCH FROM CONSTANTS PAGE
					md = vmread_transport (A_V_CONSTANTS_AREA + A_1); 
					pdl_push (md);
					break;
					
				case 5: 
					// QBDLOC
					// CHECK FOR TRYING TO ADDRESS LOCALS BEFORE LOCATED
					if (A_LOCALP == -1)
						ILLOP ("QBDLOC");
					pdl_push (pdl_ref (A_LOCALP + A_1));
					break;
					
				case 6: 
					// QBDARG
					// FETCH ARG (%LP-INITIAL-LOCAL-BLOCK-OFFSET = 1)
					pdl_push (pdl_ref (A_AP + A_1 + 1));
					break;
					
				case 7: // PDL ILLEGAL
					ILLOP ("QBOEFF");
				}
			}
			break;
			
		case FEF_ARG_REST:
			// QBRA1
			// REST ARG MISSING, MAKE 1ST LOCAL NIL
			if (A_LOCALP == -1)
				set_local_block (argidx);
			
			pdl_push (SYM_NIL);
			break;
			
		case FEF_ARG_FREE:
			// QBDFRE
			// takes no local slot; may take an S-V slot
			if (ldb (FEF_SPECIAL_BIT, arg_desc))
				sv_slot++;
			arg_desc_ptr++;
			goto next;
			
		case FEF_ARG_INTERNAL:	  
		case FEF_ARG_INTERNAL_AUX:
			// QBDINT
			if (ldb (FEF_SPECIAL_BIT, arg_desc)) {
				// IF SPECIAL, NO LOCAL SLOT, TAKES S-V SLOT
				sv_slot++;
				arg_desc_ptr++;
				goto next;
			}
			
			if (A_LOCALP == -1)
				set_local_block (argidx); // ALSO MUST LOCATE LOCAL BLOCK
			
			// IF LOCAL, IGNORE AT BIND TIME BUT RESERVE LOCAL SLOT
			pdl_push (SYM_NIL);
			break;
		}

		// QBD1A
		argidx++;
		if (ldb (FEF_SPECIAL_BIT, arg_desc))
			QBLSPCL ();
		arg_desc_ptr++;
		
		
	next:; // QBD1
	}

	// QBD2
	// HERE WHEN BIND DESC LIST HAS BEEN USED UP
done:
	if (A_LOCALP == -1)
		set_local_block (argidx); // SET UP LOCAL BLOCK

	tmp = dpb (A_LOCALP - A_AP, LP_ENS_MACRO_LOCAL_BLOCK_ORIGIN, FIX_ZERO);
	tmp = dpb (A_R, LP_ENS_NUM_ARGS_SUPPLIED, tmp);
	pdl_store (A_AP + LP_ENTRY_STATE, tmp);

	// FINISH LINEARLY ENTERING
	// QLENX
	LC_fef = A_A;
	LC_pc = new_pc;
	A_IPMARK = A_AP; // redundant with first of QLENTR? 
	if (A_ERROR_SUBSTATUS != 0) {
		pdl_push (make_pointer (dtp_fix, A_ERROR_SUBSTATUS));
		trap ("FUNCTION-ENTRY");
	}
}


/*
 * Leave a frame when we're running just macrocode, and no micro-stack needs to be saved.
 * This routine saves and clears M-QBBFL, and saves the LC (even if the current frame
 * is not a FEF frame; in that case it won't be looked at).
 */
void
QLLV (void)
{
	int pdl_off, exit_state;

	pdl_off = A_AP + LP_EXIT_STATE;
	exit_state = pdl_ref (pdl_off);
	exit_state = dpb (LC_pc, LP_EXS_EXIT_PC, exit_state);
	if (ldb (M_FLAGS_QBBFL, A_FLAGS))
		exit_state = dpb (1, LP_EXS_BINDING_BLOCK_PUSHED,
				  exit_state);
	else
		exit_state = dpb (0, LP_EXS_BINDING_BLOCK_PUSHED,
				  exit_state);

	A_FLAGS = dpb (0, M_FLAGS_QBBFL, A_FLAGS);
	pdl_store (pdl_off, exit_state);
}


// enable_leave is only 0 when doing SG-ENTER-CALL
void
QMRCL (void (*leave)(void))
{

	if (ldb (M_FLAGS_TRAP_ON_CALLS, A_FLAGS))
		trap ("qmrcl");

	A_R = pdl_top - A_IPMARK; // arg count 

	A_A = pdl_ref (A_IPMARK); /* get the function to call */
	D_QMRCL (leave);
}

void
D_QMRCL (void (*leave)(void))
{
	/* D-QMRCL */
retry:
	switch (q_data_type (A_A)) {
	case dtp_symbol:
		A_A = vmread_transport (A_A + 2);
		pdl_store (A_IPMARK, A_A);
		goto retry;
	case dtp_fix:
	case dtp_extended_number:
	case dtp_small_flonum:
		if (leave) (*leave)();
		NUMBER_CALLED_AS_FUNCTION ();
		break;
	case dtp_locative:
	case dtp_list:
		if (leave) (*leave)();
		INTP1 ();
		break;
	case dtp_u_code_entry:
		if (leave) (*leave)();
		QME1 ();
		break;
	case dtp_fef_pointer:
		if (leave) (*leave)();
		QLENTR ();
		break;
	case dtp_array_pointer:
		/* no QLLV */
		QARYR ();
		break;
	case dtp_stack_group:
		if (leave) (*leave)();
		SG_CALL ();
		break;
	case dtp_closure:
	case dtp_stack_closure:
		if (leave) (*leave)();
		QCLS ();
		break;
	case dtp_select_method:
		if (leave) (*leave)();
		CALL_SELECT_METHOD ();
		break;
	case dtp_instance:
		if (leave) (*leave)();
		CALL_INSTANCE ();
		break;
	case dtp_entity:
		if (leave) (*leave)();
		CALL_ENTITY ();
		break;
	default:
		ILLOP ("calling bad object");
	}
}

void
store_dest (void)
{
	// QMDTBD
	switch (ldb (M_INST_DEST, macro_inst)) {
	case D_IGNORE:
		break;
	case D_PDL:
	case D_NEXT:
		pdl_push ((A_T & Q_ALL_BUT_CDR_CODE) | (CDR_NEXT<<30));
		break;

	case D_LAST:
		pdl_push ((A_T & Q_ALL_BUT_CDR_CODE) | (CDR_NIL<<30)); 
		QMRCL (QLLV);
		break;

	case D_RETURN:
		QMDDR (0);
		break;

	case D_NEXT_LIST:
		ILLOP ("D-NEXT-LIST");
		break;

	default:
		ILLOP ("bad dest");
	}
}

void
QIMOVE(void)
{
	fetch_arg ();

	if (dflags & DBG_QIMOVE) {
		printf ("QIMOVE=");
		print_q (A_T);
		printf ("\n");
	}

	store_dest ();
}

void
QICAR(void)
{
	// QICAR
	fetch_arg ();
	QCAR ();
	store_dest ();
}

void
QICDR(void) 
{
	fetch_arg ();
	QCDR ();
	store_dest ();
}

void
QICAAR(void)
{
	// QICAR
	fetch_arg ();
	QCAR ();
	QCAR ();
	store_dest ();
}

void
QICADR(void)
{
	fetch_arg ();
	QCDR ();
	QCAR ();
	store_dest ();
}

void
QICDAR(void) 
{
	fetch_arg ();
	QCAR ();
	QCDR ();
	store_dest ();
}

void
QICDDR(void) 
{
	fetch_arg ();
	QCDR ();
	QCDR ();
	store_dest ();
}

void qi_unimp(void) {ILLOP("qi_unimp");}

int
get_branch_delta (void)
{
	int delta;
	delta = ldb (M_INST_ADR, macro_inst);
	if (delta & 0400) {
		if (delta != 0777) {
			/* sign extend */
			delta |= (-1 << 9);
		} else if (delta == 0777) {
			/* long branch */
			fetch_macro_inst ();
			delta = macro_inst;
			if (delta & 0100000)
				/* sign extend */
				delta |= (-1 << 16);
		}
	}
	return (delta);
}

void
QIBRN(void)
{
	int delta;
	// QIBRN

	/*
	 * do this whether or not the branch is taken, since
	 * it takes care of skipping the second word of a 
	 * long branch
	 */
	delta = get_branch_delta ();

	switch (ldb (M_INST_DEST, macro_inst)) {
	case 0: // QBRALW ... BR
		LC_pc += delta;
		break;

	case 1: // QBRNL ... BR-NIL
		if (q_typed_pointer (A_T) == SYM_NIL)
			LC_pc += delta;
		break;
		
	case 2: // QBRNNL ... BR-NOT-NIL
		if (q_typed_pointer (A_T) != SYM_NIL)
			LC_pc += delta;
		break;

	case 3: // QBRNLP ... BR-NIL-POP
		// BR NIL, POP IF NOT
		if (q_typed_pointer (A_T) == SYM_NIL)
			LC_pc += delta;
		else
			pdl_pop ();
		break;

	case 4: // QBRNNP ... BR-NOT-NIL-POP
		if (q_typed_pointer (A_T) != SYM_NIL)
			LC_pc += delta;
		else
			pdl_pop ();
		break;

	case 5: // QBRAT ... BR-ATOM
		if (q_data_type (A_T) != dtp_list)
			LC_pc += delta;
		break;

	case 6: // QBRNAT ... BR-NOT-ATOM
		if (q_data_type (A_T) == dtp_list)
			LC_pc += delta;
		break;

	case 7: ILLOP ("bad branch");
	}
}

// car of A_T into A_T
void
QCAR (void)
{
	switch (q_data_type (A_T)) {
	case dtp_symbol:
		if (q_typed_pointer (A_T) == SYM_NIL) {
			A_T = SYM_NIL;
			return;
		}
		break;
	case dtp_locative:
	case dtp_list:
		A_T = q_typed_pointer (vmread_transport (A_T));
		return;
	default:
		break;
	}

	trap ("QCAR");
}

void
QCAR3 (void)
{
	A_T = q_typed_pointer (vmread_transport (A_T));
}

void
QCDR (void)
{
	// QCDR 

	switch (q_data_type (A_T)) {
	case dtp_symbol:
		if (q_typed_pointer (A_T) == SYM_NIL) {
			A_T = SYM_NIL;
			return;
		}
		break;
	case dtp_locative:
		A_T = q_typed_pointer (vmread_transport (A_T));
		return;
	case dtp_list:
		vmread_transport_cdr (A_T); // CHECK FOR INVZ, DON'T REALLY TRANSPORT
		switch (q_cdr_code (md)) {
		case CDR_NORMAL: 
			// CDR-FULL-NODE
			A_T = q_typed_pointer (vmread_transport (vma + 1));
			return;
		case CDR_ERROR:
			break;
		case CDR_NIL:
			// CDR-IS-NIL
			A_T = SYM_NIL;
			return;
		case CDR_NEXT:
			A_T++; // SAME DATA TYPE AS ARG
			return;
		}
	default:
		break;
	}

	trap ("cdr");
}

void MISC_TO_LIST (void) {ILLOP ("misc to list unimp");}

void
MISC_TO_LAST (void)
{
	pdl_push ((A_T & Q_ALL_BUT_CDR_CODE) | (CDR_NIL<<30)); 
	QMRCL (QLLV);
}

void
MISC_TO_RETURN (void)
{
	ILLOP ("misc to return unimp");
}


void
MISC_TO_STACK (void) 
{
	// MISC-TO-STACK
	pdl_push (q_typed_pointer (A_T) | (CDR_NEXT<<30));
}



typedef void (*misc_func_ptr)(void);

void
misc_last (void)
{
	// XLAST 
	A_T = pdl_peek ();
	pdl_push (A_T);

	// XLAST1
	while (q_data_type (A_T) == dtp_list) {
		pdl_store (pdl_top, A_T);
		QCDR ();
		/* XXX should check for sequence break */
	}

	A_T = pdl_pop ();
	pdl_pop ();
}

void
misc_boundp (void)
{
	Q sym;

	// XBOUNP
	if (q_data_type (pdl_peek ()) != dtp_symbol)
		trap ("boundp");
	sym = pdl_pop ();
	vmread_transport_write (q_pointer (sym) + sym_value);
	if (q_data_type (md) == dtp_null)
		A_T = SYM_NIL;
	else
		A_T = SYM_T;
}

void
misc_fboundp (void)
{
	Q sym;

	// XFCTNP
	if (q_data_type (pdl_peek ()) != dtp_symbol)
		trap ("fboundp");
	sym = pdl_pop ();
	vmread_transport_write (sym + sym_function);
	if (q_data_type (md) == dtp_null)
		A_T = SYM_NIL;
	else
		A_T = SYM_T;
}

void
misc_memq (void)
{
	Q needle;
	Q haystack;

	// XMEMQ 
	haystack = q_typed_pointer (pdl_pop ());
	needle = q_typed_pointer (pdl_pop ());

	XMEMQ1 (needle, haystack);
}

void
XMEMQ1 (Q needle, Q haystack)
{
	while (haystack != SYM_NIL) {
		A_T = haystack;
		QCAR ();
		if (A_T == needle) {
			A_T = haystack;
			return;
		}

		A_T = haystack;
		QCDR ();
		haystack = A_T;
	}
}

// (DEFMIC ASSQ 322 (X ALIST) T)
void
misc_assq (void)
{
	Q alist, key, pair;

	alist = q_typed_pointer (pdl_pop ());
	key = q_typed_pointer (pdl_pop ());

	while (alist != A_V_NIL) {
		A_T = alist;
		QCAR ();
		pair = A_T;
		QCAR ();
		if (A_T == key) {
			A_T = pair;
			return;
		}
		A_T = alist;
		QCDR ();
		alist = A_T;
	}

	A_T = A_V_NIL;
}

void
misc_numberp (void)
{
	switch (q_data_type (pdl_pop ())) {
	case dtp_fix:
	case dtp_extended_number:
	case dtp_small_flonum:
		A_T = SYM_T;
		break;
	case dtp_null:
	case dtp_free:
	case dtp_symbol:
	case dtp_symbol_header:
	case dtp_header:
	case dtp_locative:
	case dtp_list:
	case dtp_u_code_entry:
	case dtp_fef_pointer:
	case dtp_array_pointer:
	case dtp_array_header:
	case dtp_stack_group:
	case dtp_instance:
	case dtp_instance_header:
	case dtp_entity:
	case dtp_stack_closure:
		A_T = SYM_NIL;
		break;
	default:
		ILLOP ("numberp");
	}
}


void
misc_symbolp (void)
{
	if (q_data_type (pdl_pop ()) == dtp_symbol)
		A_T = SYM_T;
	else
		A_T = SYM_NIL;
}

void
misc_value_cell_location (void)
{
	Q sym;

	sym = pdl_pop ();

	if (q_data_type (sym) != dtp_symbol)
		trap ("value-cell-location");
	A_T = make_pointer (dtp_locative, sym + sym_value);
}

// (DEFMIC FUNCTION-CELL-LOCATION 362 (SYMBOL) T)
void
misc_function_cell_location (void)
{
	Q sym;

	sym = pdl_pop ();

	if (q_data_type (sym) != dtp_symbol)
		trap ("value-cell-location");
	A_T = make_pointer (dtp_locative, sym + sym_function);
}

void
misc_p_contents_offset (void)
{
	Q base, off;

	// XOMR
	off = pdl_pop ();
	base = pdl_pop ();

	vma = base;
	read_transport_header ();
	
	A_T = q_typed_pointer (vmread_transport (vma + off));
}

// Provides a way to pick up the pointer-field of an external-value-cell
// pointer or a dtp-null pointer, or any invisible pointer,
// converting it into a locative and transporting it if it points to old-space.
void
misc_p_contents_as_locative_offset (void)
{
	Q off;
	Q base;
	Q prev;

	off = q_pointer (pdl_pop ());
	base = pdl_pop ();

	vma = base;
	read_transport_header ();
	
	vma = q_pointer (base) + off;
	do {
		prev = vmread (vma);
		trans_old0 ();
	} while (prev != md);
	A_T = make_pointer (dtp_locative, md);
}

void
misc_p_ldb (void)
{
	Q ppss, len, pos;
	// %P-LDB treats target Q just as 32 bits.
	// Data type is not interpreted.
	// VMA MAY POINT AT UNBOXED DATA.

	A_1 = vmread (pdl_pop ());

	// XLLDB1
	ppss = pdl_pop ();
	if (q_data_type (ppss) != dtp_fix)
		trap ("fix");

	len = get_field (ppss, 6, 0);
	pos = get_field (ppss, 6, 6);

	if (len > 24)
		trap ("ARGTYP FIXNUM-FIELD PP 0");

	A_T = make_pointer (dtp_fix, get_field (A_1, len, pos));
}

// (DEFMIC %P-LDB-OFFSET 605 (PPSS POINTER OFFSET) T)
void
misc_p_ldb_offset (void)
{
	Q ppss, len, pos;

	// XOMR0
	A_B = pdl_pop (); // GET THE OFFSET
	A_C = pdl_pop (); // POINTER
	vmread_transport_header (A_C); // read header, follow forwarding
	A_1 = vmread (vma + A_B);

	// XLLDB1
	ppss = pdl_pop ();
	if (q_data_type (ppss) != dtp_fix)
		trap ("fix");

	len = get_field (ppss, 6, 0);
	pos = get_field (ppss, 6, 6);

	if (len > 24)
		trap ("ARGTYP FIXNUM-FIELD PP 0");

	A_T = make_pointer (dtp_fix, get_field (A_1, len, pos));
}

void
misc_p_dpb (void)
{
	Q value, ppss, dest;
	Q oldval;
	Q len, pos, mask_lo, mask;
	

	dest = pdl_pop ();
	ppss = pdl_pop ();
	value = pdl_pop ();

	if (q_data_type (ppss) != dtp_fix)
		trap ("ARGTYP FIXNUM PP 1");

	oldval = vmread (dest); // VMA MAY POINT TO UNBOXED DATA

	// XOPDP1
	len = get_field (ppss, 6, 0);
	pos = get_field (ppss, 6, 6);

	mask_lo = (1 << len) - 1;
	mask = mask_lo << pos;

	vmwrite (dest, (oldval & ~mask) | ((value << pos) & mask));

	A_T = A_V_NIL;
}

void
misc_p_store_tag_and_pointer (void)
{
	Q dest, tag_bits, pointer_bits;
	// XCMBS
	pointer_bits = q_pointer (pdl_pop ());
	tag_bits = pdl_pop ();
	dest = pdl_pop ();

	if (q_data_type (tag_bits) != dtp_fix)
		trap ("ARGTYP FIXNUM PP 2");
	
	vmwrite (dest, (tag_bits << 24) | pointer_bits);
	A_T = A_V_NIL;
}

void
misc_find_position_in_list (void)
{
	Q haystack, needle, count;

	// XFPIL
	haystack = pdl_pop ();
	needle = q_typed_pointer (pdl_pop ());
	count = FIX_ZERO;

	while (haystack != SYM_NIL) {
		A_T = haystack;
		QCAR ();
		if (A_T == needle) {
			A_T = count;
			return;
		}
		count++;
		A_T = haystack;
		QCDR ();
		haystack = A_T;
	}

	A_T = SYM_NIL;
}

void
FXUNPK_P_1 (void)
{
	A_1 = q_pointer (pdl_pop ());
	if (A_1 & BOXED_SIGN_BIT)
		A_1 |= (-1 << 24);
}

void
FXUNPK_T_2 (void)
{
	A_2 = q_pointer (A_T);
	if (A_2 & BOXED_SIGN_BIT)
		A_2 |= (-1 << 24);
}

void ARITH_XNM (void) { ILLOP ("ARITH_XNM"); }
void ARITH_SFL (void) { ILLOP ("ARITH_SFL"); }

int
D_NUMARG (int type)
{
	switch (type) {
	case dtp_fix:
		FXUNPK_P_1 ();
		return (0);
	case dtp_extended_number:
		ARITH_XNM ();
		break;
	case dtp_small_flonum:
		ARITH_SFL ();
		break;

	case dtp_symbol:
	case dtp_locative:
	case dtp_list:
	case dtp_u_code_entry:
	case dtp_fef_pointer:
	case dtp_array_pointer:
	case dtp_stack_group:
	case dtp_closure:
	case dtp_select_method:
	case dtp_instance:
	case dtp_instance_header:
	case dtp_entity:
	case dtp_stack_closure:
		trap ("d-numarg");
		break;

	case dtp_trap:
	case dtp_null:
	case dtp_free:
	case dtp_symbol_header:
	case dtp_header:
	case dtp_gc_forward:
	case dtp_external_value_cell_pointer:
	case dtp_one_q_forward:
	case dtp_header_forward:
	case dtp_body_forward:
	case dtp_array_header:
		ILLOP ("d-numarg");
		break;
	}

	return (1);
}

void ARITH_XNM_ANY (void) {ILLOP("unimp");}
void ARITH_SFL_ANY (void) {ILLOP("unimp");}
void ARITH_ANY_XNM (void) {ILLOP("unimp");}
void ARITH_FIX_SFL (void) {ILLOP("unimp");}

int
D_NUMARG1 (int type)
{
	switch (type) {
	case dtp_fix:
		FXUNPK_P_1 ();
		return (0);
	case dtp_extended_number:
		ARITH_XNM_ANY ();
		break;

	case dtp_small_flonum:
		ARITH_SFL_ANY ();
		break;
	case dtp_symbol:
	case dtp_locative:
	case dtp_list:
	case dtp_u_code_entry:
	case dtp_fef_pointer:
	case dtp_array_pointer:
	case dtp_stack_group:
	case dtp_closure:
	case dtp_select_method:
	case dtp_instance:
	case dtp_instance_header:
	case dtp_entity:
	case dtp_stack_closure:
		trap ("d-numarg");
		break;

	case dtp_trap:
	case dtp_null:
	case dtp_free:
	case dtp_symbol_header:
	case dtp_header:
	case dtp_gc_forward:
	case dtp_external_value_cell_pointer:
	case dtp_one_q_forward:
	case dtp_header_forward:
	case dtp_body_forward:
	case dtp_array_header:
		ILLOP ("d-numarg");
		break;
	}

	return (1);
}

/* for second arg */
int
D_FIXNUM_NUMARG2 (int type, int i_arg)
{
	switch (type) {
	case dtp_fix:
		FXUNPK_T_2 ();
		return (0);
	case dtp_extended_number:
		ARITH_ANY_XNM ();
		break;

	case dtp_small_flonum:
		ARITH_FIX_SFL ();
		break;
	case dtp_symbol:
	case dtp_locative:
	case dtp_list:
	case dtp_u_code_entry:
	case dtp_fef_pointer:
	case dtp_array_pointer:
	case dtp_stack_group:
	case dtp_closure:
	case dtp_select_method:
	case dtp_instance:
	case dtp_instance_header:
	case dtp_entity:
	case dtp_stack_closure:
		trap ("d-numarg");
		break;

	case dtp_trap:
	case dtp_null:
	case dtp_free:
	case dtp_symbol_header:
	case dtp_header:
	case dtp_gc_forward:
	case dtp_external_value_cell_pointer:
	case dtp_one_q_forward:
	case dtp_header_forward:
	case dtp_body_forward:
	case dtp_array_header:
		ILLOP ("d-numarg");
		break;
	}

	return (1);
}

void
FIX_OVERFLOW (int i_arg)
{
	ILLOP ("FIX_OVERFLOW");
}

int
test_fix_overflow (int val)
{
	return ((val ^ (val << 1)) & 0x01000000);
}

// DISPATCH TABLE FOR CHECKING FOR SINGLE-BIT ADD/SUBTRACT-TYPE FIXNUM OVERFLOW
// ON VALUE WHICH IS UNBOXED IN M-1.  DISPATCH ON SIGN BIT AND LOW DATA TYPE BIT.
// I-ARG SHOULD BE 0 IF RESULT ONLY TO M-T, OR 1 IF ALSO TO PDL.  
// IN ANY CASE, DOES ESSENTIALLY POPJ-XCT-NEXT.
// NEXT SHOULD BE INSTRUCTION TO BOX M-1 AS A FIXNUM.

void
FIXPACK_T (void)
{
	// Return it via M-T checking only for single-bit overflow
	if (test_fix_overflow (A_1)) {
		FIX_OVERFLOW (0);
		return;
	}
	A_T = make_pointer (dtp_fix, A_1);
}

void
FIXPACK_P (void)
{
	if (test_fix_overflow (A_1)) {
		FIX_OVERFLOW (1);
		return;
	}
	A_T = make_pointer (dtp_fix, A_1) | (CDR_NEXT<<30);
	pdl_push (A_T);
}

void
X1PLS (void)
{
	// X1PLS
	A_A = ARITH_1ARG_ADD1;
	if (D_NUMARG (q_data_type (pdl_peek ())) == 0) {
		A_1++;
		FIXPACK_T ();
		return;
	}
	/* non-zero returns means D_NUMARG already did everything */
}

void
misc_one_plus (void)
{
	X1PLS ();
}

void
X1MNS (void)
{
	A_A = ARITH_1ARG_SUB1;
	if (D_NUMARG (q_data_type (pdl_peek ())) == 0) {
		A_1--;
		FIXPACK_T ();
		return;
	}
	/* non-zero returns means D_NUMARG already did everything */
}

void
misc_one_minus (void)
{
	X1MNS ();
}

void
misc_instance_loc (void)
{
	Q instance, idx, hdr, size;

	idx = q_pointer (pdl_pop ());
	instance = pdl_pop ();
	if (q_data_type (instance) != dtp_instance)
		trap ("XINSTANCE-LOC");
	hdr = vmread_transport_header (instance);
	if (q_data_type (hdr) != dtp_instance_header)
		trap ("bad instance header");
	instance = vma; // Possibly-forwarded instance

	size = vmread (md + INSTANCE_DESCRIPTOR_SIZE);

	// Don't access the header!
	if (idx == 0)
		trap ("ARGTYP PLUSP M-1 1 NIL %INSTANCE-LOC");

	if (idx >= size)
		trap ("SUBSCRIPT-OOB M-1 M-2");

	A_T = make_pointer (dtp_locative, q_pointer (instance) + idx);
}

void
misc_instance_ref (void)
{
	misc_instance_loc ();
	A_T = q_typed_pointer (vmread_transport (A_T));
}

// (DEFMIC %INSTANCE-SET 522 (VAL INSTANCE INDEX) T)
void
misc_instance_set (void)
{
	misc_instance_loc ();
	A_S = A_T;
	A_T = pdl_pop ();

	vmread_transport_write (A_S);
	md = (md & ~Q_TYPED_POINTER) | q_typed_pointer (A_T);
	vmwrite_gc_write_test (vma, md);
	// NO SEQ BRK, CALLED BY MVR (???)
	A_T = A_S;
}

#define MOST_NEGATIVE_FIXNUM (-1<<23)
#define MOST_POSITIVE_FIXNUM ((1<<23)-1)

void
return_m_1 (void)
{
	if (MOST_NEGATIVE_FIXNUM <= (int)A_1 && (int)A_1 <= MOST_POSITIVE_FIXNUM) {
		A_T = make_pointer (dtp_fix, A_1);
		return;
	}
	ILLOP ("fixnum overflow");
}

void
misc_xbus_read (void)
{
	Q addr;

	addr = pdl_pop ();

	if (q_data_type (addr) != dtp_fix)
		trap ("xbus read");
	
	printf ("(%%xbus-read %#o)\n", q_pointer (addr));

	A_1 = 0;
	return_m_1 ();
}

// Takes a fixnum or bignum argument on the stack and returns the low-order 32
// bits of it in M-1.  Bashes M-I, M-J only.
void
GET_32_BITS (void)
{
	if (q_data_type (pdl_peek ()) == dtp_fix) {
		FXUNPK_P_1 ();
		return;
	}
	ILLOP ("get-32-bits");
}

void
misc_xbus_write (void)
{
	Q addr;

	GET_32_BITS ();  // M-1 gets value to write
	if (q_data_type (pdl_peek ()) != dtp_fix)
		trap ("xbus-write");

	addr = q_pointer (pdl_pop ());
	
	printf ("(%%xbus-write %#o %#o)\n", addr, A_1);

	switch (addr) {
	case 0377760:
	case 0377763: 
		/* used in setup-cpt for tv sync program */
		break;

	default:
		printf ("%%xbus-write to unknown addr %#o\n", addr);
		ILLOP ("xbus write");
	}

	A_T = FIX_ZERO;
}

void
misc_zerop (void)
{
	A_A = ARITH_1ARG_ZEROP;
	if (D_NUMARG (q_data_type (pdl_peek ())) == 0) {
		if (A_1 == 0)
			A_T = SYM_T;
		else
			A_T = SYM_NIL;
		return;
	}
	ILLOP ("misc_zerop");
}

void
misc_minusp (void)
{
	A_A = ARITH_1ARG_MINUSP;
	if (D_NUMARG (q_data_type (pdl_peek ())) == 0) {
		if ((int)A_1 < 0)
			A_T = SYM_T;
		else
			A_T = SYM_NIL;
		return;
	}
	ILLOP ("misc minusp");
}

void
misc_symeval (void)
{
	// XSYMEV
	if (q_data_type (pdl_peek ()) != dtp_symbol)
		trap ("ARGTYP SYMBOL PP T XSYME2");

	A_T = q_typed_pointer (vmread_transport (pdl_pop () + sym_value));
}

// (DEFMIC FSYMEVAL 600 (SYMBOL) T)
void
misc_fsymeval (void)
{
	if (q_data_type (pdl_peek ()) != dtp_symbol)
		trap ("ARGTYP SYMBOL PP T XSYME2");

	A_T = q_typed_pointer (vmread_transport (pdl_pop () + sym_function));
}

void
misc_xstore (void)
{
	// XXSTOR

	pdl_pop (); // STORE IN LAST ARRAY ELEM REF ED
	arr_base = A_QLARYH;
	gahd1 ();
	arr_idx = q_pointer (A_QLARYL);

	if (ldb (ARRAY_DISPLACED_BIT, arr_hdr)) {
		DSP_ARRAY_SETUP ();
	}

	if (arr_idx >= arr_nelts)
		trap ("SUBSCRIPT-OOB M-Q M-S"); // INDEX OUT OF BOUNDS

	A_T = pdl_pop ();
	ARRAY_TYPE_STORE_DISPATCH (ldb (ARRAY_TYPE_FIELD, arr_hdr));
}

void
misc_logdpb (void)
{
	Q fixnum, ppss, byte;
	Q len, pos, mask_lo, mask;
	Q result;

	// DPB FOR FIXNUMS ONLY, CAN STORE INTO SIGN BIT
	fixnum = pdl_pop ();
	ppss = pdl_pop ();
	byte = pdl_pop ();


	len = get_field (ppss, 6, 0);
	pos = get_field (ppss, 6, 6);

	mask_lo = (1 << len) - 1;
	mask = mask_lo << pos;

	result = (fixnum & ~mask) | ((byte << pos) & mask);
	result = make_pointer (dtp_fix, result);
	

	if (0) {
		printf ("(%%logdpb ");
		print_q (byte);
		printf (" ");
		print_q (ppss);
		printf (" ");
		print_q (fixnum);
		printf (") => ");
		print_q (result);
		printf ("\n");
	}

	A_T = result;
}

// (DEFMIC DPB 316 (VALUE PPSS WORD) T)
void
misc_dpb (void)
{
	// DPB never changes the sign of quantity DPB'ed into, it extends
	// the sign arbitrarily far to the left past the byte.

	if (q_data_type (pdl_ref (pdl_top - 2)) != dtp_fix)
		trap ("fix");

	// ONLY THE THIRD OPERAND IS 
	// PROCESSED VIA NUMARG. THUS DPB IS A
	// ONE OPERAND OP.

	A_A = ARITH_1ARG_DPB;
	if (D_NUMARG (q_data_type (pdl_peek ())) == 0) {
		// FIXNUM CASE.  DATA TO DPB INTO (ARG3) SIGN EXTENDED IN M-1.
		Q value, ppss;
		int len, pos;
		Q old_sign_bit, new_sign_bit;
		Q mask, mask_lo;

		ppss = pdl_pop ();
		value = pdl_pop ();

		if (q_data_type (ppss) != dtp_fix)
			trap ("fix");

		len = get_field (ppss, 6, 0);
		pos = get_field (ppss, 6, 6);

		if (len >= 24)
			trap ("dbp: bad len");
		
		A_2 = pos + len;

		if (A_2 < 32) {
			old_sign_bit = (A_1 & 0x80000000);
			mask_lo = (1 << len) - 1;
			mask = mask_lo << pos;
			A_1 = (A_1 & ~mask) | ((value << pos) & mask);
			new_sign_bit = (A_1 & 0x80000000);
			if (old_sign_bit == new_sign_bit) {
				return_m_1 ();
				return;
			}
			ILLOP ("dpb make bignum");
		} else {
			ILLOP ("dpb make bignum 2");
		}
	}
}

// (DEFMIC LDB 315 (PPSS WORD) T)

// LDB can only extract from fixnums and bignums.  The target is considered to
// have infinite sign extension.  LDB "should" always return a positive number.
// This issue currently doesn't arise, since LDB is implemented only for
// positive-fixnum-sized bytes, i.e. a maximum of 23. bits wide.  Note the
// presence of %LOGLDB, which will load a 24-bit byte of a fixnum and return
// it as a possibly-negative fixnum.
void
misc_ldb (void)
{
	Q ppss, len, pos;

	// Only the second operand is processed via NUMARG.  Thus LDB
	// is considered to be a one operand op.

	A_A = ARITH_1ARG_LDB;
	if (D_NUMARG (q_data_type (pdl_peek ())) == 0) {
		ppss = pdl_pop ();

		if (q_data_type (ppss) != dtp_fix)
			trap ("fix");

		len = get_field (ppss, 6, 0);
		pos = get_field (ppss, POINTER_BITS - 6, 6);

		if (len >= 24)
			trap ("ldb");

		while (pos + len > 32) {
			pos--;
			A_1 = (signed)A_1 >> 1;
		}
		
		A_T = FIX_ZERO | get_field (A_1, len, pos);
		return;
	}

	// else, bignum case, which D_NUMARG has already handled
}

void
misc_lsh (void)
{
	Q val, amount;
	int ival;

	amount = pdl_pop ();
	val = pdl_pop ();

	if (q_data_type (amount) != dtp_fix)
		trap ("ARGTYP FIXNUM PP 1 XLSH");

	if (q_data_type (val) != dtp_fix)
		trap ("ARGTYP FIXNUM PP 0 XLSH0");

	ival = q_pointer (amount);
	if (amount & BOXED_SIGN_BIT)
		ival |= (-1 << 24);

	val = q_pointer (val);

	if (ival < -24)
		val = 0;
	else if (ival < 0)
		val >>= (-ival);
	else if (ival < 24)
		val <<= ival;
	else
		val = 0;

	A_T = make_pointer (dtp_fix, val);
}

// GET TWO PDL ARGUMENTS, FIRST TO M-1, SECOND TO M-2
void
FXGTPP (void)
{
	// FXGTPP FIXGET FIXGET-1

	A_2 = pdl_pop ();
	A_1 = pdl_pop ();

	if (q_data_type (A_1) != dtp_fix)
		trap ("fix");

	if (q_data_type (A_2) != dtp_fix)
		trap ("fix");

	A_1 = q_pointer (A_1);
	if (A_1 & BOXED_SIGN_BIT)
		A_1 |= (-1 << 24);
	
	A_2 = q_pointer (A_2);
	if (A_2 & BOXED_SIGN_BIT)
		A_2 |= (-1 << 24);
}	

void
misc_24_bit_plus (void)
{
	// SPECIAL NON-OVERFLOW-CHECKING FUNCTIONS FOR WEIRD HACKS
	// X24ADD
	FXGTPP ();

	A_T = make_pointer (dtp_fix, A_1 + A_2);
}

void
misc_unibus_write (void)
{
	Q addr, data;

	data = pdl_pop ();
	addr = pdl_pop ();

	if (q_data_type (addr) != dtp_fix)
		trap ("fix");

	if (q_data_type (data) != dtp_fix)
		trap ("fix");

	addr = q_pointer (addr);

	switch (addr) {
	case 0764112: /* in set-mouse-mode */

		/* in write-unibus-map */
	case 0766140: case 0766142: case 0766144: case 0766146:
	case 0766150: case 0766152: case 0766154: case 0766156:
	case 0766160: case 0766162: case 0766164: case 0766166:
	case 0766170: case 0766172: case 0766174: case 0766176:
		break;
	default:
		printf ("%%unibus-write unknown addr %#o\n", addr);
		ILLOP ("unibus_write");
	}


	A_T = q_typed_pointer (data);
}

void
misc_data_type (void)
{
	A_T = make_pointer (dtp_fix, q_data_type (pdl_pop ()));
}

// GAHDRA GAHDR
void
check_array (void)
{
	if (q_data_type (arr_base) != dtp_array_pointer
	    && q_data_type (arr_base) != dtp_stack_group)
		trap ("not array");
}


// Pop index and array off stack, and return in VMA the address
// of the slot in the leader specified by the index.
Q
XFLAD1 (void)
{
	arr_idx = pdl_pop ();
	arr_base = pdl_pop ();

	if (q_data_type (arr_idx) != dtp_fix)
		trap ("fix");
	arr_idx = q_pointer (arr_idx);

	check_array ();

	gahd1 ();
	if (ldb (ARRAY_LEADER_BIT, arr_hdr) == 0)
		trap ("no leader");

	// GET LENGTH OF ARRAY LEADER
	// NO TRANSPORT SINCE JUST TOUCHED HEADER
	Q len = q_pointer (vmread (arr_base - 1));

	if (arr_idx >= len)
		trap ("leader out of bounds");

	return (arr_base - 2 - arr_idx); // XXX junk in type bits
}

// (DEFMIC ARRAY-LEADER 430 (ARRAY INDEX) T)
void
misc_array_leader (void)
{
	A_T = q_typed_pointer (vmread_transport (XFLAD1 ()));
}

// (DEFMIC AS-1 515 (VALUE ARRAY SUB) T)
void
misc_as_1 (void)
{
	Q val;

	// XAS1

	arr_idx = pdl_pop ();
	arr_base = pdl_pop ();
	val = pdl_pop ();

	if (q_data_type (arr_idx) != dtp_fix)
		trap ("fix");
	arr_idx = q_pointer (arr_idx);

	check_array ();
	gahd1 ();
	if (arr_ndim != 1)
		trap ("ARRAY-NUMBER-DIMENSIONS M-D 1 M-A");

	// XAS1A
	if (ldb (ARRAY_DISPLACED_BIT, arr_hdr))
		DSP_ARRAY_SETUP ();

	if (arr_idx >= arr_nelts)
		trap ("SUBSCRIPT-OOB M-Q M-S");

	
	A_T = val;
	ARRAY_TYPE_STORE_DISPATCH (ldb (ARRAY_TYPE_FIELD, arr_hdr));
}

// (DEFMIC STORE-ARRAY-LEADER 431 (X ARRAY INDEX) T)
void
misc_store_array_leader (void)
{
	Q loc = XFLAD1 ();

	// NEEDS TRANSPORTER HACKERY HERE IF ONE-Q-FORWARD S 
	// IN ARRAY-LEADERS ARE TO BE SUPPORTED.

	A_T = q_typed_pointer (pdl_pop ());
	vmwrite_gc_write_test (loc, A_T);
}

// (DEFMIC STACK-GROUP-RESUME 542 (SG X) T)
void
misc_stack_group_resume (void)
{
	Q called_sg_state, called_current_state;

	// SG-RESUME
	A_SG_TEM = pdl_pop (); //Get the value being transmitted.
	A_A = pdl_pop (); // Get the destination SG.
	A_SG_TEM1 = A_V_NIL; // Argument list.
	A_2 = A_A;

	called_sg_state = vmread (A_2 - (2 + SG_STATE));
	called_current_state = ldb (SG_ST_CURRENT_STATE, called_sg_state);
	TRAP_ON_BAD_SG_STATE (called_current_state);

	A_TEM = SG_STATE_AWAITING_CALL;
	SGLV ();

	// This is like SG-ENTER (q.v.) except that it doesn't set up
	// the PREVIOUS-STACK-GROUP at all, and so it takes no arg in
	// A-SG-TEM2.
	// SG-ENTER-NO-PREV

	A_QCSTKG = A_A;
	SGENT ();
	SG_ENTER_1 ();
}

// (DEFMIC ARRAYP 573 (X) T)
void
misc_arrayp (void)
{
	if (q_data_type (pdl_pop ()) == dtp_array_pointer)
		A_T = A_V_TRUE;
	else
		A_T = A_V_NIL;
}

// (DEFMIC STRINGP 575 (X) T)
void
misc_stringp (void)
{
	Q hdr, ndim;

	// A STRING IS DEFINED TO BE A ONE-D ARRAY OF TYPE ART-STRING OR ART-FAT-STRING.
	A_A = pdl_pop ();
	if (q_data_type (A_A) != dtp_array_pointer) {
		A_T = A_V_NIL;
		return;
	}

	hdr = vmread_transport_header (A_A);
	ndim = ldb (ARRAY_NUMBER_OF_DIMENSIONS, hdr);
	
	if (ndim != 1) {
		A_T = A_V_NIL;
		return;
	}

	switch (ldb (ARRAY_TYPE_FIELD, hdr)) {
	case ART_STRING:
	case ART_FAT_STRING:
		A_T = A_V_TRUE;
		break;
	default:
		A_T = A_V_NIL;
		break;
	}
}

// (DEFMIC RPLACA 327 (CONS X) T)
void
misc_rplaca (void)
{
	Q newval, cons, oldval;

	// XRPLCA
	newval = pdl_pop ();
	cons = pdl_pop ();

	// QRAR1
	switch (q_data_type (cons)) {
	case dtp_list:
	case dtp_locative:
		oldval = vmread_transport_write (cons);
		newval = (newval & Q_TYPED_POINTER) | (oldval & ~Q_TYPED_POINTER);
		vmwrite_gc_write_test (cons, newval);
		A_T = cons;
		break;
	default:
		trap ("rplaca");
		break;
	}
}

// (DEFMIC NOT 342 (X) T)
void
misc_not (void)
{
	if (q_typed_pointer (pdl_pop ()) == A_V_NIL)
		A_T = A_V_TRUE;
	else
		A_T = A_V_NIL;
}

// SUBROUTINE TO PICK UP THE PLIST OF THE OBJECT IN M-T, RETURNING IT IN M-T.
// RETURNS NIL IF A RANDOM TYPE, FOR MACLISP COMPATIBILITY.  UNFORTUNATELY
// NOT USEFUL FOR PLIST-CHANGING THINGS, BUT THOSE AREN'T CURRENTLY IN MICROCODE ANYWAY.
void
PLGET (void)
{
	Q dt;
	dt = q_data_type (A_T);
	if (dt == dtp_symbol) {
		// PLGET2
		A_T = q_typed_pointer (vmread_transport (A_T + sym_plist));
		return;
	}

	if (dt == dtp_list || dt == dtp_locative) {
		QCDR (); // "DISEMBODIED" PROPERTY LIST
		return;
	}

	A_T = A_V_NIL; // GET OF RANDOM THINGS NIL IN MACLISP, SO ...
}

// (DEFMIC GET 320 (SYMBOL PROPNAME) T)
void
misc_get (void)
{
	Q save;

	// XGET 
	A_A = q_typed_pointer (pdl_pop ()); // Arg2, property name.
	A_T = q_typed_pointer (pdl_pop ()); // Arg1, symbol or plist.
	A_B = A_T; // Save copy of arg in M-B.
	PLGET (); // Get the plist itself into M-T.

	while (A_T != A_V_NIL) {
		save = A_T;
		QCAR ();
		if (A_T == A_A) {
			A_T = save;
			QCDR ();
			QCAR ();
			break;
		}

		A_T = save;
		QCDR ();
		QCDR ();
	}
}

// (DEFMIC NTHCDR 420 (N LIST) T)
void
misc_nthcdr (void)
{
	Q n, i;

	A_T = q_typed_pointer (pdl_pop ()); // list

	if (q_data_type (pdl_peek ()) != dtp_fix)
		trap ("fix");
	n = q_pointer (pdl_pop ());
	if (n & BOXED_SIGN_BIT)
		trap ("nth neg arg");

	for (i = 0; i < n; i++) {
		if (A_T == A_V_NIL)
			break;
		QCDR ();
	}
}

// (DEFMIC NTH 417 (N LIST) T)
void
misc_nth (void)
{
	misc_nthcdr ();
	QCAR ();
}

int
isnumber (Q val)
{
	switch (q_data_type (val)) {
	case dtp_trap:
	case dtp_external_value_cell_pointer:
	case dtp_one_q_forward:
	case dtp_header_forward:
	case dtp_body_forward:
	case dtp_gc_forward:
		ILLOP ("isnumber");

	case dtp_null:
	case dtp_free:
	case dtp_symbol:
	case dtp_symbol_header:
	case dtp_header:
	case dtp_locative:
	case dtp_list:
	case dtp_u_code_entry:
	case dtp_fef_pointer:
	case dtp_array_pointer:
	case dtp_array_header:
	case dtp_stack_group:
	case dtp_closure:
	case dtp_select_method:
	case dtp_instance:
	case dtp_instance_header:
	case dtp_entity:
	case dtp_stack_closure:
		return (0);
	case dtp_fix:
	case dtp_extended_number:
	case dtp_small_flonum:
		return (1);
	}

	ILLOP ("isnumber");
}

void XEQUAL_0 (void);

// (DEFMIC EQUAL 335 (X Y) T)
void
misc_equal (void)
{
	// XEQUAL

	A_T = q_typed_pointer (pdl_pop ());
	A_B = q_typed_pointer (pdl_pop ());

	XEQUAL_0 ();
}

void XCHAR_EQUAL_1_2 (void);

// (DEFMIC CHAR-EQUAL 414 (CH1 CH2) T)
void
misc_char_equal (void)
{
	ILLOP ("check this");

	// XCHAR-EQUAL
	FXGTPP ();

	A_1 &= CH_CHAR;
	A_2 &= CH_CHAR;

	XCHAR_EQUAL_1_2 ();
}

void
XCHAR_EQUAL_1_2 (void)
{
	// Enter here with LDB'ed arguments in M-1, M-2
	if (A_1 == A_2) {
		// Equal if really equal
		A_T = A_V_TRUE;
		return;
	}

	A_T = q_typed_pointer (A_ALPHABETIC_CASE_AFFECTS_STRING_COMPARISON);
	if (A_T != A_V_NIL) {
		// Certainly not equal if case matters
		A_T = A_V_NIL;
		return;
	}

	A_1 = toupper (A_1);
	A_2 = toupper (A_2);
	if (A_1 == A_2) {
		A_T = A_V_TRUE;
		return;
	}

	A_T = A_V_NIL;
}

// (DEFMIC ARRAY-ACTIVE-LENGTH 552 (ARRAY) T)
void
XAAIXL (void)
{
	// this is a call to GAHDRA
	arr_base = pdl_pop ();
	check_array ();
	gahd1 ();

	if (ldb (ARRAY_LEADER_BIT, arr_hdr) != 0) {
		ILLOP ("check this");
		A_T = q_typed_pointer (vmread (arr_base - 2)); // Get fill pointer from leader
		if (q_data_type (A_T) == dtp_fix) {
			// if it's a fixnum, use it as a fill pointer
			return;
		}
	}

	if ((ldb (ARRAY_DISPLACED_BIT, arr_hdr)) != 0) {
		ILLOP ("check this");
		A_T = make_pointer (dtp_fix, vmread (arr_data + 1)); // DISPLACED, GET INDEX LENGTH
		return;
	}

	A_T = make_pointer (dtp_fix, arr_nelts);
}


// (DEFMIC %STRING-EQUAL 416 (STRING1 INDEX1 STRING2 INDEX2 COUNT) T)
void
XSTRING_EQUAL (void)
{
	//Arguments are the two strings (which must really be strings),
	//the two starting indices (which must be fixnums), and the
	//number of characters to compare.  If this count is a fixnum, it
	//is the number of characters to compare; if this runs off the end
	//of either string, they are not equal (no subscript-oob error occurs).
	//However, it won't work to have the starting-index greater than the
	//the length of the array (it is allowed to be equal).
	//If this count is NIL, the string's lengths are gotten via array-active-length.
	//Then if the lengths to be compared are not equal, the strings are not
	//equal, otherwise they are compared.  This takes care of the most common
	//cases, but is not the same as the STRING-EQUAL function.
	//Only the %%CH-CHAR field is compared.  There are no "case shifts".

	A_J = q_typed_pointer (pdl_pop ()); // Get count argument (typed)
	if (q_data_type (pdl_peek ()) != dtp_fix)
		trap ("fix");

	arr_idx = q_pointer (pdl_pop ()); // Index into second string (M-Q A-Q)
	XAAIXL (); // Get second string's length and decode array
	if (arr_ndim != 1)
		trap ("ARRAY-NUMBER-DIMENSIONS M-D 1 M-A");
	A_C = A_T - arr_idx; // First string's subrange length (typed)
	if (ldb (ARRAY_DISPLACED_BIT, arr_hdr))
		DSP_ARRAY_SETUP ();

	A_I = arr_idx; // Save parameters of second string
	A_K = arr_data;
	A_ZR = arr_hdr;

	if (q_data_type (pdl_peek ()) != dtp_fix)
		trap ("fix");
	arr_idx = q_pointer (pdl_pop ()); // Index into first string
	XAAIXL (); // Get first string's length and decode array
	if (arr_ndim != 1)
		trap ("ARRAY-NUMBER-DIMENSIONS M-D 1 M-A");
	A_T = A_T - arr_idx; // First string's subrange length (typed)
	if (ldb (ARRAY_DISPLACED_BIT, arr_hdr))
		DSP_ARRAY_SETUP ();

	if (A_J == A_V_NIL) {
		// no count supplied
		// XSTRING-EQUAL-2

		if (A_T != A_C) {
			A_T = A_V_NIL;
			return;
		}
		A_C = q_pointer (A_C);
	} else {
		if (q_data_type (A_J) != dtp_fix)
			trap ("ARGTYP FIXNUM M-J 4");
		
		// If count exceeds either array, 
		// then the answer is NIL.
		if (A_J > A_C || A_J > A_T) {
			A_T = A_V_NIL;
			return;
		}
		
		A_C = q_pointer (A_J); // Number of chars to be compared
	}

	// No bounds-checking required beyond this point
	// XSTRING-EQUAL-0

	if (A_C == 0) {
		// If no characters to compare, result is T
		A_T = A_V_TRUE;
		return;
	}
	A_C += arr_idx; // Highest location to reference in first str
	

	// XSTRING-EQUAL-1 This is the character-comparison loop (38-46 cycles/char)
	while (1) {
		A_BDIV_V1 = arr_idx;
		A_BDIV_V2 = arr_data;
		ARRAY_TYPE_REF_DISPATCH (ldb (ARRAY_TYPE_FIELD, arr_hdr));
		A_1 = A_T & CH_CHAR; // Character from first string

		arr_idx = A_I;
		arr_data = A_K;
		ARRAY_TYPE_REF_DISPATCH (ldb (ARRAY_TYPE_FIELD, A_ZR));
		A_2 = A_T & CH_CHAR; // Character from second string

		XCHAR_EQUAL_1_2 ();
		if (A_T == A_V_NIL)
			break;

		arr_idx = A_BDIV_V1;
		arr_data = A_BDIV_V2;

		arr_idx++;
		A_I++;

		// All chars equal => strings equal
		if (arr_idx >= A_C) {
			A_T = A_V_TRUE;
			break;
		}
	}
}

void
XEQUAL_0 (void)
{
	while (1) {
		if (A_T == A_B)
			goto ret_true;

		if (q_data_type (A_T) != q_data_type (A_B))
			goto ret_false;

		if (isnumber (A_T)) {
			A_A = ARITH_2ARG_EQUAL;
			if (D_NUMARG1 (q_data_type (pdl_peek ())) == 0) {
				/* non-eq fixnums */
				goto ret_false;
			}
			/* A_T already set */
			return;
		}

		if (q_data_type (A_T) == dtp_array_pointer) {
			// XEQUAL-ARRAY
			vmread_transport_header (A_T);
			A_1 = ldb (ARRAY_TYPE_FIELD, md);
			
			if (A_1 != ART_STRING && A_1 != ART_FAT_STRING)
				goto ret_false;

			// XEQUAL-STRING
			vmread_transport_header (A_B);
			A_2 = ldb (ARRAY_TYPE_FIELD, md);
			if (A_2 != ART_STRING && A_2 != ART_FAT_STRING)
				goto ret_false;

			pdl_push (A_T);
			pdl_push (FIX_ZERO);
			pdl_push (A_B);
			pdl_push (FIX_ZERO);
			pdl_push (A_V_NIL);
			XSTRING_EQUAL ();
			return;
		}

		if (q_data_type (A_T) != dtp_list)
			goto ret_false;
		
		pdl_push (A_T);
		pdl_push (A_B);

		QCAR3 ();
		A_B = A_T;

		A_T = pdl_peek ();
		QCAR3 ();

		XEQUAL_0 ();

		// XEQUAL-CDR
		if (A_T == A_V_NIL) {
			pdl_pop ();
			pdl_pop ();
			goto ret_false;
		}

		// If the cars match, tail-recursively check the two cdrs.
		A_T = pdl_pop ();
		QCDR ();
		A_B = A_T;
		
		A_T = pdl_pop ();
		QCDR ();
	}

ret_false:
	A_T = A_V_NIL;
	return;

ret_true:
	A_T = A_V_TRUE;
	return;
}

// (DEFMIC LENGTH 324 (LIST) T)
void
misc_length (void)
{
	Q count;

	A_T = q_typed_pointer (pdl_pop ());
	A_A = A_T;

	// XTLENG
	if (q_data_type (A_T) != dtp_list && A_T != A_V_NIL)
		trap ("M-T A-V-NIL TRAP");

	count = FIX_ZERO;
	while (1) {
		if (q_data_type (A_T) != dtp_list)
			break;
		count++;
		QCDR ();
	}

	A_T = make_pointer (dtp_fix, count);
}

// (DEFMIC %PUSH 534 (X) NIL)
// I would be rather surprised if this is ever called!!  
// Foo, I'm surprised!
void
misc_push (void)
{
	// "push" by neglecting to pop the arg */
	A_T = q_typed_pointer (pdl_peek ());
}

// (DEFMIC %ASSURE-PDL-ROOM 536 (ROOM) NIL)
void
misc_assure_pdl_room (void)
{
	Q size;

	// XAPDLR
	size = q_pointer (pdl_pop ()); // NUMBER OF PUSHES PLANNING TO DO
	size += (pdl_top - A_AP - 1); // CURRENT FRAME SIZE

	// NOTE FUDGE FACTOR OF 10 SINCE WE DON'T
	// CURRENTLY KNOW HOW MANY COMPILER-GENERATED
	// PUSHES MIGHT BE GOING TO HAPPEN
	if (size > 0370) {
		trap ("STACK-FRAME-TOO-LARGE");
	}
}

void
SBPL_ADI (void)
{
	A_1 = A_QLBNDP - A_QLBNDO; // STORE ADI-BIND-STACK-LEVEL ADI BLOCK
	pdl_push (make_pointer (dtp_fix, A_1));
	pdl_push (dpb (ADI_BIND_STACK_LEVEL, ADI_TYPE, FIX_ZERO)
		  | Q_FLAG_BIT);
}

// (DEFMIC %CATCH-OPEN 456 () NIL T)
void
misc_catch_open (void)
{
	Q val, idx;

	macro_inst = dpb (D_IGNORE, M_INST_DEST, macro_inst);
	A_T = make_pointer (dtp_u_code_entry, CATCH_U_CODE_ENTRY);
	A_S = q_typed_pointer (pdl_pop ());
	SBPL_ADI (); // PUSH ADI-BIND-STACK-LEVEL BLOCK

	pdl_push (A_S | Q_FLAG_BIT); // PUSH RESTART PC
	A_R = 0;
	//A_R = micro_stack_pointer; XXX

	val = dpb (A_R, ADI_RPC_MICRO_STACK_LEVEL, FIX_ZERO);
	val = dpb (ADI_RESTART_PC, ADI_TYPE, val);
	val |= Q_FLAG_BIT;
	pdl_push (val);

	// XCTO1
	open_call_block (ldb (M_INST_DEST, macro_inst));
	idx = A_IPMARK + LP_CALL_STATE;
	pdl_store (idx, dpb (1, LP_CLS_ADI_PRESENT, pdl_ref (idx)));
}


// (DEFMIC %OPEN-CALL-BLOCK 533 (FUNCTION ADI-PAIRS DESTINATION) NIL)
void
misc_open_call_block (void)
{
	Q idx, i, n;

	// STUFF FOR CALLS WITH NUMBER OF ARGUMENTS NOT KNOWN AT COMPILE TIME
	// AND FOR MAKING CALLS WITH SPECIAL ADI OF DIVERS SORTS

	A_C = q_pointer (pdl_pop ());
	A_A = q_pointer (pdl_pop ());
	A_T = q_typed_pointer (pdl_pop ());
	if (A_A == 0) {
		// If no ADI, push regular call block
		open_call_block (A_C);
		return;
	}

	idx = pdl_top; // ADI, fix the flag bits
	A_A *= 2; // 2 QS per ADI pair

	n = A_A * 2 - 1;
	for (i = 0; i < n; i++) {
		idx = pdl_top - i;
		pdl_store (idx, pdl_ref (idx) | Q_FLAG_BIT);
	}
	idx = pdl_top - i;
	pdl_store (idx, pdl_ref (idx) & ~Q_FLAG_BIT); // Clear flag bit in last wd of ADI

	open_call_block (A_C);

	idx = pdl_top + LP_CALL_STATE;
	pdl_store (idx, dpb (1, LP_CLS_ADI_PRESENT, pdl_ref (idx)));
}

// (DEFMIC %ACTIVATE-OPEN-CALL-BLOCK 535 () NIL)
void
misc_activate_open_call_block (void)
{
	// Fix CDR-code of last arg then activate call	
	pdl_store (pdl_top,
		   (pdl_ref (pdl_top) & ~Q_CDR_CODE)
		   | (CDR_NIL << 30));
	QMRCL (QLLV);
}


Q
get_fill_val (Q hdr)
{
	// FROM ARRAY EXHAUSTED
	// COMPUTE FILLER VALUE IN M-T
	switch (ldb (ARRAY_TYPE_FIELD, hdr)) {
	case ART_1B: case ART_2B: case ART_4B: case ART_8B: case ART_16B:
	case ART_32B: case ART_STRING: case ART_HALF_FIX: case ART_FLOAT:
	case ART_FPS_FLOAT: case ART_FAT_STRING: 
		return (FIX_ZERO);

	case ART_Q: case ART_Q_LIST: case ART_STACK_GROUP_HEAD:
	case ART_SPECIAL_PDL: case ART_REGULAR_PDL:
		return (A_V_NIL);

	default:
		trap ("bad array type");
		break;
	}
}

void
GADPTR (void)
{
	check_array ();
	gahd1 ();
	arr_idx = 0;
	if (ldb (ARRAY_DISPLACED_BIT, arr_hdr))
		DSP_ARRAY_SETUP ();
	
}

// (DEFMIC COPY-ARRAY-CONTENTS 500 (FROM TO) T)
void
misc_copy_array_contents (void)
{
	Q to_arr, from_arr;
	Q to_nelts, to_data, to_idx, to_hdr;
	Q to_fill;
	Q save_from_idx, save_from_data;

	// NOTE:  AN OPTIMIZATION TO DO IT WORD BY WORD MIGHT BE HANDY...
	// XCARC
	to_arr = pdl_pop ();
	from_arr = pdl_pop ();

	// XCARC0
	arr_base = to_arr;
	GADPTR ();
	to_nelts = arr_nelts; // TO LENGTH
	to_data = arr_data; // TO ADDRESS
	to_idx = arr_idx; // TO INITIAL INDEX
	to_hdr = arr_hdr; // TO ARRAY HEADER
	to_fill = get_fill_val (to_hdr);

	arr_base = from_arr; // FROM-ARRAY
	GADPTR ();

	// XCARC1
	while (to_idx < to_nelts) {
		if (arr_idx < arr_nelts) {
			// M-T := FROM ITEM, CLOBBER M-J
			ARRAY_TYPE_REF_DISPATCH (ldb (ARRAY_TYPE_FIELD, arr_hdr));
		} else {
			A_T = to_fill;
		}
		
		// XCARC4
		save_from_idx = arr_idx;
		save_from_data = arr_data;

		arr_idx = to_idx;
		arr_data = to_data;
		ARRAY_TYPE_STORE_DISPATCH (ldb (ARRAY_TYPE_FIELD, to_hdr));
		
		// XCARC5
		to_idx++;

		arr_idx = save_from_idx + 1;
		arr_data = save_from_data;
	}

	A_T = A_V_TRUE;
}

// (DEFMIC *BOOLE 352 (FN ARG1 ARG2) T)
void
misc_boole (void)
{
	// The 2nd arg of BOOLE becomes the A operand of the logical instruction.
	// The 3rd arg becomes the M operand.
	A_T = pdl_pop ();  // M operand - winds up in A_2 on right of exp
	A_A = pdl_pop ();  // A operand - winds up in A_1 on left of exp
	A_B = pdl_pop ();

	if (q_data_type (A_B) != dtp_fix)
		trap ("fix");

	pdl_push (A_A); // Put arg back in standard place

	if (D_NUMARG1 (q_data_type (pdl_peek ())) == 0) {
		if (D_FIXNUM_NUMARG2 (q_data_type (A_T), NUMBER_CODE_FIXNUM) == 0) {
			switch (A_B & 0xf) {
			case 0: A_1 = 0; break; // SETZ
			case 1: A_1 = A_1 & A_2; break; // AND
			case 2: A_1 = ~A_1 & A_2; break; // ANDCA
			case 3: A_1 = A_2; // SETM
			case 4: A_1 = ~A_2; // ANDCM
			case 5: A_1 = A_1; // SETA 
			case 6: A_1 = A_1 ^ A_2; // XOR
			case 7: A_1 = A_1 | A_2; // IOR
			case 010: A_1 = ~A_1 & ~A_2; // ANDCB
			case 011: A_1 = ~(A_1 ^ A_2); // EQV
			case 012: A_1 = ~A_1; // SETCA
			case 013: A_1 = ~A_1 | A_2; // ORCA
			case 014: A_1 = ~A_2; // SETCM
			case 015: A_1 = A_1 | ~A_2; // ORCM
			case 016: A_1 = ~A_1 | ~A_2; // ORCB
			case 017: A_1 = ~0; // SETO
			}
			A_T = make_pointer (dtp_fix, A_1);
		}
	}
	// else, A_T already set by dispatch subroutine 
}

void misc_car (void) {  A_T = pdl_pop(); QCAR(); }
void misc_cdr (void) {  A_T = pdl_pop(); QCDR(); }

void misc_caar (void) {  A_T = pdl_pop(); QCAR(); QCAR(); }
void misc_cadr (void) {  A_T = pdl_pop(); QCDR(); QCAR(); }
void misc_cdar (void) {  A_T = pdl_pop(); QCAR(); QCDR(); }
void misc_cddr (void) {  A_T = pdl_pop(); QCDR(); QCDR(); }

void misc_caaar (void) {  A_T = pdl_pop(); QCAR(); QCAR(); QCAR(); }
void misc_caadr (void) {  A_T = pdl_pop(); QCDR(); QCAR(); QCAR(); }
void misc_cadar (void) {  A_T = pdl_pop(); QCAR(); QCDR(); QCAR(); }
void misc_caddr (void) {  A_T = pdl_pop(); QCDR(); QCDR(); QCAR(); }
void misc_cdaar (void) {  A_T = pdl_pop(); QCAR(); QCAR(); QCDR(); }
void misc_cdadr (void) {  A_T = pdl_pop(); QCDR(); QCAR(); QCDR(); }
void misc_cddar (void) {  A_T = pdl_pop(); QCAR(); QCDR(); QCDR(); }
void misc_cdddr (void) {  A_T = pdl_pop(); QCDR(); QCDR(); QCDR(); }

void misc_caaaar (void) {  A_T = pdl_pop(); QCAR(); QCAR(); QCAR(); QCAR(); }
void misc_caaadr (void) {  A_T = pdl_pop(); QCDR(); QCAR(); QCAR(); QCAR(); }
void misc_caadar (void) {  A_T = pdl_pop(); QCAR(); QCDR(); QCAR(); QCAR(); }
void misc_caaddr (void) {  A_T = pdl_pop(); QCDR(); QCDR(); QCAR(); QCAR(); }
void misc_cadaar (void) {  A_T = pdl_pop(); QCAR(); QCAR(); QCDR(); QCAR(); }
void misc_cadadr (void) {  A_T = pdl_pop(); QCDR(); QCAR(); QCDR(); QCAR(); }
void misc_caddar (void) {  A_T = pdl_pop(); QCAR(); QCDR(); QCDR(); QCAR(); }
void misc_cadddr (void) {  A_T = pdl_pop(); QCDR(); QCDR(); QCDR(); QCAR(); }

void misc_cdaaar (void) {  A_T = pdl_pop(); QCAR(); QCAR(); QCAR(); QCDR(); }
void misc_cdaadr (void) {  A_T = pdl_pop(); QCDR(); QCAR(); QCAR(); QCDR(); }
void misc_cdadar (void) {  A_T = pdl_pop(); QCAR(); QCDR(); QCAR(); QCDR(); }
void misc_cdaddr (void) {  A_T = pdl_pop(); QCDR(); QCDR(); QCAR(); QCDR(); }
void misc_cddaar (void) {  A_T = pdl_pop(); QCAR(); QCAR(); QCDR(); QCDR(); }
void misc_cddadr (void) {  A_T = pdl_pop(); QCDR(); QCAR(); QCDR(); QCDR(); }
void misc_cdddar (void) {  A_T = pdl_pop(); QCAR(); QCDR(); QCDR(); QCDR(); }
void misc_cddddr (void) {  A_T = pdl_pop(); QCDR(); QCDR(); QCDR(); QCDR(); }




// (DEFMIC %ARGS-INFO 532 (FUNCTION) T)
void
misc_args_info (void)
{
	Q tem, ndim;

	// FUNCTION CAN BE ANYTHING MEANINGFUL IN FUNCTION
	// CONTEXT. RETURNS FIXNUM.  FIELDS AS IN
	// NUMERIC-ARG-DESC-INFO IN QCOM.
	// XARGI

	A_S = pdl_pop ();

	A_T = dpb (-1, ARG_DESC_MAX_ARGS, FIX_ZERO);
	A_T = dpb (1, ARG_DESC_INTERPRETED, A_T);


	// ENTER HERE FROM APPLY, ALSO REENTER TO TRY AGAIN (CLOSURE, ETC).
	// XARGI0

top:
	A_S = q_typed_pointer (A_S);

	switch (q_data_type (A_S)) {
	case dtp_trap:
	case dtp_free:
	case dtp_symbol_header:
	case dtp_header:
	case dtp_gc_forward:
	case dtp_external_value_cell_pointer:
	case dtp_one_q_forward:
	case dtp_header_forward:
	case dtp_body_forward:
	case dtp_list:
	case dtp_array_header:
	case dtp_instance_header:
		ILLOP ("args-info");
		
	case dtp_null:
	case dtp_fix:
	case dtp_extended_number:
	case dtp_locative:
	case dtp_small_flonum:
		return;

	case dtp_select_method:
		// CAN'T SAY WITHOUT KEY SO BE CONSERVATIVE
		return;

	case dtp_instance:
		// INSTANCE: (COULD GET FUNCTION BUT WHY BOTHER
		// SINCE IT WILL BE A SELECT-METHOD ANYWAY)
		return;

	case dtp_symbol:
		// replace with fctn cell
		A_S = vmread_transport (A_S + sym_function);
		goto top;

	case dtp_u_code_entry:
		tem = vmread_transport (A_V_MICRO_CODE_ENTRY_AREA + A_S);
		if (q_data_type (tem != dtp_fix)) {
			A_S = tem;
			goto top;
		}

		vmread_transport (A_V_MICRO_CODE_ENTRY_ARGS_INFO_AREA + A_S);
		A_T = q_typed_pointer (md);
		return;
			
	case dtp_fef_pointer:
		A_T = vmread_transport (A_S + FEFHI_FAST_ARG_OPT);
		A_T = q_typed_pointer (A_T);
		return;

	case dtp_array_pointer:
		tem = vmread_transport_header (A_S);
		ndim = ldb (ARRAY_NUMBER_OF_DIMENSIONS, tem);
		A_T = dpb (ndim, ARG_DESC_MIN_ARGS, FIX_ZERO);
		A_T = dpb (ndim, ARG_DESC_MAX_ARGS, A_T);
		return;

	case dtp_stack_group:
		// STACK GROUP ACCEPTS ANY NUMBER OF EVALED ARGS
		A_T = dpb (-1, ARG_DESC_MAX_ARGS, FIX_ZERO);
		return;

	case dtp_closure:
	case dtp_entity:
	case dtp_stack_closure:
		A_T = make_pointer (dtp_list, A_S);
		QCAR ();
		A_S = A_T;
		goto top;

	}

	ILLOP ("args_info");
}

void
SCAV0 (void)
{
}

void
SCAVT (void)
{
}

// DECODE AREA SPEC IN M-S.  RETURN FIXNUM, WITH DATA-TYPE, IN M-S.
// THIS CAN CALL TRAP OR JUMP TO IT, THUS CALLER MUST HAVE (ERROR-TABLE ARGTYP AREA M-S NIL)
// M-S MUST HAVE DATA-TYPE AND NO CDR-CODE/FLAG.
Q
CONS_GET_AREA (Q area)
{
	if (q_data_type (area) == dtp_symbol)
		area = vmread_transport (area + sym_value);
	if (q_data_type (area) != dtp_fix)
		trap ("ARGTYP AREA M-S NIL CONS-GET-AREA");
	if (q_pointer (area) > SIZE_OF_AREA_ARRAYS)
		trap ("cons-get-area");
	return (area);
}

Q
lcons_cache (Q area, Q size)
{
	Q result;

	if (ldb (M_FLAGS_TRANSPORT, A_FLAGS))
		return (A_V_NIL); // Transporter must avoid cache
	
	if (area != A_LCONS_CACHE_AREA)
		return (A_V_NIL);

	if (A_LCONS_CACHE_FREE_POINTER + size > A_LCONS_CACHE_FREE_LIMIT)
		return (A_V_NIL);

	result = A_LCONS_CACHE_FREE_POINTER; // Allocate it here
	A_LCONS_CACHE_FREE_POINTER += size;
	
	SCAV0 ();

	return (result);
}

void
LCONS5 (void)
{
	ILLOP ("LCONS5");
}

// area is M-S; size is M-B
Q
lcons (Q area, Q size)
{
	Q result;
	Q region, region_bits;
	Q type, rep;
	Q region_length;
	Q old_free_pointer, new_free_pointer;
	Q region_origin;

	if (area == A_V_NIL)
		area = q_typed_pointer (A_CNSADF);

	if (size <= 0)
		trap ("CONS-ZERO-SIZE M-B");

	if ((result = lcons_cache (area, size)) != A_V_NIL)
		return (result);

	area = CONS_GET_AREA (area);
	region = q_pointer (vmread (A_V_AREA_REGION_LIST + area));

keep_going:
	while ((region & BOXED_SIGN_BIT) == 0) {
		region_bits = vmread (A_V_REGION_BITS + region);
		type = ldb (REGION_SPACE_TYPE, region_bits);
		rep = ldb (REGION_REPRESENTATION_TYPE, region_bits);
		
		if (rep == REGION_REPRESENTATION_TYPE_LIST) {
			switch (type) {
			case REGION_SPACE_FREE:
				ILLOP ("bad region");
				
			case REGION_SPACE_OLD:
				break;

			case REGION_SPACE_NEW:
				if (ldb (M_FLAGS_TRANSPORT, A_FLAGS) == 0)
					goto win;
				break;
				
			case REGION_SPACE_COPY:
				if (ldb (M_FLAGS_TRANSPORT, A_FLAGS) != 0)
					goto win;
				break;
				
			default:
				ILLOP ("cons: unknown region space type");
			}
		}
			
		region = vmread (A_V_REGION_LIST_THREAD + region);
	}

	LCONS5 ();

win:

	// CONSF
	//This region is the right type, see if adequate free space, if so do it.
	region_length = q_pointer (vmread (A_V_REGION_LENGTH + region));
	old_free_pointer = q_pointer (vmread (A_V_REGION_FREE_POINTER + region));
	new_free_pointer = old_free_pointer + size; // Proposed new free pointer
	if (new_free_pointer > region_length) {
		region = vmread (A_V_REGION_LIST_THREAD + region);
		goto keep_going;
	}

	region_origin = q_pointer (vmread (A_V_REGION_ORIGIN + region));
	result = q_pointer (region_origin + old_free_pointer);

	if (ldb (M_FLAGS_TRANSPORT, A_FLAGS) == 0) {
		A_LCONS_CACHE_AREA = area;
		A_LCONS_CACHE_REGION = region;
		A_LCONS_CACHE_REGION_ORIGIN = region_origin;
		A_LCONS_CACHE_FREE_POINTER = region_origin + new_free_pointer;
		A_LCONS_CACHE_FREE_LIMIT = region_origin + region_length;

		SCAV0 ();
	} else {
		SCAVT ();
	}

	return (result);
}

// (DEFMIC CONS 366 (X Y) T)
void
misc_cons (void)
{
	Q carval, cdrval;
	A_T = lcons (A_CNSADF, 2); // default cons area

	cdrval = q_typed_pointer (pdl_pop ());
	carval = q_typed_pointer (pdl_pop ());

	vmwrite_gc_write_test (A_T, carval | (CDR_NORMAL<<30));
	vmwrite_gc_write_test (A_T+1, cdrval | (CDR_ERROR<<30));

	A_T = make_pointer (dtp_list, A_T);
}

// (DEFMIC (> . M->) 412 (NUM1 NUM2) T)
void
misc_greater (void)
{
	A_T = pdl_pop ();
	QIGRP ();
}

// (DEFMIC (EQ . M-EQ) 633 (X Y) T)
void
misc_eq (void)
{
	Q x, y;

	x = q_typed_pointer (pdl_pop ());
	y = q_typed_pointer (pdl_pop ());
	if (x == y)
		A_T = A_V_TRUE;
	else
		A_T = A_V_NIL;
}

struct misc_func {
	char *name;
	misc_func_ptr func;
};

struct misc_func misc_funcs[01000] = {
	[0242] = { "CAR", misc_car }, 
	[0243] = { "CDR", misc_cdr },
	[0244] = { "CAAR", misc_caar },
	[0245] = { "CADR", misc_cadr },
	[0246] = { "CDAR", misc_cdar },
	[0247] = { "CDDR", misc_cddr },
	[0250] = { "CAAAR", misc_caaar },
	[0251] = { "CAADR", misc_caadr },
	[0252] = { "CADAR", misc_cadar },
	[0253] = { "CADDR", misc_caddr },
	[0254] = { "CDAAR", misc_cdaar },
	[0255] = { "CDADR", misc_cdadr },
	[0256] = { "CDDAR", misc_cddar },
	[0257] = { "CDDDR", misc_cdddr },
	[0260] = { "CAAAAR", misc_caaaar },
	[0261] = { "CAAADR", misc_caaadr },
	[0262] = { "CAADAR", misc_caadar },
	[0263] = { "CAADDR", misc_caaddr },
	[0264] = { "CADAAR", misc_cadaar },
	[0265] = { "CADADR", misc_cadadr },
	[0266] = { "CADDAR", misc_caddar },
	[0267] = { "CADDDR", misc_cadddr },
	[0270] = { "CDAAAR", misc_cdaaar },
	[0271] = { "CDAADR", misc_cdaadr },
	[0272] = { "CDADAR", misc_cdadar },
	[0273] = { "CDADDR", misc_cdaddr },
	[0274] = { "CDDAAR", misc_cddaar },
	[0275] = { "CDDADR", misc_cddadr },
	[0276] = { "CDDDAR", misc_cdddar },
	[0277] = { "CDDDDR", misc_cddddr },
	[0303] = { "%DATA-TYPE", misc_data_type },
	[0314] = { "%LOGDPB", misc_logdpb },
	[0315] = { "LDB", misc_ldb },
	[0316] = { "DPB", misc_dpb },
	[0317] = { "%P-STORE-TAG-AND-POINTER", misc_p_store_tag_and_pointer },
	[0320] = { "GET", misc_get },
	[0322] = { "ASSQ", misc_assq },
	[0323] = { "LAST", misc_last },
	[0342] = { "NOT", misc_not },
	[0322] = { "ASSQ", misc_assq },
	[0324] = { "LENGTH", misc_length },
	[0325] = { "1+", misc_one_plus },
	[0326] = { "1-", misc_one_minus },
	[0327] = { "RPLACA", misc_rplaca },
	[0331] = { "ZEROP", misc_zerop },
	[0335] = { "EQUAL", misc_equal },
	[0337] = { "XSTORE", misc_xstore },
	[0350] = { "LSH", misc_lsh },
	[0352] = { "*BOOLE", misc_boole },
	[0353] = { "NUMBERP", misc_numberp },
	[0355] = { "MINUSP", misc_minusp },
	[0361] = { "VALUE-CELL-LOCATION", misc_value_cell_location },
	[0362] = { "FUNCTION-CELL-LOCATION", misc_function_cell_location },
	[0366] = { "CONS", misc_cons },
	[0373] = { "SYMEVAL", misc_symeval },
	[0410] = { "MEMQ", misc_memq },
	[0412] = { ">", misc_greater },
	[0417] = { "NTH", misc_nth },
	[0420] = { "NTHCDR", misc_nthcdr },
	[0430] = { "ARRAY-LEADER", misc_array_leader },
	[0431] = { "STORE-ARRAY-LEADER", misc_store_array_leader },
	[0456] = { "%CATCH-OPEN", misc_catch_open },
	[0472] = { "%P-LDB", misc_p_ldb },
	[0473] = { "%P-DPB", misc_p_dpb },
	[0500] = { "COPY-ARRAY-CONTENTS", misc_copy_array_contents },
	[0505] = { "FIND-POSITION-IN-LIST", misc_find_position_in_list }, 
	[0515] = { "AS-1", misc_as_1 },
	[0520] = { "%INSTANCE-REF", misc_instance_ref },
	[0521] = { "%INSTANCE-LOC", misc_instance_loc },
	[0522] = { "%INSTANCE-SET", misc_instance_set },
	[0527] = { "%P-CONTENTS-OFFSET", misc_p_contents_offset },
	[0532] = { "%ARGS-INFO", misc_args_info },
	[0533] = { "%OPEN-CALL-BLOCK", misc_open_call_block },
	[0534] = { "%PUSH", misc_push },
	[0535] = { "%ACTIVATE-OPEN-CALL-BLOCK",misc_activate_open_call_block},
	[0536] = { "%ASSURE-PDL-ROOM", misc_assure_pdl_room },
	[0542] = { "STACK-GROUP-RESUME", misc_stack_group_resume },
	[0556] = { "%UNIBUS-WRITE", misc_unibus_write },
	[0571] = { "SYMBOLP", misc_symbolp },
	[0573] = { "ARRAYP", misc_arrayp },
	[0574] = { "FBOUNDP", misc_fboundp },
	[0575] = { "STRINGP", misc_stringp },
	[0576] = { "BOUNDP", misc_boundp },
	[0600] = { "FSYMEVAL", misc_fsymeval },
	[0605] = { "%P-LDB-OFFSET", misc_p_ldb_offset },
	[0624] = { "%24-BIT-PLUS", misc_24_bit_plus },
	[0632] = { "%P-CONTENTS-AS-LOCATIVE-OFFSET", misc_p_contents_as_locative_offset },
	[0633] = { "EQ", misc_eq },
	[0637] = { "%XBUS-READ", misc_xbus_read },
	[0640] = { "%XBUS-WRITE", misc_xbus_write },
};

void
MISC(void)
{
	struct misc_func *mp;

	// GET LOW 9 BITS OF INST
	A_B = ldb (M_INST_ADR, macro_inst);

	mp = &misc_funcs[A_B]; /* already masked to 9 bits */
	
	if (mp->func == NULL)
		ILLOP ("unimp MISC");
		
	(*mp->func)();

	if (dflags & DBG_MISC)
		print_q_dbg ("misc val", A_T);

	/* D-ND3 */
	switch (ldb (M_INST_DEST, macro_inst)) {
	case D_IGNORE: break;
	case D_PDL: MISC_TO_STACK (); break;
	case D_NEXT: MISC_TO_STACK (); break;
	case D_LAST: MISC_TO_LAST (); break;
	case D_RETURN:
		QMDDR (0);
		break;
	case D_MICRO: MISC_TO_LIST (); break;
	default: ILLOP ("MISC dest");
	}
}

void
QIAND (void)
{
	A_A = ARITH_2ARG_BOOLE;
	if (D_NUMARG1 (q_data_type (pdl_peek ())) == 0) {
		if (D_FIXNUM_NUMARG2 (q_data_type (A_T), NUMBER_CODE_FIXNUM) == 0) {
			A_T = make_pointer (dtp_fix, A_1 & A_2);
			pdl_push (A_T | (CDR_NIL<<30));
			return;
		}
	}
	ILLOP ("QIAND");
}

void
QIIOR (void)
{
	A_A = ARITH_2ARG_BOOLE;
	if (D_NUMARG1 (q_data_type (pdl_peek ())) == 0) {
		if (D_FIXNUM_NUMARG2 (q_data_type (A_T), NUMBER_CODE_FIXNUM) == 0) {
			A_T = make_pointer (dtp_fix, A_1 | A_2);
			pdl_push (A_T | (CDR_NIL<<30));
			return;
		}
	}
	ILLOP ("QIAND");
}

void
QIADD (void)
{
	A_A = ARITH_2ARG_ADD;
	if (D_NUMARG1 (q_data_type (pdl_peek ())) == 0) {
		if (D_FIXNUM_NUMARG2 (q_data_type (A_T), NUMBER_CODE_FIXNUM) == 0) {
			A_1 += A_2;
			FIXPACK_P ();
			return;
		}
	}
	ILLOP ("QIADD");
}

void
QISUB (void)
{
	A_A = ARITH_2ARG_SUB;
	if (D_NUMARG1 (q_data_type (pdl_peek ())) == 0) {
		if (D_FIXNUM_NUMARG2 (q_data_type (A_T),
				      NUMBER_CODE_FIXNUM) == 0) {
			A_1 -= A_2;
			FIXPACK_P ();
			return;
		}
	}
	ILLOP ("QIADD");
}

void
QIDIV (void)
{
	A_A = ARITH_2ARG_DIV;
	if (D_NUMARG1 (q_data_type (pdl_peek ())) == 0) {
		if (D_FIXNUM_NUMARG2 (q_data_type (A_T),
				      NUMBER_CODE_FIXNUM) == 0) {
			A_1 = (signed)A_1 / (signed)A_2;
			FIXPACK_P ();
			return;
		}
	}
	ILLOP ("QIDIV");
}

void
QIMUL (void)
{
	A_A = ARITH_2ARG_DIV;
	if (D_NUMARG1 (q_data_type (pdl_peek ())) == 0) {
		if (D_FIXNUM_NUMARG2 (q_data_type (A_T), NUMBER_CODE_FIXNUM) == 0) {
			long long val;
			val = (long long)A_1 * (long long)A_2;
			if (MOST_NEGATIVE_FIXNUM <= val && val <= MOST_POSITIVE_FIXNUM) {
				A_1 = val;
				FIXPACK_P ();
				return;
			}
			ILLOP ("QIMUL overflow");
		}
	}
	ILLOP ("QIMUL");
}

void
QIND1(void) 
{
	fetch_arg ();

	// D-ND1
	// note: these end by pushing M-T on the pld 
	switch (ldb (M_INST_DEST, macro_inst)) {
	case 0: ILLOP ("qind1");
	case 1:	QIADD (); break; 
	case 2: QISUB (); break; //(QISUB)
	case 3: QIMUL (); break;
	case 4: QIDIV (); break;
	case 5:	QIAND (); break;
	case 6: ILLOP ("qind1"); //(QIXOR)
	case 7: QIIOR (); break;
	}
}

void
QIEQL (void)
{
	/* compare top of pdl with A_T */
	A_A = ARITH_2ARG_EQUAL;
	if (D_NUMARG1 (q_data_type (pdl_peek ())) == 0) {
		D_FIXNUM_NUMARG2 (q_data_type (A_T), NUMBER_CODE_FIXNUM);
		if (dflags & DBG_QIEQL)
			printf ("(= %d %d)\n", A_1, A_2);
		if (A_1 == A_2)
			A_T = SYM_T;
		else
			A_T = SYM_NIL;
		return;
	}

	ILLOP ("QIEQL");
}

void
QIGRP (void)
{
	A_A = ARITH_2ARG_GREATERP;
	if (D_NUMARG1 (q_data_type (pdl_peek ())) == 0) {
		if (D_FIXNUM_NUMARG2 (q_data_type (A_T),
				      NUMBER_CODE_FIXNUM) == 0) {
			if ((int)A_1 > (int)A_2)
				A_T = SYM_T;
			else
				A_T = SYM_NIL;
			return;
		}
	}
	ILLOP ("QIGRP");
}

void
QILSP (void)
{
	A_A = ARITH_2ARG_GREATERP;
	if (D_NUMARG1 (q_data_type (pdl_peek ())) == 0) {
		if (D_FIXNUM_NUMARG2 (q_data_type (A_T),
				      NUMBER_CODE_FIXNUM) == 0) {
			if ((int)A_1 < (int)A_2)
				A_T = SYM_T;
			else
				A_T = SYM_NIL;
			return;
		}
	}
	ILLOP ("QILSP");
}

void
SETE_CDR (void)
{
	// QISCDR
	QCDR ();
	A_1 = ldb (M_INST_DELTA, macro_inst);
	QADCM2 ();
}

void
QISP1 (void)
{
	// SETE-1+
	pdl_push (A_T);
	X1PLS ();
	A_1 = ldb (M_INST_DELTA, macro_inst);
	QADCM2 ();
}

void
QISM1 (void)
{
	// SETE-1-
	pdl_push (A_T);
	X1MNS ();
	A_1 = ldb (M_INST_DELTA, macro_inst);
	QADCM2 ();
}

void
QIEQ (void)
{
	Q a, b;
	a = q_typed_pointer (pdl_pop ());
	b = q_typed_pointer (pdl_pop ());
	if (a == b)
		A_T = A_V_TRUE;
	else
		A_T = A_V_NIL;
}

void
QIND2(void) 
{
	fetch_arg ();

	// D-ND2 
	switch (ldb (M_INST_DEST, macro_inst)) {
	case 0: QIEQL (); break; // =
	case 1: QIGRP (); break; // > QIGRP
	case 2: QILSP (); break; // < QILSP
	case 3: QIEQ (); break; // EQ QIEQ
	case 4: SETE_CDR (); break; // SETE-CDR  QISCDR
	case 5: ILLOP("QIND2"); // SETE-CDDR QISCDDR
	case 6: QISP1 (); break; // SETE-1+  QISP1
	case 7: QISM1 (); break; // SETE-1-  QISM1
	}
}



// DISPATCH ON REGISTER FIELD FOR STORE CYCLE, VALUE IN M-T.
// USE THIS ONE IF A READ CYCLE HASN'T BEEN DONE YET
void
QADCM2 (void)
{
	// QADCM2
	switch (ldb (M_INST_REGISTER, macro_inst)) {
	case reg_fef100:
	case reg_fef200:
	case reg_fef300:
		/* full delta */
		// QSTFE
		A_1 = ldb (M_INST_ADR, macro_inst);
		/* fall in */
	case reg_fef: 
		// QSTFE1
		vmread_transport_write (LC_fef + A_1);
		// could check flag and call QSTFE-MONITOR
		vmwrite_gc_write_test (vma, (md & Q_CDR_CODE) | (A_T & Q_ALL_BUT_CDR_CODE));
		break;

	case reg_const: ILLOP ("write to constant");
	case reg_local: 
		// QSTLOC
		pdl_store (A_LOCALP + A_1, A_T);
		break;
	case reg_arg: 
		// QSTARG
		pdl_store (A_AP + A_1 + 1, A_T);
		break;
	case reg_pdl:
		ILLOP ("QADCM2 write to PDL");
	}
}

// returns location in A_B, cdr code and flag bits in M-E
void
QBND4 (Q addr)
{
	Q old_value;

	if (q_pointer (A_QLBNDP) + 23 >= q_pointer (A_QLBNDH))
		trap ("PDL-OVERFLOW SPECIAL");

	vmread_transport_no_evcp (addr);
	addr = vma;
	A_B = addr;
	A_E = q_all_but_typed_pointer (md) | make_pointer (dtp_locative, 0);
	old_value = q_typed_pointer (md);

	if (ldb (M_FLAGS_QBBFL, A_FLAGS) == 0) {
		old_value |= Q_FLAG_BIT;
		A_FLAGS = dpb (1, M_FLAGS_QBBFL, A_FLAGS);
	}

	A_QLBNDP++;
	vmwrite_gc_write_test (A_QLBNDP, old_value);
	A_QLBNDP++;
	vmwrite_gc_write_test (A_QLBNDP, addr);
}

void
QIBND_combined (Q pop_flag, Q new_val)
{
	Q old_value;
	Q vcell;
	Q cell_bits;

	// QIBNDP
	
	// QIBND, QBND1  SAVE PRESENT BINDING

	// SAVE CURRENT CONTENTS, DON'T CHANGE
	// LEAVE M-E SET TO OLD CONTENTS (MAINLY FOR CDR CODE)

	A_1 = ldb (M_INST_DELTA, macro_inst);

	// dispatch QADCM6 
	// DISPATCH FOR GETTING EFFECTIVE ADDRESS FOR BIND to pdl.
	// THIS HAS TO HAVE A SPECIAL KLUDGE FOR DOING EXACTLY ONE LEVEL
	// OF INDIRECTION ON DATA IN THE FEF.
	switch (ldb (M_INST_REGISTER, macro_inst)) {
	case reg_fef:
	case reg_fef100:
	case reg_fef200:
	case reg_fef300: 
		// SPECIAL KLUDGEY ADDRESS ROUTINE FOR BIND.  ALWAYS INDIRECTS ONE LEVEL.
		// RETURNS WITH ADDRESS ON PDL.
		// QBAFE
		A_1 = ldb (M_INST_ADR, macro_inst); // FULL DELTA
		vmread_transport_no_evcp (pdl_ref (A_AP) + A_1); // ONLY TRANSPORT, DON'T DO INVZ

		// MAKE SURE IT WAS AN EVCP AND RETURN LOCATIVE ON PDL
		if (q_data_type (md) != dtp_external_value_cell_pointer)
			ILLOP ("QBAFE");
		pdl_push (make_pointer (dtp_locative, md));
		break;
		
	case reg_local:
		// QVMALCL
		pdl_push (convert_pdl_buffer_address (A_LOCALP + A_1) | (CDR_NEXT<<30));
		break;
	case reg_arg: 
		// QVMAARG
		pdl_push (convert_pdl_buffer_address (A_AP + 1 + A_1) | (CDR_NEXT<<30));
		break;

	case reg_const:
	case reg_pdl:
		ILLOP ("QBAFE reg");
	}

	// QBND2

	vcell = pdl_pop ();

	// M-B has location being bound.  MD gets current contents.
	// Will return with old-value saved and Q-ALL-BUT-TYPED-POINTER in M-E,
	// VMA and M-B updated to actual location bound (different if there is a ONE-Q-FORWARD).

	// QBND4
	if (q_pointer (A_QLBNDP) + 23 >= q_pointer (A_QLBNDH))
		trap ("PDL-OVERFLOW SPECIAL");

	vmread_transport_no_evcp (vcell);
	vcell = vma;
	old_value = q_typed_pointer (md);
	cell_bits = q_all_but_typed_pointer (md);

	A_QLBNDP++;
	if (ldb (M_FLAGS_QBBFL, A_FLAGS) == 0) {
		old_value |= Q_FLAG_BIT;
		A_FLAGS = dpb (1, M_FLAGS_QBBFL, A_FLAGS);
	}

	// QBND3
	// this gc_write_test had better not do anything,
	// since special pdl is on an odd word here

	vmwrite_gc_write_test (A_QLBNDP, old_value);
	A_QLBNDP++;
	vmwrite_gc_write_test (A_QLBNDP, vcell);

	// rest of QIBNDP (pop) QIBDN1

	if (pop_flag != A_V_NIL) {
		A_T = (pdl_pop () & Q_TYPED_POINTER) | cell_bits;
	} else {
		A_T = new_val;
	}

	vmwrite_gc_write_test (vcell, A_T);
}

void
QIND3(void) 
{
	/* D-ND3 */
	switch (ldb (M_INST_DEST, macro_inst)) {
	case 0: ILLOP ("QIBND");
	case 1:
		QIBND_combined (A_V_NIL, A_V_NIL);
		break;
	case 2:
		QIBND_combined (A_V_TRUE, A_V_NIL);
		break;
	case 3: 
		// QISETN
		A_T = SYM_NIL;
		A_1 = ldb (M_INST_DELTA, macro_inst);
		QADCM2 ();
		break;

	case 4: 
		// QISETZ
		A_T = FIX_ZERO;
		A_1 = ldb (M_INST_DELTA, macro_inst);
		QADCM2 ();
		break;

	case 5: ILLOP ("QIPSHE");
	case 6: // QIMVM  MOVEM
		A_T = pdl_peek ();
		A_1 = ldb (M_INST_DELTA, macro_inst);
		QADCM2 ();
		break;
	case 7: 
		// QIPOP
		A_T = pdl_pop ();
		A_1 = ldb (M_INST_DELTA, macro_inst);
		QADCM2 ();
		break;
	}
}

void
fetch_macro_inst (void)
{
	macro_inst = vmread (LC_fef + (LC_pc / 2));
	if (LC_pc & 1)
		macro_inst >>= 16;
	macro_inst &= 0xffff;
	LC_pc++;
}	

void
QMLP (void)
{
	char buf[100];

	/* XXX test for sequence break */

	macro_count++;

	if (macro_count >= 33400) {
		dflags = -1;
	}

	if (dflags & DBG_QMLP) {
		disassemble (buf, LC_fef, LC_pc);
		printf ("#%d: %s [%d]\n", macro_count, buf, pdl_top);
	}

	fetch_macro_inst ();

	switch (ldb (M_INST_OP, macro_inst)) {
	case 0: QICALL(); break; /* CALL */
	case 1: QICAL0(); break; /* CALL0 */
	case 2: QIMOVE(); break; /* MOVE */
	case 3: QICAR(); break; /* CAR */
	case 4: QICDR(); break; /* CDR */
	case 5: QICADR(); break; /* CADR */
	case 6: QICDDR(); break; /* CDDR */
	case 7: QICDAR(); break; /* CDAR */
	case 010: QICAAR(); break; /* CAAR */
	case 011: QIND1(); break; /* ND1 */
	case 012: QIND2(); break; /* ND2 */
	case 013: QIND3(); break; /* ND3 */
	case 014: QIBRN(); break; /* BRANCH */
	case 015: MISC(); break; /* MISC */
	case 016: qi_unimp (); break; /* unused */
	case 017: qi_unimp (); break; /* unused */
	}
}


void
process_mcr (void)
{
	int idx;
	int type, start, size;
	int nblocks, offset, addr;

	idx = 0;

	while (1) {
		type = mcr[idx++];
		switch (type) {
		case 1:
			start = mcr[idx++];
			size = mcr[idx++];
			printf ("imem start %#o; size %d (%#o)\n",
				start, size, size);
			
			idx += size * 2;
			break;
		case 2: 
			start = mcr[idx++];
			size = mcr[idx++];
			printf ("dmem start %#o; size %#o\n",
				start, size);
			idx += size;
			break;
		case 3:
			nblocks = mcr[idx++];
			offset = mcr[idx++];
			addr = mcr[idx++];
			printf ("main mem nblocks %d; offset %d; addr %#o\n",
				nblocks, offset, addr);
			/*
			 * micro-code-symbol-area, 2 pages for area 3 
			 * these are the starting addresses for the
			 * misc instructions
			 */
			memcpy (mcr_micro_code_symbol_area,
				mcr + offset * 256,
				sizeof mcr_micro_code_symbol_area);
			break;
		case 4:
			start = mcr[idx++];
			size = mcr[idx++];
			printf ("amem start %#o; size %#o\n",
				start, size);
			if (size != AMEM_SIZE) {
				printf ("bad amem size in mcr\n");
				exit (1);
			}
			memcpy (mcr_amem, mcr + idx, size * 4);
			idx += size;
			break;
		case 0:
			goto done;

		default:
			printf ("unknown type %d\n", type);
			exit (1);
		}
	}

done:;
}

int
main (int argc, char **argv)
{
	int c;
	FILE *f;
	char *name;

	setbuf (stdout, NULL);

	while ((c = getopt (argc, argv, "")) != EOF) {
		switch (c) {
		default:
			usage ();
		}
	}
	
	name = "disk.img";
	if ((f = fopen (name, "r")) == NULL) {
		fprintf (stderr,
			 "can't open disk image %s (from usim project)\n",
			 name);
		exit (1);
	}

	fseek (f, 17 * 1024, SEEK_SET);
	fread (mcr, 1, sizeof mcr, f);
	
	process_mcr ();

	fseek (f, 33108 * 1024, SEEK_SET);
	fread (vm, 1, 25344 * 1024, f);

	memcpy (vm + AMEM_ADDR, mcr_amem, sizeof mcr_amem);

	beg0000 ();

	print_q_verbose (LC_fef);
	printf ("\n");

	while (1)
		QMLP ();

	return (0);
}

/* ================================================================ */

int disassemble_fetch (Q fef, int pc);
void disassemble_address (char *buf, Q fef, int reg, int disp);

int
disassemble (char *buf, Q fef, int pc)
{
	char *destnames[]
		= { "D-IGNORE","D-PDL","D-NEXT","D-LAST",
		    "D-RETURN","D-NEXT-Q","D-LAST-Q","D-NEXT-LIST" };
	int inst, op, dest, disp, reg;
	char *p;

	inst = disassemble_fetch (fef, pc);

	op = get_field (inst, 4, 011);
	dest = get_field (inst, 3, 015);
	disp = get_field (inst, 011, 0);
	reg = get_field (inst, 3, 6);

	p = buf;
	sprintf (p, "%#o ", pc);
	p += strlen (p);

	if (inst == 0) {
		sprintf (buf, "0");
	} else if (op < 011) {
		char *opnames[]
			= { "CALL","CALL0","MOVE","CAR","CDR","CADR","CDDR","CDAR","CAAR" };
		sprintf (p, "%s %s", opnames[op], destnames[dest]);
		disassemble_address (buf, fef, reg, disp);
	} else if (op == 011) {
		char *nd1_names[] = {"ND1-UNUSED","+","-","*","/","LOGAND","LOGXOR","LOGIOR"};
		sprintf (p, "%s", nd1_names[dest]);
		disassemble_address (buf, fef, reg, disp);
	} else if (op == 012) {
		char *nd2_names[]
			= {"=",">","<","EQ","SETE-CDR","SETE-CDDR","SETE-1+","SETE-1-"};
		sprintf (p, "%s", nd2_names[dest]);
		disassemble_address (buf, fef, reg, disp);
	} else if (op == 013) {
		char *nd3_names[] = {"BIND-OBSOLETE?","BIND-NIL","BIND-POP","SET-NIL",
				     "SET-ZERO","PUSH-E","MOVEM","POP"};
		sprintf (p, "%s", nd3_names[dest]);
		disassemble_address (buf, fef, reg, disp);
	} else if (op == 014) {
		char *br_names[] = {"BR","BR-NIL","BR-NOT-NIL","BR-NIL-POP",
				    "BR-NOT-NIL-POP","BR-ATOM","BR-NOT-ATOM","BR-ILL-7"};
		if (disp > 0400) /* XXX bug? should it be >= ? */
			disp |= -0400; /* sign extend */
		if (disp != -1) {
			sprintf (p, "%s %#o", br_names[dest], pc + disp + 1);
		} else {
			/* long branch */
			pc++;
			disp = disassemble_fetch (fef, pc);
			if (disp > 0100000)
				disp |= -0100000; /* sign extend */
			sprintf (p, "%s* %#o", br_names[dest], pc + disp + 1);
			return (2);
		}
	} else if (op == 015) {
		sprintf (p, "(MISC) ");
		p += strlen (p);

		if (disp < 0100) {
			sprintf (p, "LIST %d long", disp);
		} else if (disp < 0200) {
			sprintf (p, "LIST-IN-AREA %d long", disp - 0100);
		} else if (disp < 0220) {
			int x = disp - 0177; /* code 200 does 1 unbind */
			sprintf (p, "UNBIND %d binding%s", x, x == 1 ? "" : "s");
			if (dest == 0)
				return (1);
		} else if (disp < 0240) {
			int x = disp - 0220; /* code 220 does 0 pops */
			sprintf (p, "POP-PDL %d time%s", x, x == 1 ? "" : "s");
			if (dest == 0)
				return (1);
		} else {
			/* later, look up misc_num in #'MICRO-CODE-SYMBOL-NAME-AREA */
			if (disp < 01000 && misc_funcs[disp].name != NULL)
				sprintf (p, "%s", misc_funcs[disp].name);
			else
				sprintf (p, "#%#o", disp);
		}
		p += strlen (p);
		sprintf (p, " %s", destnames[dest]);
	} else {
		sprintf (p, "UNDEF-%#o", op);
	}
	return (1);
}

void
disassemble_address (char *buf, Q fef, int reg, int disp)
{
	char *p;

	p = buf + strlen (buf);
	*p++ = ' ';
	if (reg < 4) {
		sprintf (p, "FEF|%d", disp);
	} else if (reg == 4) {
		sprintf (p, "CONST|%#o", disp & 077);
	} else if (disp == 0777) {
		sprintf (p, "PDL-POP");
	} else if (reg == 5) {
		sprintf (p, "LOCAL|%d", disp & 077);
	} else if (reg == 6) {
		sprintf (p, "ARG|%d", disp & 077);
	} else {
		sprintf (p, "PDL|-%d", disp & 077);
	}
}

// Given a FEF and a PC, returns the corresponding 16-bit macro instruction.
// There is no error checking.
int
disassemble_fetch (Q fef, int pc)
{
	unsigned int val;

	val = vmread (fef + pc/2);
	if ((pc & 1) == 0)
		return (val & 0xffff);
	return ((val >> 16) & 0xffff);
}

#include <termios.h>
#include <signal.h>

struct termios old_termios;
int raw_mode;

	

void
cc_cooked (void)
{
	if (raw_mode == 0)
		return;
	tcsetattr (0, TCSANOW, &old_termios);
	raw_mode = 0;
}

void
cc_raw (void)
{
	struct termios t;

	if (raw_mode)
		return;
	
	tcgetattr (0, &old_termios);
	t = old_termios;
	cfmakeraw (&t);
	t.c_lflag |= ISIG;

	tcsetattr (0, TCSANOW, &t);
	raw_mode = 1;
}

void
cc_exit (void)
{
	cc_cooked ();
	printf ("\n");
	exit (0);
}

void
intr (int sig)
{
	cc_exit ();
}

Q
cc_read (unsigned int addr)
{
	if (addr < VM_WORDS)
		return (vm[addr]);
	
	printf (" *bad-addr* ");
	return (0);
}

void
cc (void)
{
	unsigned int addr;
	char ch;
	int c;
	Q val;
	unsigned int acc;
	int acc_active;

	fflush (stdout);
	setbuf (stdout, NULL);

	signal (SIGINT, intr);
	signal (SIGHUP, intr);
	signal (SIGTERM, intr);
	cc_raw ();

	printf ("\r\nCC\r\n* ");

	addr = 0;
	acc = 0;
	acc_active = 0;

	while (1) {
		if (read (0, &ch, 1) != 1)
			cc_exit ();
		c = ch & 0x7f;
		switch (c) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			putchar (c);
			if (acc_active == 0) {
				acc_active = 1;
				acc = 0;
			}
			acc = (acc * 8) + (c - '0');
			break;

		case '/':
		display:
			if (acc_active) {
				acc_active = 0;
				addr = acc;
			}
			val = cc_read (addr);
			printf ("\r\n%#o/   %#o  ", addr, val);
			break;

		case '\r': case '\n':
			addr++;
			goto display;

		case '^':
			if (addr > 0)
				addr--;
			goto display;

		case '\t':
			addr = q_pointer (cc_read (addr));
			goto display;
			
		case 'q':
			print_q (val);
			printf  ("  ");
			break;

		case '.':
			printf ("\r\n");
			break;

		default:
			printf ("*\r\n");
			break;
		}
	}
}

void
niceput (int c)
{
	if (c & 0x80) {
		printf ("M-");
		c &= 0x7f;
	}
	if (c < ' ')
		printf ("^%c", c + '@');
	else if (c == 0177)
		printf ("^?");
	else
		putchar (c);
}

void
print_q_verbose (Q q)
{
	Q name;
	Q save_vma, save_md;

	save_vma = vma;
	save_md = md;

	switch (q_data_type (q)) {
	case dtp_symbol:
		name = make_pointer (dtp_array_pointer, vmread (q));
		printf ("#<symbol ");
		print_q_verbose (name);
		printf (">");
		break;

	case dtp_array_pointer: {
		Q hdr = vmread (q);
		int type;
		int i;
		char *base;

		type = ldb (ARRAY_TYPE_FIELD, hdr);
		if (type == 011) { /* art-string */
			int nbytes = ldb (ARRAY_INDEX_LENGTH_IF_SHORT, hdr);
			base = (char *)(vm + q_pointer (q) + 1);
			printf ("\"");
			for (i = 0; i < nbytes; i++)
				niceput (base[i]);
			printf ("\"");
		} else {
			print_q (q);
		}
		break;
	}

	case dtp_fix:
		printf ("%#o", q_pointer (q));
		break;

	case dtp_fef_pointer:
		printf ("#<%s ", data_type_numbers_name (q_data_type (q)));
		name = vmread (q + 2);
		print_q_verbose (name);
		printf (" %#o>", q_pointer (q));
		break;
	default:
		print_q (q);
		break;
	}

	vma = save_vma;
	md = save_md;
}

void
gdb_print_q (Q q)
{
	Q vma_save, md_save;
	vma_save = vma;
	md_save = md;
	print_q (q);
	fflush (stdout);
	vma = vma_save;
	md = md_save;
}

void
gdb_print_q_verbose (Q q)
{
	Q vma_save, md_save;
	vma_save = vma;
	md_save = md;
	print_q_verbose (q);
	fflush (stdout);
	vma = vma_save;
	md = md_save;
}

