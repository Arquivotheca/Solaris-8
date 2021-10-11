/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)td_so.c	1.59	99/08/10 SMI"

/*
*
* Description:
* This module contains the functions that interact with the
* synchronization variables(locks, semaphores, r/w locks, and
* events).
*/

#pragma weak td_ta_map_addr2sync = __td_ta_map_addr2sync
#pragma weak td_sync_waiters = __td_sync_waiters
#pragma weak td_sync_setstate = __td_sync_setstate
#pragma weak td_sync_get_info = __td_sync_get_info

#include <thread_db.h>
#include "td.h"
#include "td_so.h"
#include "td_po.h"

/*
* Description:
*   Given an address to a synchronization variable,
* convert it to a synchronization handle.
*
* Input:
*   *ta_p - thread agent
*   addr - address of synchronization primitive data structure
*
* Output:
*   *sh_p - synchronization handle corresponding to
* synchronization primitive
*   td_ta_map_addr2sync - return value
*
* Side effects:
*   none
*   Imported functions called: none
*
*/
td_err_e
__td_ta_map_addr2sync(const td_thragent_t *ta_p, psaddr_t addr,
	td_synchandle_t *sh_p)
{

	uint16_t sync_magic;

	/*
	 * Sanity checks.
	 */
	if (ta_p == NULL) {
		return (TD_BADTA);
	}
	if (ta_p->ph_p == NULL) {
		return (TD_BADPH);
	}
	if (sh_p == NULL) {
		return (TD_BADSH);
	}
	if (addr == NULL) {
		return (TD_ERR);
	}

	/*
	 * Check the magic number of the sync. object to make sure it's
	 * valid.  The magic number is at the same offset for all
	 * sync. objects.
	 */
	if (ps_pdread(ta_p->ph_p,
	    (psaddr_t) &((mutex_t *) addr)->mutex_magic,
	    (caddr_t) &sync_magic, sizeof (sync_magic)) != PS_OK)
		return (TD_BADSH);
	if (sync_magic != MUTEX_MAGIC && sync_magic != COND_MAGIC &&
	    sync_magic != SEMA_MAGIC && sync_magic != RWL_MAGIC)
		return (TD_BADSH);

	/*
	 * Just fill in the appropriate fields of the sync. handle.
	 */

	sh_p->sh_ta_p = (td_thragent_t *) ta_p;
	sh_p->sh_unique = addr;

	return (TD_OK);

}

typedef struct {
	td_thr_iter_f *waiter_cb;
	psaddr_t sync_obj_addr;
	uint16_t sync_magic;
	void *waiter_cb_arg;
	td_err_e errcode;
} waiter_cb_ctl_t;


static int
waiters_cb(const td_thrhandle_t *th_p, void *arg)

