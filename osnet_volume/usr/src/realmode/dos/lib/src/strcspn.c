/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)strcspn.c	1.5	94/05/23 SMI\n"

/*
 * Solaris Primary Boot Subsystem - Support Library Routine
 *===========================================================================
 * Provides minimal services for the real-mode environment that the operating 
 * system would normally supply.
 *
 *   Function name:	strcspn  (strcspn.c)
 *
 *   Calling Syntax:	matchnum = strcspn ( s1, charset )
 *
 *   Description:	Return the number of characters in the maximum leading
 *			segment of string which consists solely of characters
 *			NOT from charset.
 *
 */

#include <bioserv.h>


long _FARC_
strcspn ( char _FAR_ *string, char _FAR_ *charset )
{
   register char _FAR_ *p, _FAR_ *q;

   for ( q = string; *q != '\0'; ++q ) {

      for ( p = charset; *p != '\0' && *p != *q; ++p )
         ;

      if ( *p != '\0' )
         break;

   }

   return ( (long) ( q-string ) );
}
