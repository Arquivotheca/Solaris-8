/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pr_exit.c	1.1	97/12/23 SMI"

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/lwp.h>
#include "libproc.h"

/*
 * exit() system call -- executed by subject process.
 */
int
pr_exit(struct ps_prochandle *Pr, int status)
{
	sysret_t rval;			/* return value from exit() */
	argdes_t argd[1];		/* arg descriptors for exit() */
	argdes_t *adp;

	if (Pr == NULL) {		/* no subject process */
		exit(status);
		return (0);		/* not reached */
	}

	adp = &argd[0];		/* status argument */
	adp->arg_value = status;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	rval = Psyscall(Pr, SYS_exit, 1, &argd[0]);
	/* actually -- never returns.  Expect ENOENT */

	if (rval.sys_errno < 0) {
		if (errno == ENOENT)	/* expected case */
			rval.sys_errno = ENOENT;
		else
			rval.sys_errno = ENOSYS;
	}

	if (rval.sys_errno == 0)		/* can't happen? */
		return (rval.sys_rval1);

	if (rval.sys_errno == ENOENT)	/* expected case */
		return (0);

	errno = rval.sys_errno;
	return (-1);
}

/*
 * lwp_exit() system call -- executed by subject lwp.
 */
int
pr_lwp_exit(struct ps_prochandle *Pr)
{
	sysret_t rval;			/* return value from lwp_exit() */

	if (Pr == NULL) {		/* no subject process */
		_lwp_exit();
		return (0);		/* not reached */
	}

	rval = Psyscall(Pr, SYS_lwp_exit, 0, NULL);
	/* actually -- never returns.  Expect ENOENT */

	if (rval.sys_errno < 0) {
		if (errno == ENOENT)	/* expected case */
			rval.sys_errno = ENOENT;
		else
			rval.sys_errno = ENOSYS;
	}

	if (rval.sys_errno == 0)		/* can't happen? */
		return (rval.sys_rval1);

	if (rval.sys_errno == ENOENT)	/* expected case */
		return (0);

	errno = rval.sys_errno;
	return (-1);
}
