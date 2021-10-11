/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)strncat.c	1.5	94/05/23 SMI\n"

/*
 * Solaris Primary Boot Subsystem - Support Library Routine
 *===========================================================================
 * Provides minimal services for the real-mode environment that the operating 
 * system would normally supply.
 *
 *   Function name:	strncat  (strncat.c)
 *
 *   Calling Syntax:	ps1 = strncat ( ps1, ps2, strmax )
 *
 *   Description:	Concatenate s2 on the end of s1.  S1's space must be
 *			large enough. At most n characters are moved.
 *			Return s1.
 *
 */

#include <bioserv.h>

char _FAR_ * _FARC_
strncat ( register char _FAR_ *s1, register char _FAR_ * s2, register short n)
{
	register char _FAR_ *os1 = s1;

	n++;
	while ( *s1++ )
        		;

	--s1;
	while ( *s1++ = *s2++ ) {
		if ( --n == 0 ) {
			s1[-1] = '\0';
			break;
		}
   }

	return(os1);
}
