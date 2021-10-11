/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)memset.c	1.5	94/05/23 SMI\n"

/*
 * Solaris Primary Boot Subsystem - Support Library Routine
 *===========================================================================
 * Provides minimal services for the real-mode environment that the operating 
 * system would normally supply.
 *
 *   Function name:	memset  (memset.c)
 *
 *   Calling Syntax:	start = memset ( start, val, count )
 *
 *   Description:	sets a range of "count" chars starting at "start"
 *			to the character "val".  Returns "start".
 *
 */

#include <bioserv.h>


char _FAR_ * _FARC_
memset ( char _FAR_ *sp1, register short c, register long n )
{
   if (n != 0) {
      register char _FAR_ *sp = sp1;
      do {
         *sp++ = ( char ) c;
      } while ( --n != 0 );
   }
   return( sp1 );
}
