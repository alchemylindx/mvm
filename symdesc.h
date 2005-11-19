#ifndef _SYMDESC_H_
#define _SYMDESC_H_

struct sym data_type_numbers_desc[] = {
	{ "dtp-trap", 0 },
	{ "dtp-null", 1 },
	{ "dtp-free", 2 },
	{ "dtp-symbol", 3 },
	{ "dtp-symbol-header", 4 },
	{ "dtp-fix", 5 },
	{ "dtp-extended-number", 6 },
	{ "dtp-header", 7 },
	{ "dtp-gc-forward", 010 },
	{ "dtp-external-value-cell-pointer", 011 },
	{ "dtp-one-q-forward", 012 },
	{ "dtp-header-forward", 013 },
	{ "dtp-body-forward", 014 },
	{ "dtp-locative", 015 },
	{ "dtp-list", 016 },
	{ "dtp-u-code-entry", 017 },
	{ "dtp-fef-pointer", 020 },
	{ "dtp-array-pointer", 021 },
	{ "dtp-array-header", 022 },
	{ "dtp-stack-group", 023 },
	{ "dtp-closure", 024 },
	{ "dtp-small-flonum", 025 },
	{ "dtp-select-method", 026 },
	{ "dtp-instance", 027 },
	{ "dtp-instance-header", 030 },
	{ "dtp-entity", 031 },
	{ "dtp-stack-closure", 032 },
};

struct sym sys_com_desc[] = {
	{ "SYS-COM-AREA-ORIGIN-PNTR", 0 },
	{ "SYS-COM-VALID-SIZE", 1 },
	{ "SYS-COM-PAGE-TABLE-PNTR", 2 },
	{ "SYS-COM-PAGE-TABLE-SIZE", 3 },
	{ "SYS-COM-OBARRAY-PNTR", 4 },
	{ "SYS-COM-REMOTE-KEYBOARD", 5 },
	{ "SYS-COM-MICRO-LOAD-M-DATA", 6 },
	{ "SYS-COM-MICRO-LOAD-A-DATA", 7 },
	{ "SYS-COM-MICRO-LOAD-ADDRESS", 010 },
	{ "SYS-COM-MICRO-LOAD-FLAG", 011 },
	{ "SYS-COM-UNIBUS-INTERRUPT-LIST", 012 },
	{ "SYS-COM-TEMPORARY", 013 },
	{ "SYS-COM-FREE-AREA-NUM-LIST", 014 },
	{ "SYS-COM-FREE-REGION-NUM-LIST", 015 },
	{ "SYS-COM-MEMORY-SIZE", 016 },
	{ "SYS-COM-WIRED-SIZE", 017 },
	{ "SYS-COM-CHAOS-FREE-LIST", 020 },
	{ "SYS-COM-CHAOS-TRANSMIT-LIST", 021 },
	{ "SYS-COM-CHAOS-RECEIVE-LIST", 022 },
	{ "SYS-COM-DEBUGGER-REQUESTS", 023 },
	{ "SYS-COM-DEBUGGER-KEEP-ALIVE", 024 },
	{ "SYS-COM-DEBUGGER-DATA-1", 025 },
	{ "SYS-COM-DEBUGGER-DATA-2", 026 },
        { "SYS-COM-MAJOR-VERSION", 027 },
};

struct sym array_header_fields_desc[] = {
	{ "ARRAY-INDEX-LENGTH-IF-SHORT", BIT_FIELD (012, 0) },
	{ "ARRAY-NAMED-STRUCTURE-FLAG",  BIT_FIELD (1, 012) },
	{ "ARRAY-LONG-LENGTH-FLAG", BIT_FIELD (1, 013) },
	{ "ARRAY-NUMBER-OF-DIMENSIONS", BIT_FIELD (3, 014) },
	{ "ARRAY-FLAG", BIT_FIELD (1, 017) },
	{ "ARRAY-DISPLACED-BIT", BIT_FIELD (1, 020) },
	{ "ARRAY-LEADER-BIT", BIT_FIELD (1, 021) },
	{ "ARRAY-TYPE-FIELD", BIT_FIELD (5, 023) },
};

