/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)Pisprocdir.c	1.2	98/01/29 SMI"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "libproc.h"

/*
 * Return TRUE iff dir is the /proc directory.
 */
int
Pisprocdir(struct ps_prochandle *Pr, const char *dir)
{
	struct stat statb;
	struct statvfs statvfsb;
	int truth;

	/*
	 * Create and hold the /proc agent lwp across the following
	 * two system calls executed by the subject process.
	 * This is just an optimization.
	 */
	if (Pr != NULL && Pcreate_agent(Pr) != 0)
		return (0);

	/*
	 * We can't compare the statb.st_fstype string to "proc" because
	 * a loop-back mount of /proc would show "lofs" instead of "proc".
	 * Instead we use the statvfs() f_basetype string.
	 */
	truth = (pr_stat(Pr, dir, &statb) == 0 &&
	    pr_statvfs(Pr, dir, &statvfsb) == 0 &&
	    (statb.st_mode & S_IFMT) == S_IFDIR &&
	    statb.st_ino == 2 &&
	    strcmp(statvfsb.f_basetype, "proc") == 0);

	if (Pr != NULL)
		Pdestroy_agent(Pr);

	return (truth);
}
