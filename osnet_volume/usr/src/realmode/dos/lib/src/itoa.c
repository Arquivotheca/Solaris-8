/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)itoa.c	1.5	94/05/23 SMI\n"

/*
 * Solaris Primary Boot Subsystem - Support Library Routine
 *===========================================================================
 * Provides minimal services for the real-mode environment that the operating 
 * system would normally supply.
 *
 *   Function name:	itoa  (itoa.c)
 *
 *   Calling Syntax:	charp = itoa ( shortnum, *p )
 *
 *   Description:	Convert integer i to a character string pointed to by
 *			"ptr".  "ptr's" space must be large enough to contain
 *			the resulting string.  Returns "ptr".
 *
 */

#include <bioserv.h>


char _FAR_ * _FARC_
itoa ( register short i, register char _FAR_ *ptr )
{
#define radix 10
   register short dig, tmpnum;
   char _FAR_ *op;

   op = ptr;

   for ( dig = 1, tmpnum = i; tmpnum >= radix; ++dig ) {
      tmpnum /= radix;
      tmpnum = tmpnum % radix ? tmpnum++ : tmpnum;
   }

   ptr += dig;
   *ptr = '\0';

   while ( --dig >= 0 ) {
      *(--ptr) = i % radix + '0';
      i /= radix;
   }
   return ( op );
}

