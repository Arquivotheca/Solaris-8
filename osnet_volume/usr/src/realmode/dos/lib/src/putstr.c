/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)putstr.c	1.5	94/05/23 SMI\n"

/*
 * Solaris Primary Boot Subsystem - Support Library Routine
 *===========================================================================
 * Provides minimal services for the real-mode environment that the operating 
 * system would normally supply.
 *
 *   Function name:	putstr  (putstr.c)
 *
 *   Calling Syntax:	putstr ( pmsg )
 *
 *   Description:	simple print-string routine; uses current video
 *			page, current screen position, current attribute.
 *			No return code.
 *
 */

void
putstr ( char *pstr )
{
   while ( *pstr )
         putchar ( *pstr++ );
}
