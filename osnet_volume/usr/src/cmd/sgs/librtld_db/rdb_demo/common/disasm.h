/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided this notice is preserved and that due credit is given
 * to Sun Microsystems, Inc.  The name of Sun Microsystems, Inc. may
 * not be used to endorse or promote products derived from this
 * software without specific prior written permission.  This software
 * is provided ``as is'' without express or implied warranty.
 */
#pragma ident	"@(#)disasm.h	1.3	98/03/18 SMI"

#if ! defined(_DISASM_H)
#define	_DISASM_H

#pragma	ident	"@(#)disasm.h	1.3	98/03/18 SMI"

typedef char *(*FUNCPTR)();

extern char *	disassemble(unsigned int, unsigned long, FUNCPTR,
			unsigned int, unsigned int, int);

#define	V8_MODE		1	/* V8 */
#define	V9_MODE		2	/* V9 */
#define	V9_SGI_MODE	4	/* V9/SGI */

#endif
