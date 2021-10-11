/*
 *  Copyright (c) 1997 by Sun Microsystems, Inc.
 *  All Rights Reserved.
 */
 
#ident "@(#)strchr.c	1.1	97/01/17 SMI"
 
/*
 *  Minimal C library for Solaris x86 real mode drivers, string index:
 *
 *    Much like the ANSI strchr, except that input pointer and return value
 *    are both "far".
 */

#include <dostypes.h>
#include <string.h>

char far *
strchr(const char far *cp, int c)
{
	while (*cp && (*cp != c)) cp++;
	return ((*cp == c) ? (char far *)cp : 0);
}