struct sym stack_group_qs_desc[] = {
	{ "SG-NAME", 0 },
	{ "SG-REGULAR-PDL", 1 },
	{ "SG-REGULAR-PDL-LIMIT", 2 },
	{ "SG-SPECIAL-PDL", 3 },
	{ "SG-SPECIAL-PDL-LIMIT", 4 },
	{ "SG-INITIAL-FUNCTION-INDEX", 5 },
	{ "SG-UCODE", 6 },
	{ "SG-TRAP-TAG", 7 },
	{ "SG-RECOVERY-HISTORY", 010 },
	{ "SG-FOOTHOLD-DATA", 011 },
	{ "SG-STATE", 012 },

	/* in sg-state */
	{ "SG-ST-CURRENT-STATE", BIT_FIELD (6, 0) },
	{ "SG-ST-FOOTHOLD-EXECUTING-FLAG", BIT_FIELD (1, 6) },
	{ "SG-ST-PROCESSING-ERROR-FLAG", BIT_FIELD (1, 7) },
	{ "SG-ST-PROCESSING-INTERRUPT-FLAG", BIT_FIELD (1, 010) },
	{ "SG-ST-SAFE", BIT_FIELD (1, 011) },
	{ "SG-ST-INST-DISP", BIT_FIELD (2, 012) },
	{ "SG-ST-IN-SWAPPED-STATE", BIT_FIELD (1, 026) },
	{ "SG-ST-SWAP-SV-ON-CALL-OUT", BIT_FIELD (1, 025) },
	{ "SG-ST-SWAP-SV-OF-SG-THAT-CALLS-ME", BIT_FIELD (1, 024) },

	{ "SG-PREVIOUS-STACK-GROUP", 013 },
	{ "SG-CALLING-ARGS-POINTER", 014 },
	{ "SG-CALLING-ARGS-NUMBER", 015 }, 
	{ "SG-TRAP-AP-LEVEL", 016 },
	{ "SG-REGULAR-PDL-POINTER", 017 },
	{ "SG-SPECIAL-PDL-POINTER", 020 },
	{ "SG-AP", 021 },
	{ "SG-IPMARK", 022 },
	{ "SG-TRAP-MICRO-PC", 023 },
	{ "SG-SAVED-QLARYH", 024 },
	{ "SG-SAVED-QLARYL", 025 },

	{ "SG-SAVED-M-FLAGS", 026 },
	{ "SG-FLAGS-QBBFL", BIT_FIELD (1, 0) },

	{ "SG-FLAGS-CAR-SYM-MODE", BIT_FIELD (2, 1) },
	{ "SG-FLAGS-CAR-NUM-MODE", BIT_FIELD (2, 3) },
	{ "SG-FLAGS-CDR-SYM-MODE", BIT_FIELD (2, 5) },
	{ "SG-FLAGS-CDR-NUM-MODE", BIT_FIELD (2, 7) },

	{ "SG-FLAGS-DONT-SWAP-IN", BIT_FIELD (1, 011) },
	{ "SG-FLAGS-TRAP-ENABLE", BIT_FIELD (1, 012) },
	{ "SG-FLAGS-MAR-MODE", BIT_FIELD (2, 013) },
	{ "SG-FLAGS-PGF-WRITE", BIT_FIELD (1, 015) },

	{ "SG-AC-K", 027 },
	{ "SG-AC-S", 030 },
	{ "SG-AC-J", 031 },
	{ "SG-AC-I", 032 },
	{ "SG-AC-Q", 033 },
	{ "SG-AC-R", 034 },
	{ "SG-AC-T", 035 },
	{ "SG-AC-E", 036 },
	{ "SG-AC-D", 037 },
	{ "SG-AC-C", 040 },
	{ "SG-AC-B", 041 },
	{ "SG-AC-A", 042 },
	{ "SG-AC-ZR", 043 },
	{ "SG-AC-2", 044 },
	{ "SG-AC-1", 045 },
	{ "SG-VMA-M1-M2-TAGS", 046 },
	{ "SG-SAVED-VMA", 047 },
	{ "SG-PDL-PHASE", 050 },
};

