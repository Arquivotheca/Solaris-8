/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)corectl.c	1.1	99/03/31 SMI"

#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/kmem.h>
#include <sys/sysmacros.h>
#include <sys/procset.h>
#include <sys/corectl.h>
#include <sys/cmn_err.h>

/*
 * core file control system call.
 */

refstr_t	*core_file;
uint32_t	core_options = CC_PROCESS_PATH;
kmutex_t	core_lock;

int		allow_setid_core;	/* obsolete */

static int set_proc_path(pid_t pid, refstr_t *rp);

/*
 * Called once, from icode(), to set init's core file name pattern.
 */
void
init_core()
{
	curproc->p_corefile = refstr_alloc("core");
	if (allow_setid_core) {
		core_options |= CC_PROCESS_SETID;
		cmn_err(CE_NOTE,
		    "'set allow_setid_core = 1' in /etc/system is obsolete");
		cmn_err(CE_NOTE,
		    "Use the coreadm command instead of 'allow_setid_core'");
	}
}

int
corectl(int subcode, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3)
{
	int error = 0;
	proc_t *p;
	refstr_t *rp;
	size_t size;
	char *path;

	switch (subcode) {
	case CC_SET_OPTIONS:
		if (!suser(CRED()))
			error = EPERM;
		else if (arg1 & ~CC_OPTIONS)
			error = EINVAL;
		else
			core_options = (uint32_t)arg1;
		break;

	case CC_GET_OPTIONS:
		return (core_options);

	case CC_GET_GLOBAL_PATH:
	case CC_GET_PROCESS_PATH:
		if (subcode == CC_GET_GLOBAL_PATH) {
			mutex_enter(&core_lock);
			if ((rp = core_file) != NULL)
				refstr_hold(rp);
			mutex_exit(&core_lock);
		} else {
			rp = NULL;
			mutex_enter(&pidlock);
			if ((p = prfind((pid_t)arg3)) == NULL ||
			    p->p_stat == SIDL) {
				mutex_exit(&pidlock);
				error = ESRCH;
			} else {
				mutex_enter(&p->p_lock);
				mutex_exit(&pidlock);
				mutex_enter(&p->p_crlock);
				/*
				 * Allow anyone to see init's core pattern.
				 * Restrict all others to the owner or root.
				 */
				if (p->p_pid != 1 &&
				    !hasprocperm(p->p_cred, CRED()))
					error = EPERM;
				else if ((rp = p->p_corefile) != NULL)
					refstr_hold(rp);
				mutex_exit(&p->p_crlock);
				mutex_exit(&p->p_lock);
			}
		}
		if (rp == NULL) {
			if (error == 0 && suword8((void *)arg1, 0))
				error = EFAULT;
		} else {
			error = copyoutstr(refstr_value(rp), (char *)arg1,
					(size_t)arg2, NULL);
			refstr_rele(rp);
		}
		break;

	case CC_SET_GLOBAL_PATH:
		if (!suser(CRED())) {
			error = EPERM;
			break;
		}
		/* FALLTHROUGH */
	case CC_SET_PROCESS_PATH:
		size = MIN((size_t)arg2, MAXPATHLEN);
		path = kmem_alloc(size, KM_SLEEP);
		error = copyinstr((char *)arg1, path, size, NULL);
		if (error == 0) {
			refstr_t *nrp = refstr_alloc(path);
			if (subcode == CC_SET_PROCESS_PATH)
				error = set_proc_path((pid_t)arg3, nrp);
			else if (*path != '\0' && *path != '/')
				error = EINVAL;
			else {
				mutex_enter(&core_lock);
				rp = core_file;
				if (*path == '\0')
					core_file = NULL;
				else
					refstr_hold(core_file = nrp);
				mutex_exit(&core_lock);
				if (rp != NULL)
					refstr_rele(rp);
			}
			refstr_rele(nrp);
		}
		kmem_free(path, size);
		break;

	default:
		error = EINVAL;
		break;
	}

	if (error)
		return (set_errno(error));
	return (0);
}

typedef struct {
	int count;
	refstr_t *rp;
} counter_t;

static int
set_one_proc_path(proc_t *p, counter_t *counterp)
{
	refstr_t *rp;

	mutex_enter(&p->p_crlock);
	if ((p->p_flag & SSYS) || !hasprocperm(p->p_cred, CRED()))
		mutex_exit(&p->p_crlock);
	else {
		mutex_exit(&p->p_crlock);
		counterp->count++;
		refstr_hold(counterp->rp);
		mutex_enter(&p->p_lock);
		rp = p->p_corefile;
		p->p_corefile = counterp->rp;
		mutex_exit(&p->p_lock);
		if (rp != NULL)
			refstr_rele(rp);
	}

	return (0);
}

static int
set_proc_path(pid_t pid, refstr_t *rp)
{
	proc_t *p;
	counter_t counter;
	int error = 0;

	counter.count = 0;
	counter.rp = rp;

	if (pid == -1) {
		procset_t set;

		setprocset(&set, POP_AND, P_ALL, P_MYID, P_ALL, P_MYID);
		error = dotoprocs(&set, set_one_proc_path, (char *)&counter);
		if (error == 0 && counter.count == 0)
			error = EPERM;
	} else if (pid > 0) {
		mutex_enter(&pidlock);
		if ((p = prfind(pid)) == NULL || p->p_stat == SIDL)
			error = ESRCH;
		else {
			(void) set_one_proc_path(p, &counter);
			if (counter.count == 0)
				error = EPERM;
		}
		mutex_exit(&pidlock);
	} else {
		int nfound = 0;
		pid_t pgid;

		if (pid == 0)
			pgid = curproc->p_pgrp;
		else
			pgid = -pid;

		mutex_enter(&pidlock);
		for (p = pgfind(pgid); p != NULL; p = p->p_pglink) {
			if (p->p_stat != SIDL) {
				nfound++;
				(void) set_one_proc_path(p, &counter);
			}
		}
		mutex_exit(&pidlock);
		if (nfound == 0)
			error = ESRCH;
		else if (counter.count == 0)
			error = EPERM;
	}

	if (error)
		return (set_errno(error));
	return (0);
}
