/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)errexit.c	6.3	94/11/04 SMI"

#include  "errmsg.h"

/*
 *	This routine sets the exit(2) value for exit actions.
 * 	It returns the previous value.
 */

int
errexit(e)
int	e;
{
	int	oe;

	oe = Err.exit;
	Err.exit = e;
	return (oe);
}