struct sym fefh_constant_values_desc[] = {
	{ "FEFH-NO-ADL", BIT_FIELD (1, 022) },
	{ "FEFH-FAST-ARG", BIT_FIELD (1, 021) },
	{ "FEFH-SV-BIND", BIT_FIELD (1, 020) },
	{ "FEFH-PC", BIT_FIELD (020, 0) },
	{ "FEFH-PC-IN-WORDS", BIT_FIELD (017, 1) },
};

struct sym m_inst_buffer_desc[] = {
	{ "M-INST-DEST", BIT_FIELD (3, 015) },
	{ "M-INST-OP", BIT_FIELD (4, 011) },
	{ "M-INST-ADR", BIT_FIELD (011, 0) },
	{ "M-INST-REGISTER", BIT_FIELD (3, 6) },
	{ "M-INST-DELTA", BIT_FIELD (6, 0) },
};

struct sym linear_pdl_call_state_desc[] = {
	// LPCLS (%LP-CALL-STATE).  Stored when this call frame is created.
	{ "LP-CLS-DOWNWARD-CLOSURE-PUSHED", BIT_FIELD (1, 025) },
	{ "LP-CLS-ADI-PRESENT", BIT_FIELD (1, 024) },
	{ "LP-CLS-DESTINATION", BIT_FIELD (4, 020) },
	{ "LP-CLS-DELTA-TO-OPEN-BLOCK", BIT_FIELD (010, 010) },
	{ "LP-CLS-DELTA-TO-ACTIVE-BLOCK", BIT_FIELD (010, 0) },
};

struct sym linear_pdl_exit_state_desc[] = {
	// LPEXS (%LP-EXIT-STATE).  Stored when this frame calls out.
	{ "LP-EXS-MICRO-STACK-SAVED", BIT_FIELD (1, 021) },
	{ "LP-EXS-PC-STATUS", BIT_FIELD (1, 020) },
	{ "LP-EXS-BINDING-BLOCK-PUSHED", BIT_FIELD (1, 020) },
	{ "LP-EXS-EXIT-PC", BIT_FIELD (017, 0) },
};

struct sym linear_pdl_entry_state_desc[] = {
	// LPENS (%LP-ENTRY-STATE).  Stored when this frame entered.
	{ "LP-ENS-NUM-ARGS-SUPPLIED", BIT_FIELD (6, 010) },
	{ "LP-ENS-MACRO-LOCAL-BLOCK-ORIGIN", BIT_FIELD (010, 0) },
};

struct sym dests_desc[] = {
	{ "D-IGNORE", 0 },
	{ "D-PDL", 1 },
	{ "D-NEXT", 2 },
	{ "D-LAST", 3 },
	{ "D-RETURN", 4 },
	/* 5, 6 obsolete */
	{ "D-NEXT-LIST", 7 },
	{ "D-MICRO", 010 },
};

struct sym cdr_codes_desc[] = {
	{ "CDR-NORMAL", 0 },
	{ "CDR-ERROR", 1 },
	{ "CDR-NIL", 2 },
	{ "CDR-NEXT", 3 },
};

struct sym areas_desc[] = {
	{ "RESIDENT-SYMBOL-AREA", 0 },
	{ "SYSTEM-COMMUNICATION-AREA", 1 },
	{ "SCRATCH-PAD-INIT-AREA", 2 },
	{ "MICRO-CODE-SYMBOL-AREA", 3 },
	{ "PAGE-TABLE-AREA", 4 },
	{ "PHYSICAL-PAGE-DATA", 5 },
	{ "REGION-ORIGIN", 6 },
	{ "REGION-LENGTH", 7 },
	{ "REGION-BITS", 010 },
	{ "REGION-SORTED-BY-ORIGIN", 011 },
	// END WIRED AREAS

