/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)erraction.c	6.3	94/11/04 SMI"

/*
 *	Routine called after error message has been printed.
 *	Dependent upon the return code of errafter.
 *	Command and library version.
 */

#include	"errmsg.h"
#include	<stdio.h>
#ifdef __STDC__
#include <stdlib.h>
#else
	extern void exit();
#endif

void
erraction(action)
int	action;
{
	switch (action) {
	case EABORT:
		abort();
		break;
	case EEXIT:
		exit(Err.exit);
		break;
	case ERETURN:
		break;
	}
}