{
	register	waiter_cb_ctl_t *wcb = arg;
	uthread_t	thr_struct;
	char		*blockaddr, *rblockaddr, *wblockaddr;
	struct ps_prochandle *ph_p;
	caddr_t		t_wchan;
#ifdef  _SYSCALL32_IMPL
	uthread32_t	thr_struct32;
#endif /* _SYSCALL32_IMPL */
	int		model = PR_MODEL_NATIVE;

	if ((ph_p = th_p->th_ta_p->ph_p) == NULL) {
		return (TD_BADTA);
	}

	if (__td_pdmodel(ph_p, &model) != 0) {
		return (TD_ERR);
	}
	if (model == PR_MODEL_NATIVE) {
		wcb->errcode = __td_read_thr_struct(th_p->th_ta_p,
		    th_p->th_unique, &thr_struct);
		if (wcb->errcode != TD_OK)
			return (1);
		if (thr_struct.t_state != TS_SLEEP) {
			/*
			 * "Can't happen", because we filtered on this in
			 * td_ta_thr_iter, but check anyway.
			 */
			return (0);
		}
		t_wchan = thr_struct.t_wchan;
	}
#ifdef  _SYSCALL32_IMPL
	else {
		wcb->errcode = __td_read_thr_struct32(th_p->th_ta_p,
		    th_p->th_unique, &thr_struct32);
		if (wcb->errcode != TD_OK)
			return (1);
		if (thr_struct32.t_state != TS_SLEEP) {
			/*
			 * "Can't happen", because we filtered on this in
			 * td_ta_thr_iter, but check anyway.
			 */
			return (0);
		}
		t_wchan = (caddr_t)thr_struct32.t_wchan;
	}
#endif /* _SYSCALL32_IMPL */

	switch (wcb->sync_magic) {
	case MUTEX_MAGIC:
	case COND_MAGIC:
		if (t_wchan == (char *) wcb->sync_obj_addr)
			return ((*wcb->waiter_cb)(th_p, wcb->waiter_cb_arg));
		break;
	case SEMA_MAGIC:
		blockaddr = (char *) ((sema_t *) wcb->sync_obj_addr)->pad2;
		if (t_wchan == blockaddr)
			return ((*wcb->waiter_cb)(th_p, wcb->waiter_cb_arg));
		break;
	case RWL_MAGIC:
		rblockaddr = (char *) ((rwlock_t *) wcb->sync_obj_addr)->pad2;
		wblockaddr = (char *) ((rwlock_t *) wcb->sync_obj_addr)->pad3;
		if (t_wchan == rblockaddr ||
		    t_wchan == wblockaddr)
			return ((*wcb->waiter_cb)(th_p, wcb->waiter_cb_arg));
		break;
	default:
		/*
		 * This shouldn't happen, but don't do anything about it
		 * here.
		 */
		break;
	}
	return (0);
}


/*
*
* Description:
*   For a given synchronization variable, iterate over the
* set of waiting threads. The call back function is passed
* two parameters, a pointer to a thread handle and a pointer
* to extra call back data that can be NULL. If the return
* value for cb is non-zero, the iterations terminate.
*
* Input:
*   *sh_p - synchronization handle
*   cb - call back function called on each thread waiting
* on synchronization variable.
*   cb_data_p - data pointer passed to cb
*
* Output:
*   td_sync_waiters - return value
*
* Side effects:
*   cb is called once for each thread waiting on the synchronization
* variable and is passed the thread handle for the thread and cb_data_p.
*   Imported functions called:
*
*/
td_err_e
__td_sync_waiters(const td_synchandle_t *sh_p,
		td_thr_iter_f * cb, void *cb_data_p)
{
	waiter_cb_ctl_t wcb;
	td_err_e return_val;

#ifdef TEST_PS_CALLS
	if (td_noop) {
		return (TD_OK);
	}
#endif

	/*
	 * Sanity checks.
	 */
	if (sh_p == NULL) {
		return (TD_BADSH);
	}
	if (sh_p->sh_ta_p == NULL) {
		return (TD_BADTA);
	}
	if (sh_p->sh_unique == NULL) {
		return (TD_BADSH);
	}

	if (ps_pdread(sh_p->sh_ta_p->ph_p,
	    (psaddr_t) &((mutex_t *) sh_p->sh_unique)->mutex_magic,
	    (caddr_t) &wcb.sync_magic, sizeof (wcb.sync_magic)) != PS_OK)
		return (TD_BADSH);
	wcb.waiter_cb = cb;
	wcb.sync_obj_addr = sh_p->sh_unique;
	wcb.waiter_cb_arg = cb_data_p;
	wcb.errcode = TD_OK;
	return_val = __td_ta_thr_iter(sh_p->sh_ta_p,
		waiters_cb, &wcb,
		TD_THR_SLEEP, TD_THR_LOWEST_PRIORITY,
		TD_SIGNO_MASK, TD_THR_ANY_USER_FLAGS);
	if (return_val != TD_OK)
		return (return_val);

	return (wcb.errcode);
}




