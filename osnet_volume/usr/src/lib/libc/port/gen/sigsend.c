/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sigsend.c	1.8	96/12/03 SMI"	/* SVr4.0 1.1	*/

/*LINTLIBRARY*/

#pragma weak sigsend = _sigsend

#include "synonyms.h"
#include <sys/types.h>
#include <sys/procset.h>
#include <signal.h>

int
sigsend(idtype_t idtype, id_t id, int sig)
{
	procset_t set;
	setprocset(&set, POP_AND, idtype, id, P_ALL, P_MYID);
	return (sigsendset(&set, sig));
}
