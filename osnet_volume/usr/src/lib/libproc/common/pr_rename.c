/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pr_rename.c	1.1	98/01/29 SMI"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "libproc.h"

/*
 * rename() system call -- executed by subject process.
 */
int
pr_rename(struct ps_prochandle *Pr, const char *old, const char *new)
{
	sysret_t rval;
	argdes_t argd[2];
	argdes_t *adp;

	if (Pr == NULL)
		return (rename(old, new));

	adp = &argd[0];		/* old argument */
	adp->arg_value = 0;
	adp->arg_object = (void *)old;
	adp->arg_type = AT_BYREF;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = strlen(old) + 1;

	adp++;			/* new argument */
	adp->arg_value = 0;
	adp->arg_object = (void *)new;
	adp->arg_type = AT_BYREF;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = strlen(new) + 1;

	rval = Psyscall(Pr, SYS_rename, 2, &argd[0]);

	if (rval.sys_errno) {
		errno = (rval.sys_errno > 0) ? rval.sys_errno : ENOSYS;
		return (-1);
	}

	return (rval.sys_rval1);
}

/*
 * link() system call -- executed by subject process.
 */
int
pr_link(struct ps_prochandle *Pr, const char *existing, const char *new)
{
	sysret_t rval;
	argdes_t argd[2];
	argdes_t *adp;

	if (Pr == NULL)
		return (link(existing, new));

	adp = &argd[0];		/* existing argument */
	adp->arg_value = 0;
	adp->arg_object = (void *)existing;
	adp->arg_type = AT_BYREF;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = strlen(existing) + 1;

	adp++;			/* new argument */
	adp->arg_value = 0;
	adp->arg_object = (void *)new;
	adp->arg_type = AT_BYREF;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = strlen(new) + 1;

	rval = Psyscall(Pr, SYS_link, 2, &argd[0]);

	if (rval.sys_errno) {
		errno = (rval.sys_errno > 0) ? rval.sys_errno : ENOSYS;
		return (-1);
	}

	return (rval.sys_rval1);
}

/*
 * unlink() system call -- executed by subject process.
 */
int
pr_unlink(struct ps_prochandle *Pr, const char *path)
{
	sysret_t rval;
	argdes_t argd[1];
	argdes_t *adp;

	if (Pr == NULL)
		return (unlink(path));

	adp = &argd[0];		/* path argument */
	adp->arg_value = 0;
	adp->arg_object = (void *)path;
	adp->arg_type = AT_BYREF;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = strlen(path) + 1;

	rval = Psyscall(Pr, SYS_unlink, 1, &argd[0]);

	if (rval.sys_errno) {
		errno = (rval.sys_errno > 0) ? rval.sys_errno : ENOSYS;
		return (-1);
	}

	return (rval.sys_rval1);
}