	{ "REGION-FREE-POINTER", 012 },
	{ "REGION-GC-POINTER", 013 },
	{ "REGION-LIST-THREAD", 014 },
	{ "AREA-NAME", 015 },
	{ "AREA-REGION-LIST", 016 },
	{ "AREA-REGION-SIZE", 017 },
	{ "AREA-MAXIMUM-SIZE", 020 },
	{ "AREA-SWAP-RECOMMENDATIONS", 021 },
	{ "GC-TABLE-AREA", 022 },
	{ "SUPPORT-ENTRY-VECTOR", 023 },
	{ "CONSTANTS-AREA", 024 },
	{ "EXTRA-PDL-AREA", 025 },
	{ "MICRO-CODE-ENTRY-AREA", 026 },
	{ "MICRO-CODE-ENTRY-NAME-AREA", 027 },
	{ "MICRO-CODE-ENTRY-ARGS-INFO-AREA", 030 },
	{ "MICRO-CODE-ENTRY-MAX-PDL-USAGE", 031 },
	{ "MICRO-CODE-ENTRY-ARGLIST-AREA", 032 },
	{ "MICRO-CODE-SYMBOL-NAME-AREA", 033 },
	{ "LINEAR-PDL-AREA", 034 },
	{ "LINEAR-BIND-PDL-AREA", 035 },
	{ "INIT-LIST-AREA", 036 },
};

struct sym m_flags_desc[] = {
	{ "M-FLAGS-QBBFL", BIT_FIELD (1, 0) },
	{ "M-FLAGS-CAR-SYM-MODE", BIT_FIELD (2, 1) },
	{ "M-FLAGS-CAR-NUM-MODE", BIT_FIELD (2, 3) },
	{ "M-FLAGS-CDR-SYM-MODE", BIT_FIELD (2, 5) },
	{ "M-FLAGS-CDR-NUM-MODE", BIT_FIELD (2, 7) },
	{ "M-FLAGS-DONT-SWAP-IN", BIT_FIELD (1, 011) },
	{ "M-FLAGS-TRAP-ENABLE", BIT_FIELD (1, 012) },
	{ "M-FLAGS-MAR-MODE", BIT_FIELD (1, 013) },
	{ "M-FLAGS-PGF-WRITE", BIT_FIELD (1, 015) },
	{ "M-FLAGS-INTERRUPT", BIT_FIELD (1, 016) },
	{ "M-FLAGS-SCAVENGE", BIT_FIELD (1, 017) },
	{ "M-FLAGS-TRANSPORT", BIT_FIELD (1, 020) },
	{ "M-FLAGS-STACK-GROUP-SWITCH", BIT_FIELD (1, 021) },
	{ "M-FLAGS-DEFERRED-SEQUENCE-BREAK", BIT_FIELD (1, 022) },
	{ "M-FLAGS-METER-STACK-GROUP-ENABLE", BIT_FIELD (1, 023) },
	{ "M-FLAGS-TRAP-ON-CALLS", BIT_FIELD (1, 024) },
};

struct sym header_types_desc[] = {
	{ "HEADER-TYPE-ERROR", 0 },
	{ "HEADER-TYPE-FEF", 1 },
	{ "HEADER-TYPE-ARRAY-LEADER", 2 },
	{ "HEADER-TYPE-unused", 3 },
	{ "HEADER-TYPE-FLONUM", 4 },
	{ "HEADER-TYPE-COMPLEX", 5 },
	{ "HEADER-TYPE-BIGNUM", 6 },
	{ "HEADER-TYPE-RATIONAL-BIGNUM", 7 },
};

