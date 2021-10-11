/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)farbzero.c	1.5	94/05/23 SMI\n"

/*
 * Solaris Primary Boot Subsystem - Support Library Routine
 *===========================================================================
 * Provides minimal services for the real-mode environment that the operating 
 * system would normally supply.
 *
 *   Function name:	farbzero  (farbzero.c)
 *
 *   Calling Syntax:	farbzero ( *start, length )
 *
 *   Description:	similar functionality to memset; sets a specified
 *			number of bytes to zero, starting at the position
 *			pointed to by "s".  No return code.
 *
 */

#include <bioserv.h>

short
farbzero ( register char _FAR_ *s, register short n )
{
#if 0
   for ( ; n >= 4; n -= 4 )	/* plop 'em out, four at a time. */
		(long)(char _FAR_ *)s = 0L;
#endif
	while (n--)			/* handle the modulo part here */
		*s++ = 0;
}

