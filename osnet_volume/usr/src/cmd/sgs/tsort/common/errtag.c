/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)errtag.c	6.3	94/11/04 SMI"

/*
	Set tag to file name and line number;
	Used by errmsg() macro.
*/

#include	"errmsg.h"


void
errtag(str, num)
char	*str;
int	num;
{
	Err.tagstr = str;
	Err.tagnum = num;
}
