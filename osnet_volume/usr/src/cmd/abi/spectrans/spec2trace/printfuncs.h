/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _PRINTFUNCS_H
#define	_PRINTFUNCS_H

#pragma ident	"@(#)printfuncs.h	1.2	99/05/14 SMI"


#ifdef	__cplusplus
extern "C" {
#endif

/* Types */
enum { CHAR, SHORT, UNSIGNED_SHORT, INT, UNSIGNED, LONG, UNSIGNED_LONG,
	CHAR_P, POINTER, FLOAT, LONG_LONG, UNSIGNED_LONG_LONG, VOID_,
	NONPRIMITIVE};

void generate_printf(ENTRY *);

/* Define, declare, initialize and use pointers to printfuncs. */
void generate_print_definitions(FILE *);
void generate_print_declarations(FILE *);
void generate_print_initializations(void);
void generate_printfunc_calls(ENTRY *); /* Use. */

int is_void(ENTRY *);

#ifdef	__cplusplus
}
#endif

#endif	/* _PRINTFUNCS_H */
