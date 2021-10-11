/*
 *  Copyright (c) 1997 by Sun Microsystems, Inc.
 *  All Rights Reserved.
 */
 
#ident "@(#)strcpy.c	1.2	97/03/10 SMI"
 
/*
 *  Minimal C library for Solaris x86 real mode drivers, string copy:
 *
 *	Much like the ANSI strcpy, except we don't return a value (to prevent
 *	caller from misusing it).
 */

#include <dostypes.h>
#include <string.h>

void
strcpy(char far *p, const char far *q)
{
	while (*p++ = *q++);
}
