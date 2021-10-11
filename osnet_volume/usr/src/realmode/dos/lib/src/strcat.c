/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)strcat.c	1.5	94/05/23 SMI\n"

/*
 * Solaris Primary Boot Subsystem - Support Library Routine
 *===========================================================================
 * Provides minimal services for the real-mode environment that the operating 
 * system would normally supply.
 *
 *   Function name:	strcat  (strcat.c)
 *
 *   Calling Syntax:	s1 = strcat ( s1, s2 )
 *
 *   Description:	Concatenate s2 on the end of s1.  S1's space must be
 *			large enough.  Return s1.
 *
 */

#include <bioserv.h>


char _FAR_ * _FARC_
strcat ( register char _FAR_ *s1, register char _FAR_ *s2 )
{
   register char _FAR_ *os1 = s1;

   while ( *s1++ )
           ;
   --s1;
   while ( *s1++ = *s2++ )
                ;
   return ( os1 );
}