struct sym fefhi_desc[] = {
	{ "FEFHI-IPC", 0 },
	{ "FEFHI-STORAGE-LENGTH", 1 },
	{ "FEFHI-FCTN-NAME", 2 },
	{ "FEFHI-FAST-ARG-OPT", 3 },
	{ "FEFHI-SV-BITMAP", 4 },
	{ "FEFHI-MISC", 5 },
	{ "FEFHI-SPECIAL-VALUE-CELL-PNTRS", 6 },
};

struct sym fefhi2_desc[] = {
	{ "FEFHI-FSO-MAX-ARGS", BIT_FIELD (6, 0) },
	{ "FEFHI-FSO-MIN-ARGS", BIT_FIELD (6, 6) },

	{ "FEFHI-MS-LOCAL-BLOCK-LENGTH", BIT_FIELD (7, 0) },
	{ "FEFHI-MS-ARG-DESC-ORG", BIT_FIELD (010, 7) },
	{ "FEFHI-MS-BIND-DESC-LENGTH",  BIT_FIELD (010, 017) },
	{ "FEFHI-MS-DEBUG-INFO-PRESENT", BIT_FIELD (1, 027) },
	{ "FEFHI-SVM-ACTIVE", BIT_FIELD (1, 026) },
	{ "FEFHI-SVM-BITS", BIT_FIELD (026, 0) },
	{ "FEFHI-SVM-HIGH-BIT", BIT_FIELD (1, 025) },
};


struct sym error_substatus_desc[] = {
	{ "M-ESUBS-TOO-FEW-ARGS", BIT_FIELD (1, 0) },
	{ "M-ESUBS-TOO-MANY-ARGS", BIT_FIELD (1, 1) },
	{ "M-ESUBS-BAD-QUOTED-ARG", BIT_FIELD (1, 2) },
	{ "M-ESUBS-BAD-EVALED-ARG", BIT_FIELD (1, 3) },
	{ "M-ESUBS-BAD-DT", BIT_FIELD (1, 4) },
	{ "M-ESUBS-BAD-QUOTE-STATUS", BIT_FIELD (1, 5) },
};

struct sym fef_specialness_desc[] = {
	{ "FEF-LOCAL", 0 },
	{ "FEF-SPECIAL", 1 },
	{ "FEF-SPECIALNESS-UNUSED", 2 },
	{ "FEF-REMOTE", 3 },
};

struct sym fef_des_dt_desc[] = {
	{ "FEF-DT-DONTCARE", 0 },
	{ "FEF-DT-NUMBER", 1 },
	{ "FEF-DT-FIXNUM", 2 },
	{ "FEF-DT-SYM", 3 },
	{ "FEF-DT-ATOM", 4 },
	{ "FEF-DT-LIST", 5 },
	{ "FEF-DT-FRAME", 6 },
};

struct sym fef_quote_status_desc[] = {
	{ "FEF-QT-DONTCARE", 0 },
	{ "FEF-QT-EVAL", 1 },
	{ "FEF-QT-QT", 2 },
};

struct sym fef_arg_syntax_desc[] = {
	{ "FEF-ARG-REQ", 0 },
	{ "FEF-ARG-OPT", 1 },
	{ "FEF-ARG-REST", 2 },
	{ "FEF-ARG-AUX", 3 },
	{ "FEF-ARG-FREE", 4 },
	{ "FEF-ARG-INTERNAL", 5 },
	{ "FEF-ARG-INTERNAL-AUX", 6 },
};


struct sym fef_init_option_desc[] = {
	{ "FEF-INI-NONE", 0 },
	{ "FEF-INI-NIL", 1 },
	{ "FEF-INI-PNTR", 2 },
	{ "FEF-INI-C-PNTR", 3 },
	{ "FEF-INI-OPT-SA", 4 },
	{ "FEF-INI-COMP-C", 5 },
	{ "FEF-INI-EFF-ADR", 6 },
	{ "FEF-INI-SELF", 7 },
};

