/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pr_ioctl.c	1.1	97/12/23 SMI"

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include "libproc.h"

/*
 * ioctl() system call -- executed by subject process.
 */
int
pr_ioctl(struct ps_prochandle *Pr, int fd, int code, void *buf, size_t size)
{
	sysret_t rval;			/* return value from ioctl() */
	argdes_t argd[3];		/* arg descriptors for ioctl() */
	argdes_t *adp = &argd[0];	/* first argument */

	if (Pr == NULL)		/* no subject process */
		return (ioctl(fd, code, buf));

	adp->arg_value = fd;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;
	adp++;			/* move to code argument */

	adp->arg_value = code;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;
	adp++;			/* move to buffer argument */

	if (size == 0) {
		adp->arg_value = (long)buf;
		adp->arg_object = NULL;
		adp->arg_type = AT_BYVAL;
		adp->arg_inout = AI_INPUT;
		adp->arg_size = 0;
	} else {
		adp->arg_value = 0;
		adp->arg_object = buf;
		adp->arg_type = AT_BYREF;
		adp->arg_inout = AI_INOUT;
		adp->arg_size = size;
	}

	rval = Psyscall(Pr, SYS_ioctl, 3, &argd[0]);

	if (rval.sys_errno) {
		errno = (rval.sys_errno > 0)? rval.sys_errno : ENOSYS;
		return (-1);
	}

	return (rval.sys_rval1);
}
