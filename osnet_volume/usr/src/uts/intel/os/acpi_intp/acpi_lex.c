/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)acpi_lex.c	1.1	99/05/21 SMI"


/* lexical analyzer for ACPI grammar */

#include <sys/inttypes.h>
#include <sys/acpi.h>

#include "acpi_exc.h"
#include "acpi_bst.h"

#include "acpi_elem.h"
#include "acpi_lex.h"


#define	T_EXT_PRE  0xFFF0	/* extended op prefix */
#define	T_LNOT_PRE 0xFFF1	/* LNOT op prefix */

/* lex table */
lex_entry_t lex_table[LEX_TABLE_SIZE] = {
/* 00 */ {CTX_PRI|CTX_NAME|CTX_DATA, T_ZERO_OP, N_ZERO}, /* ZERO */
/* 01 */ {CTX_PRI|CTX_DATA, T_ONE_OP, N_ONE}, /* ONE */
/* 02 */ {0, 0, 0},
/* 03 */ {0, 0, 0},
/* 04 */ {0, 0, 0},
/* 05 */ {0, 0, 0},
/* 06 */ {CTX_PRI|CTX_OBJ, T_ALIAS_OP, N_ALIAS}, /* ALIAS */
/* 07 */ {0, 0, 0},
/* 08 */ {CTX_PRI|CTX_OBJ, T_NAME_OP, N_NAME}, /* NAME */
/* 09 */ {0, 0, 0},
/* 0A */ {CTX_PRI|CTX_DATA, T_BYTE_OP, N_BYTE_CONST}, /* BYTE */
/* 0B */ {CTX_PRI|CTX_DATA, T_WORD_OP, N_WORD_CONST}, /* WORD */
/* 0C */ {CTX_PRI|CTX_DATA, T_DWORD_OP, N_DWORD_CONST}, /* DWORD */
/* 0D */ {CTX_PRI|CTX_DATA, T_STRING_OP, N_STRING}, /* STRING */
/* 0E */ {0, 0, 0},
/* 0F */ {0, 0, 0},

/* 10 */ {CTX_PRI|CTX_OBJ, T_SCOPE_OP, N_SCOPE}, /* SCOPE */
/* 11 */ {CTX_PRI|CTX_DATA|CTX_TYPE2, T_BUFFER_OP, N_BUFFER}, /* BUFFER */
/* 12 */ {CTX_PRI|CTX_DATA|CTX_TYPE2, T_PACKAGE_OP, N_PACKAGE}, /* PACKAGE */
/* 13 */ {0, 0, 0},
/* 14 */ {CTX_PRI|CTX_OBJ, T_METHOD_OP, N_METHOD}, /* METHOD */
/* 15 */ {0, 0, 0},
/* 16 */ {0, 0, 0},
/* 17 */ {0, 0, 0},
/* 18 */ {0, 0, 0},
/* 19 */ {0, 0, 0},
/* 1A */ {0, 0, 0},
/* 1B */ {0, 0, 0},
/* 1C */ {0, 0, 0},
/* 1D */ {0, 0, 0},
/* 1E */ {0, 0, 0},
/* 1F */ {0, 0, 0},

/* 20 */ {0, 0, 0},
/* 21 */ {0, 0, 0},
/* 22 */ {0, 0, 0},
/* 23 */ {0, 0, 0},
/* 24 */ {0, 0, 0},
/* 25 */ {0, 0, 0},
/* 26 */ {0, 0, 0},
/* 27 */ {0, 0, 0},
/* 28 */ {0, 0, 0},
/* 29 */ {0, 0, 0},
/* 2A */ {0, 0, 0},
/* 2B */ {0, 0, 0},
/* 2C */ {0, 0, 0},
/* 2D */ {0, 0, 0},
/* 2E */ {CTX_MNAME, 0, T_NAME_STRING}, /* DUAL_NAME_PREFIX */
/* 2F */ {CTX_MNAME, 0, T_NAME_STRING}, /* MULTI_NAME_PREFIX */

/* 30 */ {CTX_DNAME, 0, 0}, /* SEG_CHAR */
/* 31 */ {CTX_DNAME, 0, 0}, /* SEG_CHAR */
/* 32 */ {CTX_DNAME, 0, 0}, /* SEG_CHAR */
/* 33 */ {CTX_DNAME, 0, 0}, /* SEG_CHAR */
/* 34 */ {CTX_DNAME, 0, 0}, /* SEG_CHAR */
/* 35 */ {CTX_DNAME, 0, 0}, /* SEG_CHAR */
/* 36 */ {CTX_DNAME, 0, 0}, /* SEG_CHAR */
/* 37 */ {CTX_DNAME, 0, 0}, /* SEG_CHAR */
/* 38 */ {CTX_DNAME, 0, 0}, /* SEG_CHAR */
/* 39 */ {CTX_DNAME, 0, 0}, /* SEG_CHAR */
/* 3A */ {0, 0, 0},
/* 3B */ {0, 0, 0},
/* 3C */ {0, 0, 0},
/* 3D */ {0, 0, 0},
/* 3E */ {0, 0, 0},
/* 3F */ {0, 0, 0},

/* 40 */ {0, 0, 0},
/* 41 */ {CTX_LNAME, 0, T_NAME_STRING}, /* LEAD_NAME_CHAR */
/* 42 */ {CTX_LNAME, 0, T_NAME_STRING}, /* LEAD_NAME_CHAR */
/* 43 */ {CTX_LNAME, 0, T_NAME_STRING}, /* LEAD_NAME_CHAR */
/* 44 */ {CTX_LNAME, 0, T_NAME_STRING}, /* LEAD_NAME_CHAR */
/* 45 */ {CTX_LNAME, 0, T_NAME_STRING}, /* LEAD_NAME_CHAR */
/* 46 */ {CTX_LNAME, 0, T_NAME_STRING}, /* LEAD_NAME_CHAR */
/* 47 */ {CTX_LNAME, 0, T_NAME_STRING}, /* LEAD_NAME_CHAR */
/* 48 */ {CTX_LNAME, 0, T_NAME_STRING}, /* LEAD_NAME_CHAR */
/* 49 */ {CTX_LNAME, 0, T_NAME_STRING}, /* LEAD_NAME_CHAR */
/* 4A */ {CTX_LNAME, 0, T_NAME_STRING}, /* LEAD_NAME_CHAR */
/* 4B */ {CTX_LNAME, 0, T_NAME_STRING}, /* LEAD_NAME_CHAR */
/* 4C */ {CTX_LNAME, 0, T_NAME_STRING}, /* LEAD_NAME_CHAR */
/* 4D */ {CTX_LNAME, 0, T_NAME_STRING}, /* LEAD_NAME_CHAR */
/* 4E */ {CTX_LNAME, 0, T_NAME_STRING}, /* LEAD_NAME_CHAR */
/* 4F */ {CTX_LNAME, 0, T_NAME_STRING}, /* LEAD_NAME_CHAR */

/* 50 */ {CTX_LNAME, 0, T_NAME_STRING}, /* LEAD_NAME_CHAR */
/* 51 */ {CTX_LNAME, 0, T_NAME_STRING}, /* LEAD_NAME_CHAR */
/* 52 */ {CTX_LNAME, 0, T_NAME_STRING}, /* LEAD_NAME_CHAR */
/* 53 */ {CTX_LNAME, 0, T_NAME_STRING}, /* LEAD_NAME_CHAR */
/* 54 */ {CTX_LNAME, 0, T_NAME_STRING}, /* LEAD_NAME_CHAR */
/* 55 */ {CTX_LNAME, 0, T_NAME_STRING}, /* LEAD_NAME_CHAR */
/* 56 */ {CTX_LNAME, 0, T_NAME_STRING}, /* LEAD_NAME_CHAR */
/* 57 */ {CTX_LNAME, 0, T_NAME_STRING}, /* LEAD_NAME_CHAR */
/* 58 */ {CTX_LNAME, 0, T_NAME_STRING}, /* LEAD_NAME_CHAR */
/* 59 */ {CTX_LNAME, 0, T_NAME_STRING}, /* LEAD_NAME_CHAR */
/* 5A */ {CTX_LNAME, 0, T_NAME_STRING}, /* LEAD_NAME_CHAR */
/* 5B */ {CTX_PRI, T_EXT_PRE, 0}, /* EXT_OP_PREFIX */
/* 5C */ {CTX_MNAME, 0, T_NAME_STRING}, /* ROOT_PREFIX */
/* 5D */ {0, 0, 0},
/* 5E */ {CTX_MNAME, 0, T_NAME_STRING}, /* PARENT_PREFIX */
/* 5F */ {CTX_LNAME, 0, T_NAME_STRING}, /* LEAD_NAME_CHAR */

/* 60 */ {CTX_PRI|CTX_AAL, T_LOCAL0_OP, N_LOCAL0}, /* LOCAL0 */
/* 61 */ {CTX_PRI|CTX_AAL, T_LOCAL1_OP, N_LOCAL1}, /* LOCAL1 */
/* 62 */ {CTX_PRI|CTX_AAL, T_LOCAL2_OP, N_LOCAL2}, /* LOCAL2 */
/* 63 */ {CTX_PRI|CTX_AAL, T_LOCAL3_OP, N_LOCAL3}, /* LOCAL3 */
/* 64 */ {CTX_PRI|CTX_AAL, T_LOCAL4_OP, N_LOCAL4}, /* LOCAL4 */
/* 65 */ {CTX_PRI|CTX_AAL, T_LOCAL5_OP, N_LOCAL5}, /* LOCAL5 */
/* 66 */ {CTX_PRI|CTX_AAL, T_LOCAL6_OP, N_LOCAL6}, /* LOCAL6 */
/* 67 */ {CTX_PRI|CTX_AAL, T_LOCAL7_OP, N_LOCAL7}, /* LOCAL7 */
/* 68 */ {CTX_PRI|CTX_AAL, T_ARG0_OP, N_ARG0}, /* ARG0 */
/* 69 */ {CTX_PRI|CTX_AAL, T_ARG1_OP, N_ARG1}, /* ARG1 */
/* 6A */ {CTX_PRI|CTX_AAL, T_ARG2_OP, N_ARG2}, /* ARG2 */
/* 6B */ {CTX_PRI|CTX_AAL, T_ARG3_OP, N_ARG3}, /* ARG3 */
/* 6C */ {CTX_PRI|CTX_AAL, T_ARG4_OP, N_ARG4}, /* ARG4 */
/* 6D */ {CTX_PRI|CTX_AAL, T_ARG5_OP, N_ARG5}, /* ARG5 */
/* 6E */ {CTX_PRI|CTX_AAL, T_ARG6_OP, N_ARG6}, /* ARG6 */
/* 6F */ {0, 0, 0},

/* 70 */ {CTX_PRI|CTX_TYPE2, T_STORE_OP, N_STORE}, /* STORE */
/* 71 */ {CTX_PRI|CTX_TYPE2, T_REF_OP, N_REF}, /* REF */
/* 72 */ {CTX_PRI|CTX_TYPE2, T_ADD_OP, N_ADD}, /* ADD */
/* 73 */ {CTX_PRI|CTX_TYPE2, T_CONCAT_OP, N_CONCAT}, /* CONCAT */
/* 74 */ {CTX_PRI|CTX_TYPE2, T_SUBTRACT_OP, N_SUBTRACT}, /* SUBTRACT */
/* 75 */ {CTX_PRI|CTX_TYPE2, T_INCREMENT_OP, N_INCREMENT}, /* INCREMENT */
/* 76 */ {CTX_PRI|CTX_TYPE2, T_DECREMENT_OP, N_DECREMENT}, /* DECREMENT */
/* 77 */ {CTX_PRI|CTX_TYPE2, T_MULTIPLY_OP, N_MULTIPLY}, /* MULTIPLY */
/* 78 */ {CTX_PRI|CTX_TYPE2, T_DIVIDE_OP, N_DIVIDE}, /* DIVIDE */
/* 79 */ {CTX_PRI|CTX_TYPE2, T_SHIFT_LEFT_OP, N_SHIFT_LEFT}, /* SHIFT_LEFT */
				/* SHIFT_RIGHT */
/* 7A */ {CTX_PRI|CTX_TYPE2, T_SHIFT_RIGHT_OP, N_SHIFT_RIGHT},
/* 7B */ {CTX_PRI|CTX_TYPE2, T_AND_OP, N_AND}, /* AND */
/* 7C */ {CTX_PRI|CTX_TYPE2, T_NAND_OP, N_NAND}, /* NAND */
/* 7D */ {CTX_PRI|CTX_TYPE2, T_OR_OP, N_OR}, /* OR */
/* 7E */ {CTX_PRI|CTX_TYPE2, T_NOR_OP, N_NOR}, /* NOR */
/* 7F */ {CTX_PRI|CTX_TYPE2, T_XOR_OP, N_XOR}, /* XOR */

/* 80 */ {CTX_PRI|CTX_TYPE2, T_NOT_OP, N_NOT}, /* NOT */
/* 81 */ {CTX_PRI|CTX_TYPE2, T_LEFT_BIT_OP, N_LEFT_BIT}, /* LEFT_BIT */
/* 82 */ {CTX_PRI|CTX_TYPE2, T_RIGHT_BIT_OP, N_RIGHT_BIT}, /* RIGHT_BIT */
/* 83 */ {CTX_PRI|CTX_TYPE2, T_DEREF_OP, N_DEREF}, /* DEREF */
/* 84 */ {0, 0, 0},
/* 85 */ {0, 0, 0},
/* 86 */ {CTX_PRI|CTX_TYPE1, T_NOTIFY_OP, N_NOTIFY}, /* NOTIFY */
/* 87 */ {CTX_PRI|CTX_TYPE2, T_SIZEOF_OP, N_SIZEOF}, /* SIZEOF */
/* 88 */ {CTX_PRI|CTX_OSUP|CTX_TYPE2, T_INDEX_OP, N_INDEX}, /* INDEX */
/* 89 */ {CTX_PRI|CTX_TYPE2, T_MATCH_OP, N_MATCH}, /* MATCH */
/* 8A */ {CTX_PRI|CTX_OBJ, T_DWORD_FIELD_OP, N_DWORD_FIELD}, /* DWORD_FIELD */
/* 8B */ {CTX_PRI|CTX_OBJ, T_WORD_FIELD_OP, N_WORD_FIELD}, /* WORD_FIELD */
/* 8C */ {CTX_PRI|CTX_OBJ, T_BYTE_FIELD_OP, N_BYTE_FIELD}, /* BYTE_FIELD */
/* 8D */ {CTX_PRI|CTX_OBJ, T_BIT_FIELD_OP, N_BIT_FIELD}, /* BIT_FIELD */
/* 8E */ {CTX_PRI|CTX_TYPE2, T_TYPE_OP, N_TYPE}, /* TYPE */
/* 8F */ {0, 0, 0},

/* 90 */ {CTX_PRI|CTX_TYPE2, T_LAND_OP, N_LAND}, /* LAND */
/* 91 */ {CTX_PRI|CTX_TYPE2, T_LOR_OP, N_LOR}, /* LOR */
/* 92 */ {CTX_PRI|CTX_TYPE2, T_LNOT_PRE}, /* LNOT */
/* 93 */ {CTX_PRI|CTX_TYPE2, T_LEQ_OP, N_LEQ}, /* LEQ */
/* 94 */ {CTX_PRI|CTX_TYPE2, T_LGT_OP, N_LGT}, /* LGT */
/* 95 */ {CTX_PRI|CTX_TYPE2, T_LLT_OP, N_LLT}, /* LLT */
/* 96 */ {0, 0, 0},
/* 97 */ {0, 0, 0},
/* 98 */ {0, 0, 0},
/* 99 */ {0, 0, 0},
/* 9A */ {0, 0, 0},
/* 9B */ {0, 0, 0},
/* 9C */ {0, 0, 0},
/* 9D */ {0, 0, 0},
/* 9E */ {0, 0, 0},
/* 9F */ {0, 0, 0},

/* A0 */ {CTX_PRI|CTX_TYPE1, T_IF_OP, N_IF}, /* IF */
/* A1 */ {CTX_PRI, T_ELSE_OP, 0}, /* ELSE */
/* A2 */ {CTX_PRI|CTX_TYPE1, T_WHILE_OP, N_WHILE}, /* WHILE */
/* A3 */ {CTX_PRI|CTX_TYPE1, T_NO_OP, N_NO_OP}, /* NO */
/* A4 */ {CTX_PRI|CTX_TYPE1, T_RETURN_OP, N_RETURN}, /* RETURN */
/* A5 */ {CTX_PRI|CTX_TYPE1, T_BREAK_OP, N_BREAK}, /* BREAK */
/* A6 */ {0, 0, 0},
/* A7 */ {0, 0, 0},
/* A8 */ {0, 0, 0},
/* A9 */ {0, 0, 0},
/* AA */ {0, 0, 0},
/* AB */ {0, 0, 0},
/* AC */ {0, 0, 0},
/* AD */ {0, 0, 0},
/* AE */ {0, 0, 0},
/* AF */ {0, 0, 0},

/* B0 */ {0, 0, 0},
/* B1 */ {0, 0, 0},
/* B2 */ {0, 0, 0},
/* B3 */ {0, 0, 0},
/* B4 */ {0, 0, 0},
/* B5 */ {0, 0, 0},
/* B6 */ {0, 0, 0},
/* B7 */ {0, 0, 0},
/* B8 */ {0, 0, 0},
/* B9 */ {0, 0, 0},
/* BA */ {0, 0, 0},
/* BB */ {0, 0, 0},
/* BC */ {0, 0, 0},
/* BD */ {0, 0, 0},
/* BE */ {0, 0, 0},
/* BF */ {0, 0, 0},

/* C0 */ {0, 0, 0},
/* C1 */ {0, 0, 0},
/* C2 */ {0, 0, 0},
/* C3 */ {0, 0, 0},
/* C4 */ {0, 0, 0},
/* C5 */ {0, 0, 0},
/* C6 */ {0, 0, 0},
/* C7 */ {0, 0, 0},
/* C8 */ {0, 0, 0},
/* C9 */ {0, 0, 0},
/* CA */ {0, 0, 0},
/* CB */ {0, 0, 0},
/* CC */ {CTX_PRI|CTX_TYPE1, T_BREAKPOINT_OP, N_BREAKPOINT}, /* BREAKPOINT */
/* CD */ {0, 0, 0},
/* CE */ {0, 0, 0},
/* CF */ {0, 0, 0},

/* D0 */ {0, 0, 0},
/* D1 */ {0, 0, 0},
/* D2 */ {0, 0, 0},
/* D3 */ {0, 0, 0},
/* D4 */ {0, 0, 0},
/* D5 */ {0, 0, 0},
/* D6 */ {0, 0, 0},
/* D7 */ {0, 0, 0},
/* D8 */ {0, 0, 0},
/* D9 */ {0, 0, 0},
/* DA */ {0, 0, 0},
/* DB */ {0, 0, 0},
/* DC */ {0, 0, 0},
/* DD */ {0, 0, 0},
/* DE */ {0, 0, 0},
/* DF */ {0, 0, 0},

/* E0 */ {0, 0, 0},
/* E1 */ {0, 0, 0},
/* E2 */ {0, 0, 0},
/* E3 */ {0, 0, 0},
/* E4 */ {0, 0, 0},
/* E5 */ {0, 0, 0},
/* E6 */ {0, 0, 0},
/* E7 */ {0, 0, 0},
/* E8 */ {0, 0, 0},
/* E9 */ {0, 0, 0},
/* EA */ {0, 0, 0},
/* EB */ {0, 0, 0},
/* EC */ {0, 0, 0},
/* ED */ {0, 0, 0},
/* EE */ {0, 0, 0},
/* EF */ {0, 0, 0},

/* F0 */ {0, 0, 0},
/* F1 */ {0, 0, 0},
/* F2 */ {0, 0, 0},
/* F3 */ {0, 0, 0},
/* F4 */ {0, 0, 0},
/* F5 */ {0, 0, 0},
/* F6 */ {0, 0, 0},
/* F7 */ {0, 0, 0},
/* F8 */ {0, 0, 0},
/* F9 */ {0, 0, 0},
/* FA */ {0, 0, 0},
/* FB */ {0, 0, 0},
/* FC */ {0, 0, 0},
/* FD */ {0, 0, 0},
/* FE */ {0, 0, 0},
/* FF */ {CTX_PRI|CTX_DATA, T_ONES_OP, N_ONES}, /* ONES */
};

