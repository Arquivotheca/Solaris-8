/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)atox.c	1.5	94/05/23 SMI\n"

/*
 * Solaris Primary Boot Subsystem - Support Library Routine
 *===========================================================================
 * Provides minimal services for the real-mode environment that the operating 
 * system would normally supply.
 *
 *   Function name:	atox  (atox.c)
 *
 *   Calling Syntax:	short = atox ( char *p )
 *
 *   Description:	Convert character string pointed to by p, into a
 *			signed short integer value. 
 *			Returns 0 if the string conversion fails.
 *
 */

#include <bioserv.h>
#include <ctype.h>


short _FARC_
atox ( register char _FAR_ *p )
{
   register short n;
   register short c, neg = 0;

   if ( !isxdigit ( c = *p ) ) {
      while ( isspace ( c ) )
         c = *++p;

      switch (c) {
           case '-':
                   neg++;
                   /* FALLTHROUGH */
           case '+':
                   c = *++p;
      }

      if ( !isxdigit ( c ) )
         return ( 0 );
   }

   for ( n = '0' - c; isxdigit ( c = *++p ); ) {
      n *= 0x10;               /* two steps to avoid unnecessary overflow */
      n += '0' - c;          /* accum neg to avoid surprises at MAX */
   }
   return ( neg ? n : -n );
}
