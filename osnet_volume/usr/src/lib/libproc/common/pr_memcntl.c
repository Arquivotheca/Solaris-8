/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pr_memcntl.c	1.1	98/01/29 SMI"

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>
#include "libproc.h"

/*
 * memcntl() system call -- executed by subject process
 */

int
pr_memcntl(struct ps_prochandle *Pr,
	caddr_t addr, size_t len, int cmd, caddr_t arg, int attr, int mask)
{
	sysret_t rval;			/* return value from memcntl() */
	argdes_t argd[6];		/* arg descriptors for memcntl() */
	argdes_t *adp;

	if (Pr == NULL)		/* no subject process */
		return (memcntl(addr, len, cmd, arg, attr, mask));

	adp = &argd[0];		/* addr argument */
	adp->arg_value = (uintptr_t)addr;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	adp++;			/* len argument */
	adp->arg_value = len;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	adp++;			/* cmd argument */
	adp->arg_value = cmd;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	adp++;			/* arg argument */
	adp->arg_value = (uintptr_t)arg;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	adp++;			/* attr argument */
	adp->arg_value = attr;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	adp++;			/* mask argument */
	adp->arg_value = mask;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	rval = Psyscall(Pr, SYS_memcntl, 6, &argd[0]);

	if (rval.sys_errno < 0)
		rval.sys_errno = ENOSYS;

	if (rval.sys_errno == 0)
		return (0);
	errno = rval.sys_errno;
	return (-1);
}
