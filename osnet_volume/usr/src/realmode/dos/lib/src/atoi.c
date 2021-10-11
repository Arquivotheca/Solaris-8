/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)atoi.c	1.5	94/05/23 SMI\n"

/*
 * Solaris Primary Boot Subsystem - Support Library Routine
 *===========================================================================
 * Provides minimal services for the real-mode environment that the operating 
 * system would normally supply.
 *
 *   Function name:	atoi/atol  (atoi.c)
 *
 *   Calling Syntax:	short = atoi ( char *p )
 *			long  = atol ( char *p )
 *
 *   Description:	Convert character string pointed to by p, into signed
 *			integer value.  Returns (short/long) integer, or
 *			0 if the string conversion fails.
 *
 */

#include <bioserv.h>
#include <ctype.h>

#define ATOI

#ifdef  ATOI
typedef short TYPE;
#define NAME    atoi
#else
typedef long TYPE;
#define NAME    atol
#endif


TYPE _FARC_
NAME ( register char _FAR_ *p )
{
   register TYPE n;
   register short c, neg = 0;

   if ( !isdigit ( c = *p ) ) {
      while ( isspace ( c ) )
         c = *++p;

      switch (c) {
           case '-':
                   neg++;
                   /* FALLTHROUGH */
           case '+':
                   c = *++p;
      }

      if ( !isdigit ( c ) )
         return ( 0 );
   }

   for ( n = '0' - c; isdigit ( c = *++p ); ) {
      n *= 10;               /* two steps to avoid unnecessary overflow */
      n += '0' - c;          /* accum neg to avoid surprises at MAX */
   }
   return ( neg ? n : -n );
}

