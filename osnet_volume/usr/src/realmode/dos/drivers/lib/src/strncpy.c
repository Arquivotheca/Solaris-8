/*
 *  Copyright (c) 1997 by Sun Microsystems, Inc.
 *  All Rights Reserved.
 */
 
#ident "@(#)strncpy.c	1.2	97/03/10 SMI"
 
/*
 *  Minimal C library for Solaris x86 real mode drivers, string copy:
 *
 *	Like the ANSI strncpy, except pointers are far and we don't return
 *	a value (to prevent caller from misusing it).
 */

#include <dostypes.h>
#include <string.h>

void
strncpy(char far *p, const char far *q, unsigned n)
{
	while (n-- && (*p++ = *q++));
}
