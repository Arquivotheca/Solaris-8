/*
 *  Copyright (c) 1999 by Sun Microsystems, Inc.
 *  All rights reserved.
 *
 *  Minimal C library for Solaris x86 realmode drivers, is the character
 *  a digit?
 *
 */

#ident	"<@(#)isdigit.c	1.1	99/03/23	SMI>"

int
isdigit(char c)
{
	if ((c >= '0') && (c <= '9')) {
		return (1);
	} else {
		return (0);
	}
}
