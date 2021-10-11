/*
 *  Copyright (c) 1997 by Sun Microsystems, Inc.
 *  All Rights Reserved.
 */
 
#ident "@(#)strlen.c	1.1	97/01/17 SMI"
 
/*
 *  Minimal C library for Solaris x86 real mode drivers, string length:
 *
 *    Much like the ANSI strcmp, except that input pointers are "far".
 */

#include <dostypes.h>
#include <string.h>

int
strlen(const char far *p)
{
	int j = 0;

	while (*p++) j++;
	return (j);
}
