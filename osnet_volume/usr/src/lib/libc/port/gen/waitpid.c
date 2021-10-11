/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)waitpid.c	1.10	96/11/19 SMI"	/* SVr4.0 1.2	*/

/*LINTLIBRARY*/

#include "synonyms.h"
#include <sys/types.h>
#include <unistd.h>
#include <wait.h>

#pragma weak	_libc_waitpid = _waitpid

pid_t
waitpid(pid_t pid, int *stat_loc, int options)
{
	idtype_t idtype;
	id_t id;
	siginfo_t info;
	int error;

	if (pid > 0) {
		idtype = P_PID;
		id = pid;
	} else if (pid < -1) {
		idtype = P_PGID;
		id = -pid;
	} else if (pid == -1) {
		idtype = P_ALL;
		id = 0;
	} else {
		idtype = P_PGID;
		id = getpgid(0);
	}

	options |= (WEXITED|WTRAPPED);

	if ((error = waitid(idtype, id, &info, options)) < 0)
		return (error);

	if (stat_loc) {

		int stat = (info.si_status & 0377);

		switch (info.si_code) {
		case CLD_EXITED:
			stat <<= 8;
			break;
		case CLD_DUMPED:
			stat |= WCOREFLG;
			break;
		case CLD_KILLED:
			break;
		case CLD_TRAPPED:
		case CLD_STOPPED:
			stat <<= 8;
			stat |= WSTOPFLG;
			break;
		case CLD_CONTINUED:
			stat = WCONTFLG;
			break;
		}

		*stat_loc = stat;
	}

	return (info.si_pid);
}
