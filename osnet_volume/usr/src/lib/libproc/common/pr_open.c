/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pr_open.c	1.1	97/12/23 SMI"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include "libproc.h"

/*
 * open() system call -- executed by subject process.
 */
int
pr_open(struct ps_prochandle *Pr, const char *filename, int flags, mode_t mode)
{
	sysret_t rval;			/* return value from open() */
	argdes_t argd[3];		/* arg descriptors for open() */
	argdes_t *adp;

	if (Pr == NULL)		/* no subject process */
		return (open(filename, flags, mode));

	adp = &argd[0];		/* filename argument */
	adp->arg_value = 0;
	adp->arg_object = (void *)filename;
	adp->arg_type = AT_BYREF;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = strlen(filename)+1;

	adp++;			/* flags argument */
	adp->arg_value = (long)flags;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	adp++;			/* mode argument */
	adp->arg_value = (long)mode;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	rval = Psyscall(Pr, SYS_open, 3, &argd[0]);

	if (rval.sys_errno) {
		errno = (rval.sys_errno > 0)? rval.sys_errno : ENOSYS;
		return (-1);
	}

	return (rval.sys_rval1);
}

/*
 * creat() system call -- executed by subject process.
 */
int
pr_creat(struct ps_prochandle *Pr, const char *filename, mode_t mode)
{
	sysret_t rval;			/* return value from creat() */
	argdes_t argd[2];		/* arg descriptors for creat() */
	argdes_t *adp;

	if (Pr == NULL)		/* no subject process */
		return (creat(filename, mode));

	adp = &argd[0];		/* filename argument */
	adp->arg_value = 0;
	adp->arg_object = (void *)filename;
	adp->arg_type = AT_BYREF;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = strlen(filename)+1;

	adp++;			/* mode argument */
	adp->arg_value = (long)mode;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	rval = Psyscall(Pr, SYS_creat, 2, &argd[0]);

	if (rval.sys_errno) {
		errno = (rval.sys_errno > 0)? rval.sys_errno : ENOSYS;
		return (-1);
	}

	return (rval.sys_rval1);
}

/*
 * close() system call -- executed by subject process.
 */
int
pr_close(struct ps_prochandle *Pr, int fd)
{
	sysret_t rval;			/* return value from close() */
	argdes_t argd[1];		/* arg descriptors for close() */
	argdes_t *adp;

	if (Pr == NULL)		/* no subject process */
		return (close(fd));

	adp = &argd[0];		/* fd argument */
	adp->arg_value = (int)fd;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	rval = Psyscall(Pr, SYS_close, 1, &argd[0]);

	if (rval.sys_errno) {
		errno = (rval.sys_errno > 0)? rval.sys_errno : ENOSYS;
		return (-1);
	}

	return (rval.sys_rval1);
}
