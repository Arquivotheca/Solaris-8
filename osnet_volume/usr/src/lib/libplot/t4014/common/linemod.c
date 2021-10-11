/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved
 */

#pragma ident	"@(#)linemod.c	1.7	97/10/01 SMI"	/* SVr4.0 1.2	*/
/*LINTLIBRARY*/

#include "con.h"

void
linemod(char *s)
{
	char c;
	putch(033);
	switch (s[0]) {
	case 'l':
		c = 'd';
		break;
	case 'd':
		if (s[3] != 'd')
			c = 'a';
		else c = 'b';
		break;
	case 's':
		if (s[5] != '\0')
			c = 'c';
		else c = '`';
	}
	putch(c);
}
