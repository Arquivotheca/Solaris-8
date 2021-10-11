/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)strcmp.c	1.5	94/05/23 SMI\n"

/*
 * Solaris Primary Boot Subsystem - Support Library Routine
 *===========================================================================
 * Provides minimal services for the real-mode environment that the operating 
 * system would normally supply.
 *
 *   Function name:	strcmp  (strcmp.c)
 *
 *   Calling Syntax:	compnum = strcmp ( s1, s2 )
 *
 *   Description:	Compare strings:  s1>s2: >0  s1==s2: 0  s1<s2: <0
 *
 */

#include <bioserv.h>


long _FARC_
strcmp ( register char _FAR_ *s1, register char _FAR_ *s2 )
{

   if ( s1 == s2 )
      return ( 0 );

   while( *s1 == *s2++ )
      if ( *s1++ == '\0' )
         return ( 0 );

   return ( *s1 - s2[-1] );
}
