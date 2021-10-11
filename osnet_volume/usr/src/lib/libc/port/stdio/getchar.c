/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getchar.c	1.11	97/12/06 SMI"	/* SVr4.0 1.8	*/

/*LINTLIBRARY*/

/*
 * A subroutine version of the macro getchar.
 */

#pragma weak getchar_unlocked = _getchar_unlocked

#include "synonyms.h"
#include "shlib.h"
#include "file64.h"
#include "mtlib.h"
#include <sys/types.h>
#include <stdio.h>
#include <thread.h>
#include <synch.h>
#include "stdiom.h"

#undef getchar

#undef _getchar_unlocked

int
getchar(void)
{
	FILE *iop = stdin;

	return (getc(iop));
}

/*
 * A subroutine version of the macro getchar_unlocked.
 */


int
_getchar_unlocked(void)
{
	FILE *iop = stdin;

	return (GETC(iop));
}
