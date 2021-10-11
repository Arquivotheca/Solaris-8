/*
 *  Copyright (c) 1997 by Sun Microsystems, Inc.
 *  All Rights Reserved.
 */
 
#ident "@(#)memcpy.c	1.3	97/03/10 SMI"
 
/*
 *  Minimal C library for Solaris x86 real mode drivers, block copy:
 *
 *	Much like the ANSI memcpy except that we don't return a value (to
 *	prevent caller from misusing it!).
 */

#include <string.h>

void
memcpy(void far *p, const void far *q, unsigned len)
{
	/*
	 * This routine is used (among other things) for copying
	 * packets to and from network buffers.  Use the repeat
	 * string move instruction to give better performance
	 * than a C loop.  Could do some more optimization in
	 * future if needed.  For now we try to keep it simple.
	 */
	if (len > 0) {
		_asm {
			push	si
			push	di
			push	es
			push	ds
			cld
			mov	cx, len
			les	di, p
			lds	si, q
			rep	movsb
			pop	ds
			pop	es
			pop	di
			pop	si
		}
	}
}
