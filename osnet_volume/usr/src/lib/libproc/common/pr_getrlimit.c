/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pr_getrlimit.c	1.2	98/07/13 SMI"

#define	_LARGEFILE64_SOURCE

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/resource.h>
#include "libproc.h"

/*
 * getrlimit() system call -- executed by subject process.
 */
int
pr_getrlimit(struct ps_prochandle *Pr,
	int resource, struct rlimit *rlp)
{
	sysret_t rval;			/* return value from getrlimit() */
	argdes_t argd[2];		/* arg descriptors for getrlimit() */
	argdes_t *adp;
	int sysnum;

	if (Pr == NULL)		/* no subject process */
		return (getrlimit(resource, rlp));

	adp = &argd[0];		/* resource argument */
	adp->arg_value = resource;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	adp++;			/* rlp argument */
	adp->arg_value = 0;
	adp->arg_object = rlp;
	adp->arg_type = AT_BYREF;
	adp->arg_inout = AI_OUTPUT;
	adp->arg_size = sizeof (*rlp);

#ifdef _LP64
	if (Pstatus(Pr)->pr_dmodel == PR_MODEL_ILP32)
		sysnum = SYS_getrlimit64;
	else
		sysnum = SYS_getrlimit;
#else	/* _LP64 */
	sysnum = SYS_getrlimit;
#endif	/* _LP64 */
	rval = Psyscall(Pr, sysnum, 2, &argd[0]);

	if (rval.sys_errno) {
		errno = (rval.sys_errno > 0)? rval.sys_errno : ENOSYS;
		return (-1);
	}

	return (rval.sys_rval1);
}

/*
 * setrlimit() system call -- executed by subject process.
 */
int
pr_setrlimit(struct ps_prochandle *Pr,
	int resource, const struct rlimit *rlp)
{
	sysret_t rval;			/* return value from setrlimit() */
	argdes_t argd[2];		/* arg descriptors for setrlimit() */
	argdes_t *adp;
	int sysnum;

	if (Pr == NULL)		/* no subject process */
		return (setrlimit(resource, rlp));

	adp = &argd[0];		/* resource argument */
	adp->arg_value = resource;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	adp++;			/* rlp argument */
	adp->arg_value = 0;
	adp->arg_object = (void *)rlp;
	adp->arg_type = AT_BYREF;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = sizeof (*rlp);

#ifdef _LP64
	if (Pstatus(Pr)->pr_dmodel == PR_MODEL_ILP32)
		sysnum = SYS_setrlimit64;
	else
		sysnum = SYS_setrlimit;
#else	/* _LP64 */
	sysnum = SYS_setrlimit;
#endif	/* _LP64 */
	rval = Psyscall(Pr, sysnum, 2, &argd[0]);

	if (rval.sys_errno) {
		errno = (rval.sys_errno > 0)? rval.sys_errno : ENOSYS;
		return (-1);
	}

	return (rval.sys_rval1);
}

/*
 * getrlimit64() system call -- executed by subject process.
 */
int
pr_getrlimit64(struct ps_prochandle *Pr,
	int resource, struct rlimit64 *rlp)
{
	sysret_t rval;			/* return value from getrlimit() */
	argdes_t argd[2];		/* arg descriptors for getrlimit() */
	argdes_t *adp;
	int sysnum;

	if (Pr == NULL)		/* no subject process */
		return (getrlimit64(resource, rlp));

	adp = &argd[0];		/* resource argument */
	adp->arg_value = resource;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	adp++;			/* rlp argument */
	adp->arg_value = 0;
	adp->arg_object = rlp;
	adp->arg_type = AT_BYREF;
	adp->arg_inout = AI_OUTPUT;
	adp->arg_size = sizeof (*rlp);

#ifdef _LP64
	if (Pstatus(Pr)->pr_dmodel == PR_MODEL_ILP32)
		sysnum = SYS_getrlimit64;
	else
		sysnum = SYS_getrlimit;
#else	/* _LP64 */
	sysnum = SYS_getrlimit64;
#endif	/* _LP64 */
	rval = Psyscall(Pr, sysnum, 2, &argd[0]);

	if (rval.sys_errno) {
		errno = (rval.sys_errno > 0)? rval.sys_errno : ENOSYS;
		return (-1);
	}

	return (rval.sys_rval1);
}

/*
 * setrlimit64() system call -- executed by subject process.
 */
int
pr_setrlimit64(struct ps_prochandle *Pr,
	int resource, const struct rlimit64 *rlp)
{
	sysret_t rval;			/* return value from setrlimit() */
	argdes_t argd[2];		/* arg descriptors for setrlimit() */
	argdes_t *adp;
	int sysnum;

	if (Pr == NULL)		/* no subject process */
		return (setrlimit64(resource, rlp));

	adp = &argd[0];		/* resource argument */
	adp->arg_value = resource;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	adp++;			/* rlp argument */
	adp->arg_value = 0;
	adp->arg_object = (void *)rlp;
	adp->arg_type = AT_BYREF;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = sizeof (*rlp);

#ifdef _LP64
	if (Pstatus(Pr)->pr_dmodel == PR_MODEL_ILP32)
		sysnum = SYS_setrlimit64;
	else
		sysnum = SYS_setrlimit;
#else	/* _LP64 */
	sysnum = SYS_setrlimit64;
#endif	/* _LP64 */
	rval = Psyscall(Pr, sysnum, 2, &argd[0]);

	if (rval.sys_errno) {
		errno = (rval.sys_errno > 0)? rval.sys_errno : ENOSYS;
		return (-1);
	}

	return (rval.sys_rval1);
}
