/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)strncpy.c	1.5	94/05/23 SMI\n"

/*
 * Solaris Primary Boot Subsystem - Support Library Routine
 *===========================================================================
 * Provides minimal services for the real-mode environment that the operating 
 * system would normally supply.
 *
 *   Function name:	strncpy  (strncpy.c)
 *
 *   Calling Syntax:	s1 = strncpy ( s1, s2, count )
 *
 *   Description:	Copy string s2 to s1, truncating or null-padding to
 *			always copy n bytes.  S1's space must be large enough.
 *			Return s1.
 *
 */

#include <bioserv.h>


char _FAR_ * _FARC_
strncpy ( register char _FAR_ *s1, register char _FAR_ *s2, register short n )
{
   register char _FAR_ *os1 = s1;

   n++;                            
   while ( ( --n != 0 ) &&  ( ( *s1++ = *s2++ ) != '\0' ) )
           ;
   if ( n != 0 )
      while ( --n != 0 )
         *s1++ = '\0';

   return ( os1 );
}
