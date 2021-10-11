/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)strcpy.c	1.5	94/05/23 SMI\n"

/*
 * Solaris Primary Boot Subsystem - Support Library Routine
 *===========================================================================
 * Provides minimal services for the real-mode environment that the operating 
 * system would normally supply.
 *
 *   Function name:	strcpy  (strcpy.c)
 *
 *   Calling Syntax:
 *	read_disk ( dev, cyl#, head#, sector#, nsectors, destination )
 *
 *   Description:	Copy string s2 to s1.  S1's space must be large enough.
 *			Return s1.
 *
 */

#include <bioserv.h>


char _FAR_ * _FARC_
strcpy ( register char _FAR_ *s1, register char _FAR_ *s2 )
{
        register char _FAR_ *os1 = s1;

        while ( *s1++ = *s2++ )
                ;
        return ( os1 );
}
