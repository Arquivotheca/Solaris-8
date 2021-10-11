/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)bcopy.c	1.5	94/05/23 SMI\n"

/*
 * Solaris Primary Boot Subsystem - Support Library Routine
 *===========================================================================
 * Provides minimal services for the real-mode environment that the operating 
 * system would normally supply.
 *
 *   Function name:	bcopy  (bcopy.c)
 *
 *   Calling Syntax:	bcopy ( *source, *dest, length )
 *
 *   Description:	copies bytes from source to destination.
 *			No return code.
 *
 *   Restriction:	Transfer must occur within the same 64K segment.
 *
 */

#include <bioserv.h>

void
bcopy ( register char _FAR_ *s, register char _FAR_ *d, register long n )
{
	while (n--)
		*d++ = *s++;
}

