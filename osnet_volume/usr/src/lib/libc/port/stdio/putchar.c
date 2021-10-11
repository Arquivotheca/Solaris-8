/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)putchar.c	1.10	97/12/06 SMI"	/* SVr4.0 1.8	*/

/*	3.0 SID #	1.2	*/
/*LINTLIBRARY*/
/*
 * A subroutine version of the macro putchar
 */

#pragma weak putchar_unlocked = _putchar_unlocked

#include "synonyms.h"
#include "shlib.h"
#include "file64.h"
#include <stdio.h>
#include <thread.h>
#include <synch.h>
#include <mtlib.h>
#include "stdiom.h"

#undef putchar

int
putchar(ch)
	int ch;
{
	FILE *iop = stdout;

	return (putc(ch, iop));
}

#undef _putchar_unlocked

/*
 * A subroutine version of the macro putchar_unlocked
 */

int
_putchar_unlocked(ch)
	int ch;
{
	FILE *iop = stdout;

	return (PUTC(ch, iop));
}
