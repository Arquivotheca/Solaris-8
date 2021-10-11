/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pr_mmap.c	1.1	97/12/23 SMI"

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include "libproc.h"

/*
 * mmap() system call -- executed by subject process
 */
void *
pr_mmap(struct ps_prochandle *Pr,
	void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
	sysret_t rval;			/* return value from mmap() */
	argdes_t argd[6];		/* arg descriptors for mmap() */
	argdes_t *adp;

	if (Pr == NULL)		/* no subject process */
		return (mmap(addr, len, prot, flags, fd, off));

	adp = &argd[0];		/* addr argument */
	adp->arg_value = (long)addr;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	adp++;			/* len argument */
	adp->arg_value = (long)len;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	adp++;			/* prot argument */
	adp->arg_value = (long)prot;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	adp++;			/* flags argument */
	adp->arg_value = (long)(_MAP_NEW|flags);
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	adp++;			/* fd argument */
	adp->arg_value = (long)fd;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	adp++;			/* off argument */
	adp->arg_value = (long)off;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	rval = Psyscall(Pr, SYS_mmap, 6, &argd[0]);

	if (rval.sys_errno < 0)
		rval.sys_errno = ENOSYS;

	if (rval.sys_errno == 0)
		return ((void *)rval.sys_rval1);
	errno = rval.sys_errno;
	return ((void *)(-1));
}

/*
 * munmap() system call -- executed by subject process
 */
int
pr_munmap(struct ps_prochandle *Pr, void *addr, size_t len)
{
	sysret_t rval;			/* return value from munmap() */
	argdes_t argd[2];		/* arg descriptors for munmap() */
	argdes_t *adp;

	if (Pr == NULL)		/* no subject process */
		return (munmap(addr, len));

	adp = &argd[0];		/* addr argument */
	adp->arg_value = (long)addr;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	adp++;			/* len argument */
	adp->arg_value = (long)len;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	rval = Psyscall(Pr, SYS_munmap, 2, &argd[0]);

	if (rval.sys_errno < 0)
		rval.sys_errno = ENOSYS;

	if (rval.sys_errno == 0)
		return (rval.sys_rval1);
	errno = rval.sys_errno;
	return (-1);
}

/*
 * zmap() system call -- executed by subject process.
 * (zmap() should be a system call, but it isn't yet.)
 */
void *
pr_zmap(struct ps_prochandle *Pr, void *addr, size_t len, int prot, int flags)
{
	void *va = (void *)(-1);
	int zfd;

	/*
	 * Create and hold the /proc agent lwp across the following
	 * system calls executed by the subject process.
	 * This is just an optimization.
	 */
	if (Pr != NULL && Pcreate_agent(Pr) != 0)
		return (va);

	if ((zfd = pr_open(Pr, "/dev/zero", O_RDWR, 0)) >= 0) {
		va = pr_mmap(Pr, addr, len, prot, flags, zfd, (off_t)0);
		(void) pr_close(Pr, zfd);
	}

	if (Pr != NULL)
		Pdestroy_agent(Pr);

	return (va);
}
