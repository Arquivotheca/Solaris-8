/*
 *  Copyright (c) 1997 by Sun Microsystems, Inc.
 *  All Rights Reserved.
 */
 
#ident "@(#)strcmp.c	1.1	97/01/17 SMI"
 
/*
 *  Minimal C library for Solaris x86 real mode drivers, string compare:
 *
 *    Much like the ANSI strcmp, except that input pointers are "far".
 */

#include <dostypes.h>
#include <string.h>

int
strcmp(const char far *p, const char far *q)
{
	int j;

	while (!(j = (*p - *q)) && *p++) q++;
	return (j);
}
