/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)xtoa.c	1.5	94/05/23 SMI\n"

/*
 * Solaris Primary Boot Subsystem - Support Library Routine
 *===========================================================================
 * Provides minimal services for the real-mode environment that the operating 
 * system would normally supply.
 *
 *   Function name:	xtoa  (xtoa.c)
 *
 *   Calling Syntax:	charp = xtoa ( ulongnum, *p )
 *
 *   Description:	Convert hexadecimal integer "ulongnum" to a character
 *			string pointed to by "p".  "p"'s space must be large
 *			enough to contain the resulting string.  Return "p".
 *
 */

#include <bioserv.h>


char _FAR_ * _FARC_
xtoa ( register unsigned long i, register char _FAR_ *ptr )
{
   register short dig;
   char _FAR_ *op;

   op = ptr;

#if 0                               /* vla fornow..... */
   if ( i < 0x10 )
      dig = 1;
   else if ( i < 0x100 )
      dig = 2;
   else if ( i < 0x1000 )
      dig = 3;
   else if ( i < 0x10000 )
      dig = 4;
   else
      dig = 5;
#endif                              /* vla fornow..... */
   dig = ls_shift ( sizeof ( i ), 1 );

   ptr += dig;                               
   *(ptr--) = '\0';

   while ( --dig >= 0 ) {
      *ptr = i % 0x10 + '0'; 
      *(ptr--) = ( *ptr > '9' ) ? *ptr + 7 : *ptr;

      i = rl_shift ( i, 4 );
   }
   return ( op );
}