lex_entry_t secondary_table[LEX_TABLE_SIZE] = {
/* 00 */ {0, 0, 0},
/* 01 */ {CTX_PRI|CTX_EXT|CTX_OBJ, T_MUTEX_OP, N_MUTEX}, /* MUTEX */
/* 02 */ {CTX_PRI|CTX_EXT|CTX_OBJ, T_EVENT_OP, N_EVENT}, /* EVENT */
/* 03 */ {0, 0, 0},
/* 04 */ {0, 0, 0},
/* 05 */ {0, 0, 0},
/* 06 */ {0, 0, 0},
/* 07 */ {0, 0, 0},
/* 08 */ {0, 0, 0},
/* 09 */ {0, 0, 0},
/* 0A */ {0, 0, 0},
/* 0B */ {0, 0, 0},
/* 0C */ {0, 0, 0},
/* 0D */ {0, 0, 0},
/* 0E */ {0, 0, 0},
/* 0F */ {0, 0, 0},

/* 10 */ {0, 0, 0},
/* 11 */ {0, 0, 0},
/* 12 */ {CTX_PRI|CTX_EXT|CTX_TYPE2, T_COND_REF_OP, N_COND_REF}, /* COND_REF */
				/* CREATE_FIELD */
/* 13 */ {CTX_PRI|CTX_EXT|CTX_OBJ, T_CREATE_FIELD_OP, N_CREATE_FIELD},
/* 14 */ {0, 0, 0},
/* 15 */ {0, 0, 0},
/* 16 */ {0, 0, 0},
/* 17 */ {0, 0, 0},
/* 18 */ {0, 0, 0},
/* 19 */ {0, 0, 0},
/* 1A */ {0, 0, 0},
/* 1B */ {0, 0, 0},
/* 1C */ {0, 0, 0},
/* 1D */ {0, 0, 0},
/* 1E */ {0, 0, 0},
/* 1F */ {0, 0, 0},

/* 20 */ {CTX_PRI|CTX_EXT|CTX_TYPE1, T_LOAD_OP, N_LOAD}, /* LOAD */
/* 21 */ {CTX_PRI|CTX_EXT|CTX_TYPE1, T_STALL_OP, N_STALL}, /* STALL */
/* 22 */ {CTX_PRI|CTX_EXT|CTX_TYPE1, T_SLEEP_OP, N_SLEEP}, /* SLEEP */
/* 23 */ {CTX_PRI|CTX_EXT|CTX_TYPE2, T_ACQUIRE_OP, N_ACQUIRE}, /* ACQUIRE */
/* 24 */ {CTX_PRI|CTX_EXT|CTX_TYPE1, T_SIGNAL_OP, N_SIGNAL}, /* SIGNAL */
/* 25 */ {CTX_PRI|CTX_EXT|CTX_TYPE2, T_WAIT_OP, N_WAIT}, /* WAIT */
/* 26 */ {CTX_PRI|CTX_EXT|CTX_TYPE1, T_RESET_OP, N_RESET}, /* RESET */
/* 27 */ {CTX_PRI|CTX_EXT|CTX_TYPE1, T_RELEASE_OP, N_RELEASE}, /* RELEASE */
/* 28 */ {CTX_PRI|CTX_EXT|CTX_TYPE2, T_FROM_BCD_OP, N_FROM_BCD}, /* FROM_BCD */
/* 29 */ {CTX_PRI|CTX_EXT|CTX_TYPE2, T_TO_BCD_OP, N_TO_BCD}, /* TO_BCD */
/* 2A */ {CTX_PRI|CTX_EXT|CTX_TYPE1, T_UNLOAD_OP, N_UNLOAD}, /* UNLOAD */
/* 2B */ {0, 0, 0},
/* 2C */ {0, 0, 0},
/* 2D */ {0, 0, 0},
/* 2E */ {0, 0, 0},
/* 2F */ {0, 0, 0},

/* 30 */ {CTX_PRI|CTX_EXT|CTX_DATA, T_REVISION_OP, N_REVISION}, /* REVISION */
/* 31 */ {CTX_PRI|CTX_EXT|CTX_OSUP, T_DEBUG_OP, N_DEBUG}, /* DEBUG */
/* 32 */ {CTX_PRI|CTX_EXT|CTX_TYPE1, T_FATAL_OP, N_FATAL}, /* FATAL */
/* 33 */ {0, 0, 0},
/* 34 */ {0, 0, 0},
/* 35 */ {0, 0, 0},
/* 36 */ {0, 0, 0},
/* 37 */ {0, 0, 0},
/* 38 */ {0, 0, 0},
/* 39 */ {0, 0, 0},
/* 3A */ {0, 0, 0},
/* 3B */ {0, 0, 0},
/* 3C */ {0, 0, 0},
/* 3D */ {0, 0, 0},
/* 3E */ {0, 0, 0},
/* 3F */ {0, 0, 0},

/* 40 */ {0, 0, 0},
/* 41 */ {0, 0, 0},
/* 42 */ {0, 0, 0},
/* 43 */ {0, 0, 0},
/* 44 */ {0, 0, 0},
/* 45 */ {0, 0, 0},
/* 46 */ {0, 0, 0},
/* 47 */ {0, 0, 0},
/* 48 */ {0, 0, 0},
/* 49 */ {0, 0, 0},
/* 4A */ {0, 0, 0},
/* 4B */ {0, 0, 0},
/* 4C */ {0, 0, 0},
/* 4D */ {0, 0, 0},
/* 4E */ {0, 0, 0},
/* 4F */ {0, 0, 0},

/* 50 */ {0, 0, 0},
/* 51 */ {0, 0, 0},
/* 52 */ {0, 0, 0},
/* 53 */ {0, 0, 0},
/* 54 */ {0, 0, 0},
/* 55 */ {0, 0, 0},
/* 56 */ {0, 0, 0},
/* 57 */ {0, 0, 0},
/* 58 */ {0, 0, 0},
/* 59 */ {0, 0, 0},
/* 5A */ {0, 0, 0},
/* 5B */ {0, 0, 0},
/* 5C */ {0, 0, 0},
/* 5D */ {0, 0, 0},
/* 5E */ {0, 0, 0},
/* 5F */ {0, 0, 0},

/* 60 */ {0, 0, 0},
/* 61 */ {0, 0, 0},
/* 62 */ {0, 0, 0},
/* 63 */ {0, 0, 0},
/* 64 */ {0, 0, 0},
/* 65 */ {0, 0, 0},
/* 66 */ {0, 0, 0},
/* 67 */ {0, 0, 0},
/* 68 */ {0, 0, 0},
/* 69 */ {0, 0, 0},
/* 6A */ {0, 0, 0},
/* 6B */ {0, 0, 0},
/* 6C */ {0, 0, 0},
/* 6D */ {0, 0, 0},
/* 6E */ {0, 0, 0},
/* 6F */ {0, 0, 0},

/* 70 */ {0, 0, 0},
/* 71 */ {0, 0, 0},
/* 72 */ {0, 0, 0},
/* 73 */ {0, 0, 0},
/* 74 */ {0, 0, 0},
/* 75 */ {0, 0, 0},
/* 76 */ {0, 0, 0},
/* 77 */ {0, 0, 0},
/* 78 */ {0, 0, 0},
/* 79 */ {0, 0, 0},
/* 7A */ {0, 0, 0},
/* 7B */ {0, 0, 0},
/* 7C */ {0, 0, 0},
/* 7D */ {0, 0, 0},
/* 7E */ {0, 0, 0},
/* 7F */ {0, 0, 0},

/* 80 */ {CTX_PRI|CTX_EXT|CTX_OBJ, T_REGION_OP, N_REGION}, /* REGION */
/* 81 */ {CTX_PRI|CTX_EXT|CTX_OBJ, T_FIELD_OP, N_FIELD}, /* FIELD */
/* 82 */ {CTX_PRI|CTX_EXT|CTX_OBJ, T_DEVICE_OP, N_DEVICE}, /* DEVICE */
				/* PROCESSOR */
/* 83 */ {CTX_PRI|CTX_EXT|CTX_OBJ, T_PROCESSOR_OP, N_PROCESSOR},
				/* POWER_RES */
/* 84 */ {CTX_PRI|CTX_EXT|CTX_OBJ, T_POWER_RES_OP, N_POWER_RES},
				/* THERMAL_ZONE */
/* 85 */ {CTX_PRI|CTX_EXT|CTX_OBJ, T_THERMAL_ZONE_OP, N_THERMAL_ZONE},
				/* INDEX_FIELD */
/* 86 */ {CTX_PRI|CTX_EXT|CTX_OBJ, T_INDEX_FIELD_OP, N_INDEX_FIELD},
				/* BANK_FIELD */
/* 87 */ {CTX_PRI|CTX_EXT|CTX_OBJ, T_BANK_FIELD_OP, N_BANK_FIELD},
/* 88 */ {0, 0, 0},
/* 89 */ {0, 0, 0},
/* 8A */ {0, 0, 0},
/* 8B */ {0, 0, 0},
/* 8C */ {0, 0, 0},
/* 8D */ {0, 0, 0},
/* 8E */ {0, 0, 0},
/* 8F */ {0, 0, 0},

/* 90 */ {CTX_PRI|CTX_LNOT|CTX_TYPE2, T_LNAND_OP, N_LNAND}, /* LNAND */
/* 91 */ {CTX_PRI|CTX_LNOT|CTX_TYPE2, T_LNOR_OP, N_LNOR}, /* LNOR */
/* 92 */ {CTX_PRI|CTX_LNOT|CTX_TYPE2, T_LNZ_OP, N_LNZ}, /* LNZ */
/* 93 */ {CTX_PRI|CTX_LNOT|CTX_TYPE2, T_LNE_OP, N_LNE}, /* LNE */
/* 94 */ {CTX_PRI|CTX_LNOT|CTX_TYPE2, T_LLE_OP, N_LLE}, /* LLE */
/* 95 */ {CTX_PRI|CTX_LNOT|CTX_TYPE2, T_LGE_OP, N_LGE}, /* LGE */
/* 96 */ {0, 0, 0},
/* 97 */ {0, 0, 0},
/* 98 */ {0, 0, 0},
/* 99 */ {0, 0, 0},
/* 9A */ {0, 0, 0},
/* 9B */ {0, 0, 0},
/* 9C */ {0, 0, 0},
/* 9D */ {0, 0, 0},
/* 9E */ {0, 0, 0},
/* 9F */ {0, 0, 0},

/* A0 */ {0, 0, 0},
/* A1 */ {0, 0, 0},
/* A2 */ {0, 0, 0},
/* A3 */ {0, 0, 0},
/* A4 */ {0, 0, 0},
/* A5 */ {0, 0, 0},
/* A6 */ {0, 0, 0},
/* A7 */ {0, 0, 0},
/* A8 */ {0, 0, 0},
/* A9 */ {0, 0, 0},
/* AA */ {0, 0, 0},
/* AB */ {0, 0, 0},
/* AC */ {0, 0, 0},
/* AD */ {0, 0, 0},
/* AE */ {0, 0, 0},
/* AF */ {0, 0, 0},

/* B0 */ {0, 0, 0},
/* B1 */ {0, 0, 0},
/* B2 */ {0, 0, 0},
/* B3 */ {0, 0, 0},
/* B4 */ {0, 0, 0},
/* B5 */ {0, 0, 0},
/* B6 */ {0, 0, 0},
/* B7 */ {0, 0, 0},
/* B8 */ {0, 0, 0},
/* B9 */ {0, 0, 0},
/* BA */ {0, 0, 0},
/* BB */ {0, 0, 0},
/* BC */ {0, 0, 0},
/* BD */ {0, 0, 0},
/* BE */ {0, 0, 0},
/* BF */ {0, 0, 0},

/* C0 */ {0, 0, 0},
/* C1 */ {0, 0, 0},
/* C2 */ {0, 0, 0},
/* C3 */ {0, 0, 0},
/* C4 */ {0, 0, 0},
/* C5 */ {0, 0, 0},
/* C6 */ {0, 0, 0},
/* C7 */ {0, 0, 0},
/* C8 */ {0, 0, 0},
/* C9 */ {0, 0, 0},
/* CA */ {0, 0, 0},
/* CB */ {0, 0, 0},
/* CC */ {0, 0, 0},
/* CD */ {0, 0, 0},
/* CE */ {0, 0, 0},
/* CF */ {0, 0, 0},

/* D0 */ {0, 0, 0},
/* D1 */ {0, 0, 0},
/* D2 */ {0, 0, 0},
/* D3 */ {0, 0, 0},
/* D4 */ {0, 0, 0},
/* D5 */ {0, 0, 0},
/* D6 */ {0, 0, 0},
/* D7 */ {0, 0, 0},
/* D8 */ {0, 0, 0},
/* D9 */ {0, 0, 0},
/* DA */ {0, 0, 0},
/* DB */ {0, 0, 0},
/* DC */ {0, 0, 0},
/* DD */ {0, 0, 0},
/* DE */ {0, 0, 0},
/* DF */ {0, 0, 0},

/* E0 */ {0, 0, 0},
/* E1 */ {0, 0, 0},
/* E2 */ {0, 0, 0},
/* E3 */ {0, 0, 0},
/* E4 */ {0, 0, 0},
/* E5 */ {0, 0, 0},
/* E6 */ {0, 0, 0},
/* E7 */ {0, 0, 0},
/* E8 */ {0, 0, 0},
/* E9 */ {0, 0, 0},
/* EA */ {0, 0, 0},
/* EB */ {0, 0, 0},
/* EC */ {0, 0, 0},
/* ED */ {0, 0, 0},
/* EE */ {0, 0, 0},
/* EF */ {0, 0, 0},

/* F0 */ {0, 0, 0},
/* F1 */ {0, 0, 0},
/* F2 */ {0, 0, 0},
/* F3 */ {0, 0, 0},
/* F4 */ {0, 0, 0},
/* F5 */ {0, 0, 0},
/* F6 */ {0, 0, 0},
/* F7 */ {0, 0, 0},
/* F8 */ {0, 0, 0},
/* F9 */ {0, 0, 0},
/* FA */ {0, 0, 0},
/* FB */ {0, 0, 0},
/* FC */ {0, 0, 0},
/* FD */ {0, 0, 0},
/* FE */ {0, 0, 0},
/* FF */ {0, 0, 0},
};


