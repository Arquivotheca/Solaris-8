/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)gluecode.c	1.5	94/05/23 SMI\n"

/*
 * Gluecode for secondary boot that returns control back to the MDB driver
 */
#ifdef TURBOC
#define		ASM	asm
#endif
#ifdef MSC60
#define		ASM	_asm
#endif
#ifdef MSC70
#define		ASM	__asm
#endif

void
main()
{
	ASM	int	21h
}