/*
* Description:
*    Change the state of a synchronization variable.
* 	1) mutex lock state set to value
* 	2) semaphore's count set to value
* 	3) writer's lock set to value
* 	4) reader's lock number of readers set to value
*
* Input:
*   *sh_p - synchronization handle
*   value - new value of state of synchronization
* variable
*
* Output:
*   td_sync_setstate - return value
*
* Side effects:
*   State of synchronization variable is changed.
*   Imported functions called: ps_pdread, ps_pdwrite
*/
td_err_e
__td_sync_setstate(const td_synchandle_t *sh_p, long value)
{
	td_err_e	return_val;
	ps_err_e	db_return;
	td_so_un_t	generic_so;

	if (sh_p == NULL) {
		return (TD_BADSH);
	}
	if (sh_p->sh_ta_p == NULL) {
		return (TD_BADTA);
	}
	if (sh_p->sh_unique == NULL) {
		return (TD_BADSH);
	}

	if (rw_rdlock(&sh_p->sh_ta_p->rwlock))
		return (TD_ERR);
	if (ps_pstop(sh_p->sh_ta_p->ph_p) != PS_OK)
		return (TD_DBERR);

	/*
	 * Read the synch. variable information.  Read in the size
	 * of a cond. var. first, as it's the smallest sync. object;
	 * then read in the actual size once we know the type.
	 */

	db_return = ps_pdread(sh_p->sh_ta_p->ph_p, sh_p->sh_unique,
		(char *) &generic_so.condition, sizeof (generic_so.condition));

	if (db_return != PS_OK) {
		rw_unlock(&sh_p->sh_ta_p->rwlock);
		return (TD_DBERR);
	}

	/*
	 * Set the new value in the sync. variable, read the synch. variable
	 * information. from the process, reset its value and write it back.
	 */

	switch (generic_so.condition.mutex_magic) {
	case MUTEX_MAGIC:
		if (ps_pdread(sh_p->sh_ta_p->ph_p, sh_p->sh_unique,
		    (char *) &generic_so.lock, sizeof (generic_so.lock))
		    != PS_OK) {
			return_val = TD_DBERR;
			break;
		}
		generic_so.lock.mutex_lockw = value;
		if (ps_pdwrite(sh_p->sh_ta_p->ph_p, sh_p->sh_unique,
		    (char *) &generic_so.lock,
		    sizeof (generic_so.lock)) != PS_OK)
			return_val = TD_DBERR;
		break;
	case SEMA_MAGIC:
		if (ps_pdread(sh_p->sh_ta_p->ph_p, sh_p->sh_unique,
		    (char *) &generic_so.semaphore,
		    sizeof (generic_so.semaphore)) != PS_OK) {
			return_val = TD_DBERR;
			break;
		}
		generic_so.semaphore.count = value;
		db_return = ps_pdwrite(sh_p->sh_ta_p->ph_p, sh_p->sh_unique,
		    (char *) &generic_so.semaphore,
		    sizeof (generic_so.semaphore));
		if (db_return != PS_OK)
			return_val = TD_DBERR;
		break;
	case COND_MAGIC:
		/* Operation not supported on a condition variable */
		return_val = TD_ERR;
		break;
	case RWL_MAGIC:
		if (ps_pdread(sh_p->sh_ta_p->ph_p, sh_p->sh_unique,
		    (char *) &generic_so.rwlock,
		    sizeof (generic_so.rwlock)) != PS_OK) {
			return_val = TD_DBERR;
			break;
		}
		generic_so.rwlock.readers = value;
		db_return = ps_pdwrite(sh_p->sh_ta_p->ph_p, sh_p->sh_unique,
		    (char *) &generic_so.rwlock, sizeof (generic_so.rwlock));
		if (db_return != PS_OK)
			return_val = TD_DBERR;
		break;
	default:
		/* Bad sync. object type */
		return_val = TD_BADSH;
		__td_report_po_err(return_val,
			"Unknown Sync. variable type - td_sync_setstate");
		break;
	}
	if (ps_pcontinue(sh_p->sh_ta_p->ph_p) != PS_OK)
		return_val = TD_DBERR;

	rw_unlock(&sh_p->sh_ta_p->rwlock);
	return (return_val);
}



