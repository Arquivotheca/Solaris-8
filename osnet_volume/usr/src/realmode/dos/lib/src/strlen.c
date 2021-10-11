/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)strlen.c	1.5	94/05/23 SMI\n"

/*
 * Solaris Primary Boot Subsystem - Support Library Routine
 *===========================================================================
 * Provides minimal services for the real-mode environment that the operating 
 * system would normally supply.
 *
 *   Function name:	strlen  (strlen.c)
 *
 *   Calling Syntax:	length = strlen ( s1 )
 *
 *   Description:	returns the length (in bytes) of the specified string.
 *
 */

#include <bioserv.h>


long _FARC_
strlen ( register char _FAR_ *pstr )
{
	register long i;

	for ( i=0; *pstr; pstr++, i++ )
		;
	return ( i );
}
