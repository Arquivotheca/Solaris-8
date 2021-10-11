/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)startup.c	1.3	90/03/01 SMI"	/* SVr4.0 1.6	*/

#include "stdio.h"

#include "lp.h"
#include "msgs.h"

#include "lpadmin.h"


/**
 ** startup() - OPEN CHANNEL TO SPOOLER
 **/

void			startup ()
{
	trap_signals ();

	if (mopen() == -1)
		scheduler_active = 0;
	else
		scheduler_active = 1;

	return;
}
