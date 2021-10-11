/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)testpt.c	1.5	94/05/23 SMI\n"

/*
 * Solaris Primary Boot Subsystem - Support Library Routine
 *===========================================================================
 * Provides minimal services for the real-mode environment that the operating 
 * system would normally supply.
 *
 *   Function name:	testpt  (testpt.c)
 *
 *   Calling Syntax:	testpt ( charp )
 *
 *   Description:	useful debugging function.  prints a single character
 *			at the upper left corner of the screen.
 *			No return code.
 *
 */

#include <bioserv.h>

void _FARC_
testpt ( register char _FAR_ *marker )
{
   prtstr_pos ( marker, 1, ask_page(), 0, 0 );
}

