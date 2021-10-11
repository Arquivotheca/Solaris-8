/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)global.c	1.2	96/05/08 SMI"

#include "inc.h"

ARFILE	*listhead, *listend;

int	signum[] = {SIGHUP, SIGINT, SIGQUIT, 0};