struct sym arith_1arg_desc[] = {
	{ "ARITH-1ARG-ABS", 0 },
	{ "ARITH-1ARG-MINUS", 1 },
	{ "ARITH-1ARG-ZEROP", 2 },
	{ "ARITH-1ARG-PLUSP", 3 },
	{ "ARITH-1ARG-MINUSP", 4 },
	{ "ARITH-1ARG-ADD1", 5 },
	{ "ARITH-1ARG-SUB1", 6 },
	{ "ARITH-1ARG-FIX", 7 },
	{ "ARITH-1ARG-FLOAT", 010 },
	{ "ARITH-1ARG-SMALL-FLOAT", 011 },
	{ "ARITH-1ARG-HAULONG", 012 },
	{ "ARITH-1ARG-LDB", 013 },
	{ "ARITH-1ARG-DPB", 014 },
	{ "ARITH-1ARG-ASH", 015 },
	{ "ARITH-1ARG-ODDP", 016 },
	{ "ARITH-1ARG-EVENP", 017 },
};

struct sym arith_2arg_desc[] = {
	{ "ARITH-2ARG-ADD", 0 },
	{ "ARITH-2ARG-SUB", 1 },
	{ "ARITH-2ARG-MUL", 2 },
	{ "ARITH-2ARG-DIV", 3 },
	{ "ARITH-2ARG-EQUAL", 4 },
	{ "ARITH-2ARG-GREATERP", 5 },
	{ "ARITH-2ARG-LESSP", 6 },
	{ "ARITH-2ARG-MIN", 7 },
	{ "ARITH-2ARG-MAX", 010 },
	{ "ARITH-2ARG-BOOLE", 011 },
};

struct sym number_code_desc[] = {
	{ "NUMBER-CODE-FIXNUM",  0 },
	{ "NUMBER-CODE-SMALL-FLONUM", 1 },
	{ "NUMBER-CODE-FLONUM", 2 },
	{ "NUMBER-CODE-BIGNUM", 3 },
};


struct sym symbol_desc[] = {
	{ "sym-header", 0 },
	{ "sym-value", 1 },
	{ "sym-function", 2 },
	{ "sym-plist", 3 },
	{ "sym-package", 4 },
};

struct sym instance_descriptor_desc[] = {
	{ "INSTANCE-DESCRIPTOR-HEADER", 0 },
	{ "INSTANCE-DESCRIPTOR-RESERVED", 1 },
	{ "INSTANCE-DESCRIPTOR-SIZE", 2 },
	{ "INSTANCE-DESCRIPTOR-BINDINGS", 3 },
	{ "INSTANCE-DESCRIPTOR-FUNCTION", 4 },
	{ "INSTANCE-DESCRIPTOR-TYPENAME", 5 },
};

struct sym array_types_desc[] = {
	{ "ART-ERROR", 0 },
	{ "ART-1B", 1 },
	{ "ART-2B", 2 },
	{ "ART-4B", 3 },
	{ "ART-8B", 4 },
	{ "ART-16B", 5 },
	{ "ART-32B", 6 },
	{ "ART-Q", 7 },
	{ "ART-Q-LIST", 010 },
	{ "ART-STRING", 011 },
	{ "ART-STACK-GROUP-HEAD", 012 },
	{ "ART-SPECIAL-PDL", 013 },
	{ "ART-HALF-FIX", 014 },
	{ "ART-REGULAR-PDL", 015 },
	{ "ART-FLOAT", 016 },
	{ "ART-FPS-FLOAT", 017 },
	{ "ART-FAT-STRING", 020 },
};

struct sym sg_states_desc[] = {
	{ "SG-STATE-ERROR", 0 },
	{ "SG-STATE-ACTIVE", 1 },
	{ "SG-STATE-RESUMABLE", 2 },
	{ "SG-STATE-AWAITING-RETURN", 3 },
	{ "SG-STATE-INVOKE-CALL-ON-RETURN", 4 },
	{ "SG-STATE-INTERRUPTED-DIRTY", 5 },
	{ "SG-STATE-AWAITING-ERROR-RECOVERY", 6 },
	{ "SG-STATE-AWAITING-CALL", 7 },
	{ "SG-STATE-AWAITING-INITIAL-CALL", 8 },
	{ "SG-STATE-EXHAUSTED", 9 },
};