/*
* Description:
*   Given an synchronization handle, fill in the
* information for the synchronization variable into *si_p.
*
* Input:
*   *sh_p - synchronization handle
*
* Output:
*   *si_p - synchronization information structure corresponding to
* synchronization handle
*   td_sync_get_info - return value
*
* Side effects:
*   none
*   Imported functions called:
*/
td_err_e
__td_sync_get_info(const td_synchandle_t *sh_p, td_syncinfo_t *si_p)

{
	td_err_e ret_val = TD_OK;
	struct ps_prochandle *ph_p;
	ps_err_e ps_ret;
	td_so_un_t generic_so;

#ifdef TEST_PS_CALLS
	if (td_noop) {
		return (TD_OK);
	}
#endif

	/*
	 * Sanity checks.
	 */
	if (sh_p == NULL || sh_p->sh_unique == NULL) {
		return (TD_BADSH);
	}
	if (si_p == NULL) {
		return (TD_ERR);
	}
	if (sh_p->sh_ta_p == NULL) {
		return (TD_BADTA);
	}

	/*
	 * Get a reader lock.
	 */
	if (rw_rdlock(&(sh_p->sh_ta_p->rwlock))) {
		return (TD_ERR);
	}

	if ((ph_p = sh_p->sh_ta_p->ph_p) == NULL || ps_pstop(ph_p) != PS_OK) {
		rw_unlock(&(sh_p->sh_ta_p->rwlock));
		return (TD_BADTA);
	}

	/* Determine the sync. object type; a little type fudgery here. */
	/* A cond. var. is the smallest sync. object. */
	ps_ret = ps_pdread(ph_p, sh_p->sh_unique,
	    (char *) &generic_so.condition, sizeof (generic_so.condition));
	if (ps_ret != PS_OK) {
		rw_unlock(&(sh_p->sh_ta_p->rwlock));
		return (TD_DBERR);
	}
	si_p->si_ta_p = sh_p->sh_ta_p;
	si_p->si_sv_addr = sh_p->sh_unique;

	switch (generic_so.condition.cond_magic) {
	case MUTEX_MAGIC:
		si_p->si_type = TD_SYNC_MUTEX;
		ps_ret = ps_pdread(ph_p, sh_p->sh_unique,
		    (char *) &generic_so.lock, sizeof (generic_so.lock));
		if (ps_ret != PS_OK) {
			rw_unlock(&sh_p->sh_ta_p->rwlock);
			return (TD_DBERR);
		}
		si_p->si_shared_type = generic_so.lock.mutex_type;
		(void) memcpy(si_p->si_flags, &generic_so.lock.mutex_flag,
		    sizeof (generic_so.lock.mutex_flag));
		si_p->si_state.mutex_locked =
		    (generic_so.lock.mutex_lockw != 0);
		si_p->si_size = sizeof (generic_so.lock);
		si_p->si_has_waiters = generic_so.lock.mutex_waiters;
		si_p->si_owner.th_ta_p = sh_p->sh_ta_p;
		si_p->si_owner.th_unique = generic_so.lock.mutex_owner;
		si_p->si_data = NULL;
		break;
	case COND_MAGIC:
		si_p->si_type = TD_SYNC_COND;
		si_p->si_shared_type = generic_so.condition.cond_type;
		memcpy(si_p->si_flags, generic_so.condition.flags.flag,
		    sizeof (generic_so.condition.flags.flag));
		si_p->si_size = sizeof (generic_so.condition);
		si_p->si_has_waiters = generic_so.condition.cond_waiters;
		si_p->si_data = NULL;
		break;
	case SEMA_MAGIC:
		si_p->si_type = TD_SYNC_SEMA;
		ps_ret = ps_pdread(ph_p, sh_p->sh_unique,
		    (char *) &generic_so.semaphore,
		    sizeof (generic_so.semaphore));
		if (ps_ret != PS_OK) {
			rw_unlock(&sh_p->sh_ta_p->rwlock);
			return (TD_DBERR);
		}
		si_p->si_shared_type = generic_so.semaphore.type;
		si_p->si_state.sem_count = generic_so.semaphore.count;
		memset(si_p->si_flags, 0, sizeof (si_p->si_flags));
		si_p->si_size = sizeof (generic_so.semaphore);
		si_p->si_has_waiters =
		    ((cond_t *) generic_so.semaphore.pad2)->cond_waiters;
		si_p->si_data = (psaddr_t) generic_so.semaphore.count;
		break;
	case RWL_MAGIC:
		si_p->si_type = TD_SYNC_RWLOCK;
		ps_ret = ps_pdread(ph_p, sh_p->sh_unique,
		    (char *) &generic_so.rwlock,
		    sizeof (generic_so.rwlock));
		if (ps_ret != PS_OK) {
			rw_unlock(&sh_p->sh_ta_p->rwlock);
			return (TD_DBERR);
		}
		si_p->si_shared_type = generic_so.rwlock.type;
		si_p->si_state.nreaders = generic_so.rwlock.readers;
		memset(si_p->si_flags, 0, sizeof (si_p->si_flags));
		si_p->si_size = sizeof (generic_so.rwlock);
		si_p->si_has_waiters =
		    ((cond_t *) generic_so.rwlock.pad2)->cond_waiters ||
		    ((cond_t *) generic_so.rwlock.pad3)->cond_waiters;
		si_p->si_data = (psaddr_t) generic_so.rwlock.readers;
		break;
	default:
		ret_val = TD_BADSH;
		break;
	}

	if (ps_pcontinue(ph_p) != PS_OK)
		ret_val = TD_DBERR;
	rw_unlock(&(sh_p->sh_ta_p->rwlock));
	return (ret_val);

}

