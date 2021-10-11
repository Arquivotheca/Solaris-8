/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)errsource.c	6.3	94/11/04 SMI"

/*
 *	Set source string.
 */

#include	"errmsg.h"


void
errsource(str)
char	*str;
{
	Err.source = str;
}