struct sym adi_fields_desc[] = {
	{ "ADI-TYPE", BIT_FIELD (3, 024) },
	{ "ADI-RET-STORING-OPTION", BIT_FIELD (3, 021) },
	{ "ADI-RET-SWAP-SV", BIT_FIELD (1, 020) },
	{ "ADI-RET-NUM-VALS-EXPECTING", BIT_FIELD (6, 0) },
	{ "ADI-RPC-MICRO-STACK-LEVEL", BIT_FIELD (6, 0) },
};

struct sym adi_kinds_desc[] = {
	{ "ADI-ERR", 0 },
	{ "ADI-RETURN-INFO", 1 },
	{ "ADI-RESTART-PC", 2 },
	{ "ADI-FEXPR-CALL", 3 },
	{ "ADI-LEXPR-CALL", 4 },
	{ "ADI-BIND-STACK-LEVEL", 5 },
	{ "ADI-UNUSED-6", 6 },
	{ "ADI-USED-UP-RETURN-INFO", 7 },
};

struct sym region_space_type_desc[] = {
	{ "REGION-SPACE-FREE",  0 },
	{ "REGION-SPACE-OLD", 1 },
	{ "REGION-SPACE-NEW", 2	},
	{ "REGION-SPACE-STATIC", 3 },
	{ "REGION-SPACE-FIXED", 4 },
	{ "REGION-SPACE-EXITED", 5 },
	{ "REGION-SPACE-EXIT", 6 },
	{ "REGION-SPACE-EXTRA-PDL", 7 },
	{ "REGION-SPACE-WIRED", 10 },
	{ "REGION-SPACE-USER-PAGED", 11 },
	{ "REGION-SPACE-COPY", 12 },
};

struct sym region_bits_desc[] = {
	{ "REGION-OLDSPACE-META-BIT", 023 },
	{ "REGION-EXTRA-PDL-META-BIT", 022 },
	{ "REGION-REPRESENTATION-TYPE", BIT_FIELD (2, 020) },
	{ "REGION-SPACE-TYPE", BIT_FIELD (4, 011) },
	{ "REGION-REPRESENTATION-TYPE-LIST", 0 },
	{ "REGION-REPRESENTATION-TYPE-STRUCTURE", 1 },

};

struct sym arg_desc_desc[] = {
	{ "FEF-NAME-PRESENT", BIT_FIELD (1, 020) },
	{ "FEF-SPECIAL-BIT", BIT_FIELD (1, 016) },
	{ "FEF-SPECIALNESS", BIT_FIELD (2, 016) },
	{ "FEF-FUNCTIONAL", BIT_FIELD (1, 015) },
	{ "FEF-DES-DT", BIT_FIELD (4, 011) },
	{ "FEF-QUOTE-STATUS", BIT_FIELD (2, 7) },
	{ "FEF-ARG-SYNTAX", BIT_FIELD (3, 4) },
	{ "FEF-INIT-OPTION", BIT_FIELD (4, 0) },
};

struct sym numarg_desc[] = {
	{ "ARG-DESC-QUOTED-REST", BIT_FIELD (1, 025) },
	{ "ARG-DESC-EVALED-REST", BIT_FIELD (1, 024) },
	{ "ARG-DESC-ANY-REST", BIT_FIELD (2, 024) },
	{ "ARG-DESC-FEF-QUOTE-HAIR", BIT_FIELD (1, 023) },
	{ "ARG-DESC-INTERPRETED", BIT_FIELD (1, 022) },
	{ "ARG-DESC-FEF-BIND-HAIR", BIT_FIELD (1, 021) },
	{ "ARG-DESC-MIN-ARGS", BIT_FIELD (6, 6) },
	{ "ARG-DESC-MAX-ARGS", BIT_FIELD (6, 0) },
};

#endif /* _SYMDESC_H_ */
