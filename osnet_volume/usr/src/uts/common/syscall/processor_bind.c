/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)processor_bind.c	1.4	97/05/22 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/var.h>
#include <sys/thread.h>
#include <sys/cpuvar.h>
#include <sys/kstat.h>
#include <sys/uadmin.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/procset.h>
#include <sys/processor.h>
#include <sys/debug.h>
#include <sys/cpupart.h>

/*
 * processor_bind(2) - Processor binding interfaces.
 */

int
processor_bind(idtype_t idtype, id_t id, processorid_t cpun,
			processorid_t *obind)
{
	struct bind_arg	ba;
	int		err = 0;
	cpu_t		*cp;
	proc_t		*pp;
	kthread_id_t	tp;

	ba.bind = cpun;			/* args for bind_thread */
	ba.obind = PBIND_NONE;
	ba.err = 0;

	/*
	 * Grab the cpu partition lock in case we need to change
	 * partitions.  We might need to do this if we're binding
	 * a thread to a cpu in a different system partition.
	 */
	mutex_enter(&cp_list_lock);

	/*
	 * Since we might be making a binding to a processor, hold the
	 * cpu_lock so that the processor cannot be taken offline while
	 * we do this.
	 */
	mutex_enter(&cpu_lock);

	/*
	 * Check to be sure binding processor ID is valid.
	 */
	switch (cpun) {
	default:
		if ((cp = cpu_get(cpun)) == NULL ||
		    (cp->cpu_flags & (CPU_QUIESCED | CPU_OFFLINE)))
			err = EINVAL;
		else if ((cp->cpu_flags & CPU_READY) == 0)
			err = EIO;
		break;

	case PBIND_NONE:
	case PBIND_QUERY:
		break;
	}

	if (err) {
		mutex_exit(&cpu_lock);
		mutex_exit(&cp_list_lock);
		return (set_errno(err));
	}

	switch (idtype) {
	case P_LWPID:
		pp = curproc;
		mutex_enter(&pp->p_lock);
		if (id == P_MYID) {
			err = cpu_bind_thread(curthread, &ba);
		} else {
			int	found = 0;

			tp = pp->p_tlist;
			do {
				if (tp->t_tid == id) {
					err = cpu_bind_thread(tp, &ba);
					found = 1;
					break;
				}
			} while ((tp = tp->t_forw) != pp->p_tlist);
			if (!found)
				err = set_errno(ESRCH);
		}
		mutex_exit(&pp->p_lock);
		break;

	case P_PID:
		/*
		 * Note.  Cannot use dotoprocs here because it doesn't find
		 * system class processes, which are legal to query.
		 */
		mutex_enter(&pidlock);
		if (id == P_MYID) {
			err = cpu_bind_process(curproc, &ba);
		} else if ((pp = prfind(id)) != NULL) {
			err = cpu_bind_process(pp, &ba);
		} else {
			err = set_errno(ESRCH);
		}
		mutex_exit(&pidlock);
		break;

	default:
		/*
		 * Spec says this is invalid, even though dotoprocs could
		 * handle other idtypes.
		 */
		err = set_errno(EINVAL);
		break;
	}
	mutex_exit(&cpu_lock);
	mutex_exit(&cp_list_lock);

	/*
	 * If no search error occurred, see if any permissions errors did.
	 */
	if (err == 0 && ba.err != 0) {
		err = set_errno(ba.err);
	}

	if (err == 0 && obind != NULL)
		if (copyout((caddr_t)&ba.obind, (caddr_t)obind,
		    sizeof (ba.obind)) == -1)
			err = set_errno(EFAULT);
	return (err);			/* return success or failure */
}