#ifdef TD_INTERNAL_TESTS


/*
* Description:
*   Dump the contents of the synchronization information struct
*
* Input:
*   *si_p - synchronization information struct
*
* Output:
*   none
*
* Side effects:
*/
void
__td_si_dump(const td_syncinfo_t *si_p)
{

	diag_print("Synchronization variable:\n");
	diag_print("  Address:			%x\n", si_p->si_sv_addr);
	diag_print("  Type:				%s\n",
		td_sync_type_names[si_p->si_type]);
	diag_print("  Flags:				%x\n",
		si_p->si_flags);
	switch (si_p->si_type) {
	case TD_SYNC_MUTEX:
		diag_print("  Mutex locked:			%d\n",
			si_p->si_state.mutex_locked);
		break;
	case TD_SYNC_SEMA:
		diag_print("  Semaphore count:		%d\n",
			si_p->sem_count);
		break;
	case TD_SYNC_RWLOCK:
		diag_print("  Reader count:		%d\n",
			si_p->si_state.nreaders);
		break;
	default:
		diag_print("  BAD SO TYPE - BAD SO TYPE - BAD SO TYPE\n");
	}
	diag_print("  Size:				%x\n",
		si_p->si_size);
	diag_print("  Has waiters:			%d\n",
		si_p->si_has_waiters);
	diag_print("  Thread owner struct addr:	%d\n",
		si_p->si_owner.th_unique);
	diag_print("  Optional data ptr:		%x\n",
		si_p->si_data);

	diag_print("  Statistics:\n");
	diag_print("    Waiters:			%d\n",
		si_p->si_stats.waiters);
	diag_print("    Contention:			%d\n",
		si_p->si_stats.contention);
	diag_print("    Acquires:			%d\n",
		si_p->si_stats.acquires);
	diag_print("    Wait time:			%d\n",
		si_p->si_stats.waittime);

}
#endif
