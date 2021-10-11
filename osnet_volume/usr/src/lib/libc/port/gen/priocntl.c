/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)priocntl.c	1.9	97/01/23 SMI"	/* SVr4.0 1.1 */

/*LINTLIBRARY*/

#include	"synonyms.h"
#include	<sys/types.h>
#include	<sys/procset.h>
#include	<sys/priocntl.h>

long
__priocntl(int pc_version, idtype_t idtype, id_t id, int cmd, caddr_t arg)
{
	procset_t	procset;

	setprocset(&procset, POP_AND, idtype, id, P_ALL, 0);

	return (__priocntlset(pc_version, &procset, cmd, arg));
}
