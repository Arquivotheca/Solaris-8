/*
 *  Copyright (c) 1999 by Sun Microsystems, Inc.
 *  All rights reserved.
 *
 *  Minimal C library for Solaris x86 realmode drivers, convert ASCII to
 *  an integer.
 *
 */

#ident	"<@(#)atoin.c	1.1	99/03/23	SMI>"

int
atoin(char far *p, int len)
{
	int res, i;

	res = 0;
	for (i = 0; (i < len) && isdigit(*p); i++, p++) {
		res *= 10;
		res += *p - '0';
	}
	return (res);
}
