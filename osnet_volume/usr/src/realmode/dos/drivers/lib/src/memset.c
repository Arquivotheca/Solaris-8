/*
 *  Copyright (c) 1997 by Sun Microsystems, Inc.
 *  All Rights Reserved.
 */
 
#ident "@(#)memset.c	1.2	97/03/10 SMI"
 
/*
 *  Minimal C library for Solaris x86 real mode drivers, block initialize:
 *
 *	Much like the ANSI memset except that we don't return a value (to
 *	prevent caller from misusing it!).
 */

#include <string.h>

void
memset(void far *p, int c, unsigned len)
{
	char far *cp = (char far *)p;
	while (len-- > 0) *cp++ = c;
}
