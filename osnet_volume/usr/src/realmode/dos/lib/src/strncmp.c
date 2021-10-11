/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)strncmp.c	1.5	94/05/23 SMI\n"

/*
 * Solaris Primary Boot Subsystem - Support Library Routine
 *===========================================================================
 * Provides minimal services for the real-mode environment that the operating 
 * system would normally supply.
 *
 *   Function name:	strncmp  (strncmp.c)
 *
 *   Calling Syntax:	compnum = strncmp ( s1, s2, maxlen )
 *
 *   Description:	Compare strings (at most n bytes)
 *			returns: s1>s2: >0  s1==s2: 0  s1<s2: <0
 *
 */

#include <bioserv.h>


long _FARC_
strncmp ( register char _FAR_ *s1, register char _FAR_ *s2, register short n)
{
   n++;
   if ( s1 == s2 )
      return ( 0 );

   while ( --n != 0 && *s1 == *s2++ )
      if ( *s1++ == '\0' )
         return ( 0 );

   return ( ( n == 0 )? 0: ( *s1 - s2[-1] ) );
}