/* lex driver */
int
lex(bst *bp, int lctx, int consume)
{
	int c, second, pri_flags;
	lex_entry_t *lep;

	if ((c = (consume) ? bst_get(bp) : bst_peek(bp)) == ACPI_EXC)
		return (T_EOF);	/* EOF */
	lep = &lex_table[c];

	pri_flags = lep->flags;
	if (lep->token == T_EXT_PRE) /* any secondary lookup? */
		second = CTX_EXT;
	else if (lep->token == T_LNOT_PRE)
		second = CTX_LNOT;
	else {			/* primary lookup only */
		if (!(pri_flags & lctx)) /* context okay? */
			return (ACPI_EXC);
		return ((lctx & CTX_LOOK) ? lep->look : lep->token);
	}

	/* secondary lookup */
	if (consume) {
		if ((c = bst_get(bp)) == ACPI_EXC)
			return (ACPI_EXC);
	} else {
		(void) bst_get(bp);
		if ((c = bst_peek(bp)) == ACPI_EXC)
			return (ACPI_EXC);
		(void) bst_unget(bp);
	}
	lep = &secondary_table[c];

	if (!(lep->flags & second))	/* secondary lookup okay? */
		if (second == CTX_LNOT) { /* special exception for LNOT */
			if (consume)
				(void) bst_unget(bp);
			if (!(pri_flags & lctx)) /* context okay? */
				return (ACPI_EXC);
			return ((lctx & CTX_LOOK) ? N_LNOT : T_LNOT_OP);
		} else
			return (ACPI_EXC); /* secondary is not a match */

	if (!(lep->flags & lctx)) /* context okay? */
		return (ACPI_EXC);
	return ((lctx & CTX_LOOK) ? lep->look : lep->token);
}


/* eof */
