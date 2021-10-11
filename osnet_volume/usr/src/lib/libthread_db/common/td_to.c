/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)td_to.c	1.88	99/08/10 SMI"

/*
* Description:
*	This module contains functions for interacting with
* the threads within the program.
*/

#pragma weak td_thr_validate = __td_thr_validate  /* i386 work around */
#pragma weak td_thr_tsd = __td_thr_tsd  /* i386 work around */
#pragma weak td_thr_get_info = __td_thr_get_info
#pragma weak td_thr_sigsetmask = __td_thr_sigsetmask  /* i386 work around */
#pragma weak td_thr_setprio = __td_thr_setprio  /* i386 work around */
#pragma weak td_thr_setsigpending = __td_thr_setsigpending /* i386work around */

#pragma weak td_thr_lockowner = __td_thr_lockowner
#pragma weak td_thr_sleepinfo = __td_thr_sleepinfo
#pragma weak td_thr_dbsuspend = __td_thr_dbsuspend
#pragma weak td_thr_dbresume = __td_thr_dbresume

#pragma weak td_ta_map_id2thr = __td_ta_map_id2thr

#pragma weak td_thr_getfpregs = __td_thr_getfpregs  /* i386 work around */
#pragma weak td_thr_setfpregs = __td_thr_setfpregs  /* i386 work around */

#define	V8PLUS_SUPPORT

#include <thread_db.h>
#include "td.h"
#include "td_to_impl.h"
#include "xtd_to.h"
#include "td.extdcl.h"
#include "xtd.extdcl.h"
#include "td.pubdcl.h"

struct td_mapper_param {
	thread_t	tid;
	int		found;
	td_thrhandle_t	th;
};
typedef struct td_mapper_param td_mapper_param_t;

struct searcher {
	uintptr_t	addr;
	int		status;
};

static td_err_e
td_read_thread_tsd(const td_thrhandle_t *th_p, int model, tsd_t *thr_tsd_p,
	struct tsd_common * tsd_common_p);
static td_err_e
td_thr2to_const(td_thragent_t *ta_p, psaddr_t ts_addr,
	uthread_t * thr_struct_p, td_thrinfo_t *ti_p);
static td_err_e
td_thr2to_var(uthread_t * thr_struct_p, td_thrinfo_t *ti_p);
static td_err_e
td_thr2to(td_thragent_t *ta_p, psaddr_t ts_addr, uthread_t * thr_struct_p,
	td_thrinfo_t *ti_p);
static int
td_searcher(const td_thrhandle_t *th_p, void *data);
static int
td_mapper_id2thr(const td_thrhandle_t *th_p, td_mapper_param_t *data);
#ifdef TD_INTERNAL_TESTS
static void
td_siginfo_dump(const siginfo_t * sig_p);
#endif

/* Macros used in calls that would otherwise be > 80 characters. */
#define	TDT_M1 "malloc() failed - td_read_thread_tsd"
#define	TDT_M2 "Writing FP information: td_thr_getfpregs"

/*
* Description:    Read thread specific data information.
*
* Input:
*   *th_p - thread handle
*
* Output:
*   *thr_tsd_p - tsd count and address of array of tsd keys
*   *tsd_common - common tsd information
*   td_read_thread_tsd - return value
*
* Side effects:
*
* Notes:
*   In libthread, tsd_thread may be #define'ed to
* a reference to "t_tls" in the current threads thread struct.
*/
static	td_err_e
td_read_thread_tsd(const td_thrhandle_t *th_p, int model, tsd_t *thr_tsd_p,
	struct tsd_common * tsd_common_p)
{
	td_err_e	return_val;
	psaddr_t	array_addr;
	struct ps_prochandle	*ph_p;
	td_thragent_t	*ta_p;
	psaddr_t	t_tls;

	ta_p = th_p->th_ta_p;
	ph_p = ta_p->ph_p;

	/*
	 * Extract the thread struct address from the thread handle and read
	 * the thread struct.
	 */

#ifdef TLS

	/*
	 * I have not covered this option because I don't know that it really
	 * is an option.  But if it is, this will not compile.
	 */
	? ? ? ? ? ? ? ? ? ?
#endif

	if (model == PR_MODEL_NATIVE) {
		uthread_t thr_struct;
		return_val = __td_read_thr_struct(ta_p, th_p->th_unique,
		    &thr_struct);
		t_tls = (psaddr_t)thr_struct.t_tls;
	} else {
#ifdef  _SYSCALL32_IMPL
		uthread32_t thr_struct32;
		return_val = __td_read_thr_struct32(ta_p, th_p->th_unique,
		    &thr_struct32);
		t_tls = (psaddr_t)thr_struct32.t_tls;
#else
		return_val = TD_ERR;
#endif /* _SYSCALL32_IMPL */
	}

	/*
	 * tsd_thread holds the TSD count and a pointer array to TSD
	 * data for a thread. tsd_common holds information about
	 * number of keys used.
	 */
	*tsd_common_p = NULL_TSD_COMMON;
	*thr_tsd_p = NULL_TSD_T;

	/*
	 * The pointer to thread tsd is in t_tls field of a thread
	 * struct. If t_tls is NULL, there is no thread specific data
	 * for this thread.
	 */
	if (return_val == TD_OK && t_tls == NULL)
		return_val = TD_NOTSD;
	if (return_val != TD_OK)
		return (return_val);

	if (model == PR_MODEL_NATIVE) {
		if (ps_pdread(ph_p, (psaddr_t)t_tls,
		    thr_tsd_p, sizeof (*thr_tsd_p)) != PS_OK)
			return_val = TD_DBERR;
		else if (ps_pdread(ph_p,
		    (psaddr_t)ta_p->tdb_invar.tsd_common_addr,
		    tsd_common_p, sizeof (*tsd_common_p)) != PS_OK)
			return_val = TD_DBERR;
	} else {
#ifdef  _SYSCALL32_IMPL
		tsd32_t tsd32;
		struct tsd_common32 tsd_common32;

		if (ps_pdread(ph_p, (psaddr_t)t_tls,
		    &tsd32, sizeof (tsd32)) != PS_OK)
			return_val = TD_DBERR;
		else if (ps_pdread(ph_p,
		    (psaddr_t)ta_p->tdb_invar32.tsd_common_addr,
		    &tsd_common32, sizeof (tsd_common32)) != PS_OK)
			return_val = TD_DBERR;
		else {
			thr_tsd_p->count = tsd32.count;
			thr_tsd_p->array = (void **)tsd32.array;
			tsd_common_p->nkeys = tsd_common32.nkeys;
			tsd_common_p->max_keys = tsd_common32.max_keys;
			tsd_common_p->destructors =
				(PFrV *)tsd_common32.destructors;
			tsd_common_p->lock = tsd_common32.lock;
		}
#else
		return_val = TD_ERR;
#endif /* _SYSCALL32_IMPL */
	}

	return (return_val);
}


/*
* Description:
*   Check the struct thread address in *th_p again first
* value in "data".  If value in data is found, set second value
* in "data" to TRUE and return 1 to terminate iterations.
*   This function is used by td_thr_validate() to verify that
* a thread handle is valid.
*
* Input:
*   *th_p - thread handle
*   data[0] - struct thread address being sought
*	flag indicating struct thread address found
*
* Output:
*   td_searcher - returns 1 if thread struct address found
*   data[1] - flag indicating struct thread address found
*
* Side effects:
*/
static int
td_searcher(const td_thrhandle_t *th_p, void *data)
{
	int	return_val = 0;

	if (((int *) data)[0] == th_p->th_unique) {
		((int *) data)[1] = TRUE;
		return_val = 1;
	}
	return (return_val);
}


/*
* Description:
*   Validate the thread handle.  Check that
* a thread exists in the thread agent/process that
* corresponds to thread with handle *th_p.
*
* Input:
*   *th_p - thread handle
*
* Output:
*   td_thr_validate - return value
*	return value == TD_OK implies thread handle is valid
* 	return value == TD_NOTHR implies thread handle not valid
* 	return value == other implies error
*
* Side effects:
*   none
*   Imported functions called:
* ps_pdread, ps_pstop, ps_pcontinue.
*
*/
td_err_e
__td_thr_validate(const td_thrhandle_t *th_p)
{
	td_err_e	return_val = TD_OK;
	struct searcher	searcher_data = {0, FALSE};

#ifdef TEST_PS_CALLS
	if (td_noop) {
		(void) ps_pstop(0);

		(void) ps_pdread(0, 0, 0, 0);
		(void) ps_pcontinue(0);
		return (TD_OK);
	}
#endif

	/*
	 * check for valid thread handle pointer
	 */
	if (th_p == NULL) {
		return_val = TD_BADTH;
		return (return_val);
	}

	/*
	 * Check for valid thread handle - check for NULLs
	 */
	if ((th_p->th_unique == NULL) || (th_p->th_ta_p == NULL)) {
		return_val = TD_BADTH;
		return (return_val);
	}

	/*
	 * LOCKING EXCEPTION - Locking is not required
	 * here because no use of the thread agent is made (other
	 * than the sanity check) and checking of the thread
	 * agent will be
	 * done in __td_ta_thr_iter.  If __td_ta_thr_iter
	 * is not used or if some use of the thread agent
	 * is made other than the sanity checks, ADD
	 * locking.
	 */

	/*
	 * Use thread iterator.
	 */
	searcher_data.addr = th_p->th_unique;
	return_val = __td_ta_thr_iter(th_p->th_ta_p,
		td_searcher, &searcher_data,
		TD_THR_ANY_STATE, TD_THR_LOWEST_PRIORITY,
		TD_SIGNO_MASK, TD_THR_ANY_USER_FLAGS);

	if (return_val != TD_OK) {
		__td_report_po_err(return_val,
			"Iterator failed in td_thr_validate()");
	} else {
		if (searcher_data.status == TRUE) {
			return_val = TD_OK;
		} else {
			return_val = TD_NOTHR;
		}
	}

	return (return_val);

}

/*
* Description:
*   Get a thread's  private binding to a given thread specific
* data(TSD) key(see thr_getspecific(3T).  If the thread doesn't
* have a binding for a particular key, then NULL is returned.
*
* Input:
*   *th_p - thread handle
*   key - TSD key
*
* Output:
*   data_pp - key value for given thread. This value
*	is typically a pointer.  It is NIL if
*	there is no TSD data for this thread.
*   td_thr_tsd - return value
*
* Side effects:
*   none
*   Imported functions called: ps_pglobal_lookup,
* ps_pdread, ps_pstop, ps_pcontinue.
*/
td_err_e
__td_thr_tsd(const td_thrhandle_t *th_p, const thread_key_t key,
	void **data_pp)
{
	struct ps_prochandle *ph_p;
	tsd_t tsd_thread = NULL_TSD_T;
	struct tsd_common tsd_common = NULL_TSD_COMMON;
	td_err_e	return_val = TD_OK;
	int		model;

	/*
	 * I followed the code for libthread thr_getspecific().
	 */
#ifdef TEST_PS_CALLS
	if (td_noop) {
		(void) ps_pstop(0);
		(void) ps_pcontinue(0);
		(void) ps_pglobal_lookup(0, 0, 0, 0);
		(void) ps_pdread(0, 0, 0, 0);
		return (TD_OK);
	}
#endif

	/*
	 * Sanity checks.
	 */
	if (data_pp == NULL)
		return (TD_ERR);
	*data_pp = NULL;

	if (th_p == NULL) {
		return (TD_BADTH);
	}
	if ((th_p->th_ta_p == NULL) || (th_p->th_unique == NULL)) {
		return (TD_BADTA);
	}

	/*
	 * Get a reader lock.
	 */
	if (rw_rdlock(&(th_p->th_ta_p->rwlock))) {
		return (TD_ERR);
	}

	if ((ph_p = th_p->th_ta_p->ph_p) == NULL) {
		rw_unlock(&(th_p->th_ta_p->rwlock));
		return (TD_BADTA);
	}

	if (__td_pdmodel(ph_p, &model) != 0) {
		rw_unlock(&(th_p->th_ta_p->rwlock));
		return (TD_ERR);
	}

	/*
	 * tsd_thread holds the TSD count and an array of pointers to TSD
	 * data. tsd_common holds information about number of keys used.
	 */

	if (key == 0) {
		rw_unlock(&(th_p->th_ta_p->rwlock));
		return (TD_BADKEY);
	} else {

		/*
		 * More than 1 byte is being read.  Stop the process.
		 */
		if (ps_pstop(ph_p) == PS_OK) {

			/*
			 * Read struct containing TSD count and pointer to
			 * storage.
			 */
			return_val = td_read_thread_tsd(th_p, model,
				&tsd_thread, &tsd_common);

			if (return_val == TD_OK) {

				/*
				 * If key is greater than TSD count but less
				 * than TSD nkeys, then request is valid but
				 * there is no data.  If key is greater than
				 * TSD nkeys, then the request is invalid.
				 */
				if (key > tsd_thread.count) {
					if (key > tsd_common.nkeys) {
						return_val = TD_BADKEY;
						__td_report_po_err(return_val,
						"TSD common - td_thr_tsd");
					}
				} else if (model == PR_MODEL_NATIVE) {
					/*
					 * Read the TSD for this thread
					 * from the array of TSD.
					 */
					void *value;
					psaddr_t psaddr =
						(psaddr_t)tsd_thread.array +
						(key - 1) * sizeof (value);

					if (ps_pdread(ph_p, psaddr,
					    &value, sizeof (value)) != PS_OK)
						return_val = TD_DBERR;
					else
						*data_pp = value;
				} else {
#ifdef _SYSCALL32_IMPL
					caddr32_t value;
					psaddr_t psaddr =
						(psaddr_t)tsd_thread.array +
						(key - 1) * sizeof (value);

					if (ps_pdread(ph_p, psaddr,
					    &value, sizeof (value)) != PS_OK)
						return_val = TD_DBERR;
					else
						*data_pp = (void *)value;
#else
					return_val = TD_ERR;
#endif	/* _SYSCALL32_IMPL */
				}
			}

			/*
			 * Continue process.
			 */

			if (ps_pcontinue(ph_p) != PS_OK) {
				return_val = TD_DBERR;
			}
		}	/* ps_pstop succeeded */
		else {
			return_val = TD_DBERR;
		}

	}	/* key valid  */

	rw_unlock(&(th_p->th_ta_p->rwlock));
	return (return_val);

}





/*
* Description:
*   Write a thread structure.
*
* Input:
*   *ta_p - thread agent
*   thr_addr - address of thread structure in *td_thragent_t
*   *thr_struct_p - thread struct
*
* Output:
*   __td_write_thr_struct - return value
*
* Side effects:
*   Process is written
*/

td_err_e
__td_write_thr_struct(td_thragent_t *ta_p,
	psaddr_t thr_addr, uthread_t * thr_struct_p)
{
	td_err_e	return_val = TD_OK;
	struct ps_prochandle *ph_p;

	ph_p = ta_p->ph_p;

	if (return_val == TD_OK) {
		if (ps_pdwrite(ph_p, thr_addr, (char *) thr_struct_p,
				sizeof (*thr_struct_p)) != PS_OK) {
			return_val = TD_DBERR;
			__td_report_to_err(TD_DBERR,
				"Writing thread struct: __td_write_thr_struct");
		}
	}

	return (return_val);

}


#ifdef  _SYSCALL32_IMPL
/*
* Description:
*   Write a thread structure.
*
* Input:
*   *ta_p - thread agent
*   thr_addr - address of thread structure in *td_thragent_t
*   *thr_struct_p - thread struct
*
* Output:
*   __td_write_thr_struct - return value
*
* Side effects:
*   Process is written
*/

td_err_e
__td_write_thr_struct32(td_thragent_t *ta_p,
	psaddr_t thr_addr, uthread32_t * thr_struct_p)
{
	td_err_e	return_val = TD_OK;
	struct ps_prochandle *ph_p;

	ph_p = ta_p->ph_p;

	if (return_val == TD_OK) {
		if (ps_pdwrite(ph_p, thr_addr, (char *) thr_struct_p,
				sizeof (*thr_struct_p)) != PS_OK) {
			return_val = TD_DBERR;
			__td_report_to_err(TD_DBERR,
				"Writing thread struct: __td_write_thr_struct");
		}
	}

	return (return_val);

}
#endif /* _SYSCALL32_IMPL */

/*
* Description:
*   Read a thread structure.
*
* Input:
*   *ta_p - thread agent
*   thr_addr - address of thread structure in *ta_p
*
* Output:
*   *thr_struct_p - thread struct
*
* Side effects:
*   Process is read
*/
td_err_e
__td_read_thr_struct(const td_thragent_t *ta_p, psaddr_t thr_addr,
	uthread_t * thr_struct_p)
{
	td_err_e	return_val = TD_OK;

	if (ps_pdread(ta_p->ph_p, thr_addr, (char *) thr_struct_p,
			sizeof (*thr_struct_p)) != PS_OK) {
		return_val = TD_DBERR;
		__td_report_to_err(TD_DBERR,
			"Reading thread struct: __td_read_thr_struct");
	}

	return (return_val);

}

#ifdef  _SYSCALL32_IMPL
/*
* Description:
*   Read a thread structure.
*
* Input:
*   *ta_p - thread agent
*   thr_addr - address of thread structure in *ta_p
*
* Output:
*   *thr_struct_p - thread struct
*
* Side effects:
*   Process is read
*/
td_err_e
__td_read_thr_struct32(const td_thragent_t *ta_p, psaddr_t thr_addr,
	uthread32_t * thr_struct_p)
{
	td_err_e	return_val = TD_OK;

	if (ps_pdread(ta_p->ph_p, thr_addr, (char *) thr_struct_p,
			sizeof (*thr_struct_p)) != PS_OK) {
		return_val = TD_DBERR;
		__td_report_to_err(TD_DBERR,
			"Reading thread struct: __td_read_thr_struct");
	}

	return (return_val);

}
#endif /* _SYSCALL32_IMPL */

/*
* Description:
*   Map state from threads struct to thread information states
*
* Input:
*   ts_state - thread struct state
*
* Output:
*   *to_state - thread information state
*
* Side effects:
*   none
*/
td_err_e
__td_thr_map_state(thstate_t ts_state, td_thr_state_e *to_state)
{
	td_err_e	return_val = TD_OK;

	switch (ts_state) {
	case TS_SLEEP:
		*to_state = TD_THR_SLEEP;
		break;
	case TS_RUN:
		*to_state = TD_THR_RUN;
		break;
	case TS_DISP:
		*to_state = TD_THR_ACTIVE;
		break;
	case TS_ONPROC:
		*to_state = TD_THR_ACTIVE;
		break;
	case TS_STOPPED:
		*to_state = TD_THR_STOPPED;
		break;
	case TS_ZOMB:
		*to_state = TD_THR_ZOMBIE;
		break;
#ifdef NEW_LIBTHREAD_SUPPORT
	case TS_STOPPED_ASLEEP:
		*to_state = TD_THR_STOPPED_ASLEEP;
		break;
#endif
	default:
		__td_report_to_err(TD_ERR,
			"Unknown state from thread struct: td_thr_map_state");
		return_val = TD_ERR;
	}

	return (return_val);
}

/*
* Description:
*   Transfer constant information from thread struct to
* thread information struct.
*
* Input:
*   *ta_p - thread agent
*   ts_addr - address of thread struct
*   *thr_struct_p - libthread thread struct
*
* Output:
*   *ti_p - thread information struct
*
* Side effects:
*   none
*/
static td_err_e
td_thr2to_const(td_thragent_t *ta_p, psaddr_t ts_addr,
	uthread_t * thr_struct_p, td_thrinfo_t *ti_p)
{
	td_err_e	return_val = TD_OK;

	/*
	 * Set td_to_ph_p_(*ti_p)
	 */
	ti_p->ti_ta_p = ta_p;

	ti_p->ti_user_flags = thr_struct_p->t_usropts;
	ti_p->ti_tid = thr_struct_p->t_tid;
	ti_p->ti_tls = thr_struct_p->t_tls;
	ti_p->ti_startfunc = (psaddr_t) thr_struct_p->t_startpc;
	ti_p->ti_stkbase = (psaddr_t) thr_struct_p->t_stk;
	ti_p->ti_stksize = thr_struct_p->t_stksize;
	ti_p->ti_flags = thr_struct_p->t_flag;
	ti_p->ti_ro_area = ts_addr;
	ti_p->ti_ro_size = sizeof (uthread_t);

	return (return_val);
}

#ifdef  _SYSCALL32_IMPL
/*
* Description:
*   Transfer constant information from thread struct to
* thread information struct.
*
* Input:
*   *ta_p - thread agent
*   ts_addr - address of thread struct
*   *thr_struct_p - libthread thread struct
*
* Output:
*   *ti_p - thread information struct
*
* Side effects:
*   none
*/
static td_err_e
td_thr2to_const32(td_thragent_t *ta_p, psaddr_t ts_addr,
	uthread32_t * thr_struct_p, td_thrinfo_t *ti_p)
{
	td_err_e	return_val = TD_OK;

	/*
	 * Set td_to_ph_p_(*ti_p)
	 */
	ti_p->ti_ta_p = ta_p;

	ti_p->ti_user_flags = thr_struct_p->t_usropts;
	ti_p->ti_tid = thr_struct_p->t_tid;
	ti_p->ti_tls = (caddr_t) thr_struct_p->t_tls;
	ti_p->ti_startfunc = (psaddr_t) thr_struct_p->t_startpc;
	ti_p->ti_stkbase = (psaddr_t) thr_struct_p->t_stk;
	ti_p->ti_stksize = thr_struct_p->t_stksize;
	ti_p->ti_flags = thr_struct_p->t_flag;
	ti_p->ti_ro_area = ts_addr;
	ti_p->ti_ro_size = sizeof (uthread32_t);

	return (return_val);
}
#endif /* _SYSCALL32_IMPL */

/*
* Description:
*   Transfer variable information from thread struct to
* thread information struct.
*
* Input:
*   *thr_struct_p - libthread thread struct
*
* Output:
*   *ti_p - thread information struct variable part
*
* Side effects:
*   none
*
* Assumptions:
*   Fields not set:
*	to_db_suspended, to_events, to_traceme
*/
static td_err_e
td_thr2to_var(uthread_t * thr_struct_p, td_thrinfo_t *ti_p)
{
	td_err_e	return_val = TD_OK;
	td_thr_state_e	state;

	return_val = __td_thr_map_state(thr_struct_p->t_state, &state);
	ti_p->ti_state = state;

	ti_p->ti_db_suspended = (DBSTOPPED(thr_struct_p) != 0);

	ti_p->ti_type = TD_CONVERT_TYPE(*thr_struct_p);
	ti_p->ti_pc = thr_struct_p->t_pc;
	ti_p->ti_sp = thr_struct_p->t_sp;
	ti_p->ti_pri = thr_struct_p->t_pri;

	/*
	 * Non-Null lwp id is not always provided.  See notes in
	 * td_thr_get_var_info() header.
	 */
	if (ISVALIDLWP(thr_struct_p)) {
		ti_p->ti_lid = thr_struct_p->t_lwpid;
	} else {
		ti_p->ti_lid = 0;
	}
	ti_p->ti_sigmask = thr_struct_p->t_hold;

	/*
	 * td_to_events_( *ti_p ) always set to NULL.
	 * td_to_traceme_( *ti_p ) always set to NULL
	 */
	ti_p->ti_traceme = 0;
	td_event_emptyset(&ti_p->ti_events);

	ti_p->ti_preemptflag = thr_struct_p->t_preempt;

	/*
	 * Set the priority inversion flag in the thread information struct
	 * from the thread struct.
	 */

	/* XXX */
	memset(&(ti_p->ti_pirecflag), 0,
		sizeof (ti_p->ti_pirecflag));


	/*
	 * Set pending signal bits only if t_pending is set.
	 */
	if (thr_struct_p->t_pending) {
		ti_p->ti_pending = thr_struct_p->t_psig;
	} else {
		sigemptyset(&(ti_p->ti_pending));
	}

	return (return_val);
}

#ifdef  _SYSCALL32_IMPL
/*
* Description:
*   Transfer variable information from thread struct to
* thread information struct.
*
* Input:
*   *thr_struct_p - libthread thread struct
*
* Output:
*   *ti_p - thread information struct variable part
*
* Side effects:
*   none
*
* Assumptions:
*   Fields not set:
*	to_db_suspended, to_events, to_traceme
*/
static td_err_e
td_thr2to_var32(uthread32_t * thr_struct_p, td_thrinfo_t *ti_p)
{
	td_err_e	return_val = TD_OK;
	td_thr_state_e	state;

	return_val = __td_thr_map_state(thr_struct_p->t_state, &state);
	ti_p->ti_state = state;

	ti_p->ti_db_suspended = (DBSTOPPED(thr_struct_p) != 0);

	ti_p->ti_type = TD_CONVERT_TYPE(*thr_struct_p);
	ti_p->ti_pc = thr_struct_p->t_pc;
	ti_p->ti_sp = thr_struct_p->t_sp;
	ti_p->ti_pri = thr_struct_p->t_pri;

	/*
	 * Non-Null lwp id is not always provided.  See notes in
	 * td_thr_get_var_info() header.
	 */
	if (ISVALIDLWP(thr_struct_p)) {
		ti_p->ti_lid = thr_struct_p->t_lwpid;
	} else {
		ti_p->ti_lid = 0;
	}
	ti_p->ti_sigmask.__sigbits[0] = thr_struct_p->t_hold.__sigbits[0];
	ti_p->ti_sigmask.__sigbits[1] = thr_struct_p->t_hold.__sigbits[1];
	ti_p->ti_sigmask.__sigbits[2] = thr_struct_p->t_hold.__sigbits[2];
	ti_p->ti_sigmask.__sigbits[3] = thr_struct_p->t_hold.__sigbits[3];

	/*
	 * td_to_events_( *ti_p ) always set to NULL.
	 * td_to_traceme_( *ti_p ) always set to NULL
	 */
	ti_p->ti_traceme = 0;
	td_event_emptyset(&ti_p->ti_events);

	ti_p->ti_preemptflag = thr_struct_p->t_preempt;

	/*
	 * Set the priority inversion flag in the thread information struct
	 * from the thread struct.
	 */

	/* XXX */
	memset(&(ti_p->ti_pirecflag), 0,
		sizeof (ti_p->ti_pirecflag));


	/*
	 * Set pending signal bits only if t_pending is set.
	 */
	if (thr_struct_p->t_pending) {
		ti_p->ti_pending.__sigbits[0] =
		    thr_struct_p->t_psig.__sigbits[0];
		ti_p->ti_pending.__sigbits[1] =
		    thr_struct_p->t_psig.__sigbits[1];
		ti_p->ti_pending.__sigbits[2] =
		    thr_struct_p->t_psig.__sigbits[2];
		ti_p->ti_pending.__sigbits[3] =
		    thr_struct_p->t_psig.__sigbits[3];
	} else {
		sigemptyset(&(ti_p->ti_pending));
	}

	return (return_val);
}
#endif /* _SYSCALL32_IMPL */

/*
* Description:
*   Transfer information from thread struct to thread information struct.
*
* Input:
*   *ta_p - thread agent
*   ts_addr - address of thread struct
*   *thr_struct_p - libthread thread struct
*
* Output:
*   *ti_p - thread information struct
*
* Side effects:
*   none
*/
static td_err_e
td_thr2to(td_thragent_t *ta_p, psaddr_t ts_addr,
	uthread_t * thr_struct_p, td_thrinfo_t *ti_p)
{
	td_err_e	return_val = TD_OK;

	return_val = td_thr2to_const(ta_p, ts_addr,
		thr_struct_p, ti_p);

	if (return_val == TD_OK) {
		return_val = td_thr2to_var(thr_struct_p, ti_p);
	}
	return (return_val);
}

#ifdef  _SYSCALL32_IMPL
/*
* Description:
*   Transfer information from thread struct to thread information struct.
*
* Input:
*   *ta_p - thread agent
*   ts_addr - address of thread struct
*   *thr_struct_p - libthread thread struct
*
* Output:
*   *ti_p - thread information struct
*
* Side effects:
*   none
*/
static td_err_e
td_thr2to32(td_thragent_t *ta_p, psaddr_t ts_addr,
	uthread32_t * thr_struct_p, td_thrinfo_t *ti_p)
{
	td_err_e	return_val = TD_OK;

	return_val = td_thr2to_const32(ta_p, ts_addr,
		thr_struct_p, ti_p);

	if (return_val == TD_OK) {
		return_val = td_thr2to_var32(thr_struct_p, ti_p);
	}
	return (return_val);
}
#endif /* _SYSCALL32_IMPL */

/*
* Description:
*   Read the threads hash table for the debug process.
*
* Input:
*   *ta_p - thread agent
*
* Output:
*   *tab_p[] - hash table
*
* Side effects:
*   Debug process is read.
*/
td_err_e
__td_read_thread_hash_tbl(const td_thragent_t *ta_p, thrtab_t * tab_p,
	int tab_size)
{
	td_err_e	return_val;
	ps_err_e	db_return;
	char		error_msg[TD_MAX_BUFFER_SIZE];
	psaddr_t	symbol_addr;

	symbol_addr = (psaddr_t) ta_p->tdb_invar.allthreads_addr;

	db_return = ps_pdread(ta_p->ph_p, symbol_addr,
		(char *) tab_p, tab_size);
	if (db_return != PS_OK) {
		strcpy(error_msg, "__td_read_thread_hash_tbl - ");
		__td_report_db_err(db_return, error_msg);
		return_val = TD_DBERR;
	} else {
		return_val = TD_OK;
	}

	return (return_val);
}

#ifdef  _SYSCALL32_IMPL
td_err_e
__td_read_thread_hash_tbl32(const td_thragent_t *ta_p, thrtab32_t * tab_p,
	int tab_size)
{
	td_err_e	return_val;
	ps_err_e	db_return;
	char		error_msg[TD_MAX_BUFFER_SIZE];
	psaddr_t	symbol_addr;

	symbol_addr = (psaddr_t) ta_p->tdb_invar32.allthreads_addr;

	db_return = ps_pdread(ta_p->ph_p, symbol_addr,
		(char *) tab_p, tab_size);
	if (db_return != PS_OK) {
		strcpy(error_msg, "__td_read_thread_hash_tbl - ");
		__td_report_db_err(db_return, error_msg);
		return_val = TD_DBERR;
	} else {
		return_val = TD_OK;
	}

	return (return_val);
}
#endif /* _SYSCALL32_IMPL */

/*
* Description:
*	   Update the thread information struct. All fields in a thread
*	information structure(td_thrinfo_t) will be updated to be
*	consistent with properties of its respective thread.
*
* Input:
*	   *th_p - thread handle
*
* Output:
*	   *ti_p - updated thread information structure
*	   td_thr_get_info - return value
*
* Side effects:
*	   none
*	   Imported functions called: ps_pdread, ps_pstop,
*	ps_pcontinue.
*
*/
td_err_e
__td_thr_get_info(const td_thrhandle_t *th_p, td_thrinfo_t *ti_p)
{
	uthread_t	thr_struct;
	td_err_e	return_val = TD_ERR;
	td_err_e	td_return = TD_ERR;
	struct ps_prochandle *ph_p;
#ifdef  _SYSCALL32_IMPL
	uthread32_t	thr_struct32;
#endif /* _SYSCALL32_IMPL */
	int		model = PR_MODEL_NATIVE;

#ifdef TEST_PS_CALLS
	if (td_noop) {
		(void) ps_pstop(0);
		(void) ps_pcontinue(0);
		(void) ps_pdread(0, 0, 0, 0);
		return (TD_OK);
	}
#endif

	ph_p = th_p->th_ta_p->ph_p;

	/*
	 * Sanity checks.
	 */
	if (th_p == NULL) {
		return_val = TD_BADTH;
		return (return_val);
	}
	if ((th_p->th_ta_p == NULL) || (th_p->th_unique == NULL)) {
		return_val = TD_BADTA;
		return (return_val);
	}
	if (ti_p == NULL) {
		return_val = TD_ERR;
		return (return_val);
	}

	/*
	 * Get a reader lock.
	 */
	if (rw_rdlock(&(th_p->th_ta_p->rwlock))) {
		return_val = TD_ERR;
		return (return_val);
	}

	if (ph_p == NULL) {
		rw_unlock(&(th_p->th_ta_p->rwlock));
		return_val = TD_BADTA;
		return (return_val);
	}


	/*
	 * Null out the thread information struct.
	 */
	memset(ti_p, NULL, sizeof (*ti_p));

	/*
	 * More than 1 byte is being read.  Stop the process.
	 */
	if (ps_pstop(ph_p) == PS_OK) {

		/*
		 * Extract the thread struct address from the
		 * thread handle and read the thread struct.
		 * Transfer the thread struct to
		 * the thread information struct.  Check that
		 * the thread id is correct.
		 */

		if (__td_pdmodel(ph_p, &model) != 0) {
			return (TD_ERR);
		}
		if (model == PR_MODEL_NATIVE) {
			td_return = __td_read_thr_struct(th_p->th_ta_p,
				th_p->th_unique,
				&thr_struct);

			if (td_return == TD_OK) {
				td_return = td_thr2to(th_p->th_ta_p,
					th_p->th_unique, &thr_struct, ti_p);
				if (td_return == TD_OK) {
					return_val = TD_OK;
				} else {
					return_val = TD_ERR;
				}
			}
		}
#ifdef  _SYSCALL32_IMPL
		else {
			td_return = __td_read_thr_struct32(th_p->th_ta_p,
				th_p->th_unique,
				&thr_struct32);

			if (td_return == TD_OK) {
				td_return = td_thr2to32(th_p->th_ta_p,
					th_p->th_unique, &thr_struct32, ti_p);
				if (td_return == TD_OK) {
					return_val = TD_OK;
				} else {
					return_val = TD_ERR;
				}
			}
		}
#endif /* _SYSCALL32_IMPL */

		/*
		 * Continue process.
		 */
		if (ps_pcontinue(ph_p) != PS_OK) {
			return_val = TD_DBERR;
		}
	/* ps_pstop succeeded   */
	} else {
		return_val = TD_DBERR;
	}

	rw_unlock(&(th_p->th_ta_p->rwlock));
	return (return_val);
}

/*
* Description:
*   Get the floating point registers for the given thread.
* The floating registers are only available for a thread executing
* on and LWP.  If the thread is not running on an LWP, this
* function will return TD_NOFPREGS.
*
* Input:
*   *th_p - thread handle for which fp registers are
*		being requested
*
* Output:
*   *fpregset - floating point register values(see sys/procfs_isa.h)
*   td_thr_getfpregs - return value
*
* Side effects:
*   none
*   Imported functions called:
*/
td_err_e
__td_thr_getfpregs(const td_thrhandle_t *th_p, prfpregset_t * fpregset)
{
	td_err_e	return_val = TD_OK;
	td_err_e	td_return;
	uthread_t	thr_struct;
	struct ps_prochandle *ph_p;
#ifdef  _SYSCALL32_IMPL
	uthread32_t	thr_struct32;
#endif /* _SYSCALL32_IMPL */
	int		model = PR_MODEL_NATIVE;

#ifdef TEST_PS_CALLS
	if (td_noop) {
		(void) ps_pstop(0);
		(void) ps_pcontinue(0);
		(void) ps_pdread(0, 0, 0, 0);
		ps_lgetfpregs(0, 0, 0);
		return (TD_OK);
	}
#endif

	ph_p = th_p->th_ta_p->ph_p;

	/*
	 * Sanity checks.
	 */
	if (th_p == NULL) {
		return_val = TD_BADTH;
		return (return_val);
	}
	if ((th_p->th_ta_p == NULL) || (th_p->th_unique == NULL)) {
		return_val = TD_BADTA;
		return (return_val);
	}
	if (fpregset == NULL) {
		return_val = TD_DBERR;
		return (return_val);
	}

	/*
	 * Get a reader lock.
	 */
	if (rw_rdlock(&(th_p->th_ta_p->rwlock))) {
		return_val = TD_ERR;
	}

	if (ph_p == NULL) {
		rw_unlock(&(th_p->th_ta_p->rwlock));
		return_val = TD_BADTA;
		return (return_val);
	}


	/*
	 * More than 1 byte is being read.  Stop the process.
	 */

	if (ps_pstop(ph_p) == PS_OK) {

		/*
		* Extract the thread struct address from
		* the thread handle and read
		* the thread struct.
		*/

		if (__td_pdmodel(ph_p, &model) != 0) {
			return (TD_ERR);
		}
		if (model == PR_MODEL_NATIVE) {
			td_return = __td_read_thr_struct(th_p->th_ta_p,
				th_p->th_unique, &thr_struct);
			if (td_return == TD_OK) {

				if (ISVALIDLWP(&thr_struct) &&
					HASVALIDFP(&thr_struct)) {
					/*
					 * Read the floating point registers
					 * using the imported interface.
					 */
					if (ps_lgetfpregs(ph_p,
							thr_struct.t_lwpid,
							fpregset) ==
							PS_ERR) {
						return_val = TD_DBERR;
					}
				} else {

					/*
					* fp registers not available
					* in thread struct.
					*/
					return_val = TD_NOFPREGS;
				}
			} else {
				return_val = TD_ERR;
			}
		}
#ifdef  _SYSCALL32_IMPL
		else {
			td_return = __td_read_thr_struct32(th_p->th_ta_p,
				th_p->th_unique, &thr_struct32);
			if (td_return == TD_OK) {

				if (ISVALIDLWP(&thr_struct32) &&
					HASVALIDFP(&thr_struct32)) {
					/*
					 * Read the floating point registers
					 * using the imported interface.
					 */
					if (ps_lgetfpregs(ph_p,
							thr_struct32.t_lwpid,
							fpregset) ==
							PS_ERR) {
						return_val = TD_DBERR;
					}
				} else {

					/*
					* fp registers not available
					* in thread struct.
					*/
					return_val = TD_NOFPREGS;
				}
			} else {
				return_val = TD_ERR;
			}
		}
#endif /* _SYSCALL32_IMPL */

		/*
		 * Continue process.
		 */
		if (ps_pcontinue(ph_p) != PS_OK) {
			return_val = TD_DBERR;
		}
	}
	/* ps_pstop succeeded   */
	else {
		return_val = TD_DBERR;
	}

	rw_unlock(&(th_p->th_ta_p->rwlock));
	return (return_val);
}

/*
* Description:
*   If a thread is in the SLEEP state, its ti_p->ti_state ==
* TD_THR_SLEEP, then get the synchronization handle of the
* synchronization variable that this thread is asleep on.
*
* Input:
*   *th_p - thread handle
*
* Output:
*   *sh_p - synchronization handle on which thread is asleep
* if thread in SLEEP state, NULL otherwise.
*   td_thr_sleepinfo - return value
*
* Side effects:
*   none
*   Imported functions called: ps_pdread, ps_pstop,
* ps_pcontinue, ps_pglobal_lookup, ps_pdwrite.
*
*
*/
td_err_e
__td_thr_sleepinfo(const td_thrhandle_t *th_p, td_synchandle_t *sh_p)
{
	td_err_e	return_val = TD_OK;
	uthread_t	thr_struct;
	struct ps_prochandle *ph_p;
#ifdef  _SYSCALL32_IMPL
	uthread32_t	thr_struct32;
#endif /* _SYSCALL32_IMPL */
	int		model = PR_MODEL_NATIVE;

#ifdef TEST_PS_CALLS
	if (td_noop) {
		(void) ps_pstop(0);
		(void) ps_pcontinue(0);
		if (return_val != TD_NOT_DONE) {
			(void) ps_pglobal_lookup(0, 0, 0, 0);
			(void) ps_pdread(0, 0, 0, 0);
			(void) ps_pdwrite(0, 0, 0, 0);
		}
		return (TD_OK);
	}
#endif

	ph_p = th_p->th_ta_p->ph_p;

	/*
	 * Get a reader lock.
	 */
	if (rw_rdlock(&(th_p->th_ta_p->rwlock))) {
		return_val = TD_ERR;
	}

	if (ph_p == NULL) {
		rw_unlock(&(th_p->th_ta_p->rwlock));
		return_val = TD_BADTA;
		return (return_val);
	}

	/*
	 * More than 1 byte is geing read.  Stop the process.
	 */
	if (ps_pstop(ph_p) == PS_OK) {
		if (__td_pdmodel(ph_p, &model) != 0) {
			return (TD_ERR);
		}
		if (model == PR_MODEL_NATIVE) {
			return_val = __td_read_thr_struct(th_p->th_ta_p,
			    th_p->th_unique, &thr_struct);
			if (thr_struct.t_state != TS_SLEEP)
				return_val = TD_ERR;
			sh_p->sh_unique = (psaddr_t) thr_struct.t_wchan;
		}
#ifdef  _SYSCALL32_IMPL
		else {
			return_val = __td_read_thr_struct32(th_p->th_ta_p,
			    th_p->th_unique, &thr_struct32);
			if (thr_struct32.t_state != TS_SLEEP)
				return_val = TD_ERR;
			sh_p->sh_unique = (psaddr_t) thr_struct32.t_wchan;
		}
#endif /* _SYSCALL32_IMPL */
		sh_p->sh_ta_p = th_p->th_ta_p;

		/*
		 * Continue process.
		 */

		if (ps_pcontinue(ph_p) != PS_OK) {
			return_val = TD_DBERR;
		}
	}
	/* ps_pstop succeeded   */
	else {
		return_val = TD_DBERR;
	}
	rw_unlock(&(th_p->th_ta_p->rwlock));
	return (return_val);

}


/*
 * This structure links td_thr_lockowner and the lowner_cb callback
 * function.
 */
typedef struct {
	td_sync_iter_f *owner_cb;
	void *owner_cb_arg;
	td_thrhandle_t *th_p;
} lowner_cb_ctl_t;


static int
lowner_cb(const td_synchandle_t *sh_p, void *arg)

{
	register lowner_cb_ctl_t *ocb = arg;
	mutex_t mx;

	if (ps_pdread(sh_p->sh_ta_p->ph_p, (psaddr_t) sh_p->sh_unique,
	    (caddr_t) &mx, sizeof (mx)) != PS_OK)
		return (0);
	if (mx.mutex_magic == MUTEX_MAGIC &&
	    mx.mutex_owner == ocb->th_p->th_unique)
		return ((ocb->owner_cb)(sh_p, ocb->owner_cb_arg));
	return (0);
}


/*
* Description:
*   Iterate over the set of locks owned by a specified thread.
* If cb returns a non-zero value, terminate iterations.
*
* Input:
*   *th_p - thread handle
*   *cb - function to be called on each lock owned by thread.
*   cb_data_p - pointer passed to td_sync_iter_f()
*
* Output:
*   td_thr_lockowner - return value
*
* Side effects:
*   *cb is called on each lock owned by thread.
*   Imported functions called: ps_pdread, ps_pstop,
* ps_pcontinue, ps_pglobal_lookup, ps_pdwrite.
*/
td_err_e
__td_thr_lockowner(const td_thrhandle_t *th_p,
	td_sync_iter_f * cb, void *cb_data_p)
{
	td_err_e	return_val = TD_OK;
	lowner_cb_ctl_t	lcb;

#ifdef TEST_PS_CALLS
	if (td_noop) {
		(void) ps_pstop(0);
		(void) ps_pcontinue(0);
		if (return_val != TD_NOT_DONE) {
			(void) ps_pglobal_lookup(0, 0, 0, 0);
			(void) ps_pdread(0, 0, 0, 0);
		}
		return (TD_OK);
	}
#endif

	/*
	 * Sanity checks.
	 */
	if (th_p == NULL) {
		return (TD_BADTH);
	}
	if ((th_p->th_ta_p == NULL) || (th_p->th_unique == NULL)) {
		return (TD_BADTA);
	}
	/*
	 * Get a reader lock.
	 */
	if (rw_rdlock(&(th_p->th_ta_p->rwlock))) {
		return (TD_ERR);
	}

	if (th_p->th_ta_p->ph_p == NULL) {
		rw_unlock(&(th_p->th_ta_p->rwlock));
		return (TD_BADPH);
	}

	/*
	 * More than 1 byte is geing read.  Stop the process.
	 */
	if (ps_pstop(th_p->th_ta_p->ph_p) == PS_OK) {
		lcb.owner_cb = cb;
		lcb.owner_cb_arg = cb_data_p;
		lcb.th_p = (td_thrhandle_t *) th_p;
		return_val = __td_ta_sync_iter(th_p->th_ta_p,
		    lowner_cb, &lcb);
		if (ps_pcontinue(th_p->th_ta_p->ph_p) != PS_OK) {
			return_val = TD_DBERR;
		}
	}
	/* ps_pstop succeeded   */
	else {
		return_val = TD_DBERR;
	}

	rw_unlock(&(th_p->th_ta_p->rwlock));
	return (return_val);

}


/*
* Description:
*   Change a thread's signal mask to the value specified by
* ti_sigmask.
*
* Input:
*   *th_p - thread handle
*   ti_sigmask - new value of signal mask
*
* Output:
*   td_thr_sigsetmask - return value
*
* Side effects:
*   Thread corresponding to *th_p is assigned new signal mask.
*   Imported functions called: ps_pstop,
* ps_pcontinue, ps_pdwrite.
*/
td_err_e
__td_thr_sigsetmask(const td_thrhandle_t *th_p, const sigset_t ti_sigmask)
{
	td_err_e	return_val = TD_OK;
	uthread_t	*ts_p;
	psaddr_t	sigmask_addr;

#ifdef TEST_PS_CALLS
	if (td_noop) {
		(void) ps_pstop(0);
		(void) ps_pcontinue(0);
		(void) ps_pdwrite(0, 0, 0, 0);
		return (return_val);
	}
#endif

	/*
	 * Sanity checks.
	 */
	if (th_p == NULL) {
		return_val = TD_BADTH;
		return (return_val);
	}
	if ((th_p->th_ta_p == NULL) || (th_p->th_unique == NULL)) {
		return_val = TD_BADTA;
		return (return_val);
	}

	/*
	 * Get a reader lock.
	 */
	if (rw_rdlock(&(th_p->th_ta_p->rwlock))) {
		return_val = TD_ERR;
	}

	if (th_p->th_ta_p->ph_p == NULL) {
		rw_unlock(&(th_p->th_ta_p->rwlock));
		return_val = TD_BADTA;
		return (return_val);
	}

	/*
	 * More than 1 byte is being written.  Stop the process.
	 */
	if (ps_pstop(th_p->th_ta_p->ph_p) == PS_OK) {

		/*
		 * Write the sigmask into the thread struct.
		 */
		ts_p = (uthread_t *) th_p->th_unique;
		sigmask_addr = (psaddr_t) & (ts_p->t_hold);
		if (ps_pdwrite(th_p->th_ta_p->ph_p, sigmask_addr,
			(char *) &ti_sigmask, sizeof (ti_sigmask)) != PS_OK) {

			return_val = TD_DBERR;
		}

		/*
		 * Continue process.
		 */

		if (ps_pcontinue(th_p->th_ta_p->ph_p) != PS_OK) {
			return_val = TD_DBERR;
		}
	}
	/* ps_pstop succeeded   */
	else {
		return_val = TD_DBERR;
	}

	rw_unlock(&(th_p->th_ta_p->rwlock));
	return (return_val);
}


/*
* Description:
*   Change a thread's priority to the value specified by ti_pri.
*
* Input:
*   *th_p -  thread handle
*   ti_pri - new value of thread priority >= 0(see thr_setprio(3T))
*
* Output:
*   td_thr_setprio - return value
*
* Side effects:
*   Thread corresponding to *th_p is assigned new priority.
*   Imported functions called: ps_pdwrite.
*
*/
td_err_e
__td_thr_setprio(const td_thrhandle_t *th_p, const int ti_pri)
{
	uthread_t	*ts_p;
	psaddr_t	pri_addr;
	td_err_e	return_val = TD_OK;

#ifdef TEST_PS_CALLS
	if (td_noop) {
		(void) ps_pdwrite(0, 0, 0, 0);
		return (return_val);
	}
#endif

	/*
	 * Sanity checks.
	 */
	if (th_p == NULL) {
		return_val = TD_BADTH;
		return (return_val);
	}
	if ((th_p->th_ta_p == NULL) || (th_p->th_unique == NULL)) {
		return_val = TD_BADTA;
		return (return_val);
	}

	/*
	 * Get a reader lock.
	 */
	if (rw_rdlock(&(th_p->th_ta_p->rwlock))) {
		return_val = TD_ERR;
	}
	if (th_p->th_ta_p->ph_p == NULL) {
		rw_unlock(&(th_p->th_ta_p->rwlock));
		return_val = TD_BADTA;
		return (return_val);
	}

	/*
	 * Set the priority in the thread struct.
	 */

	/*
	 * Only setting 1 byte.  Don't stop process.
	 */

	if ((ti_pri >= THREAD_MIN_PRIORITY) &&
			(ti_pri <= THREAD_MAX_PRIORITY)) {
		ts_p = (uthread_t *) th_p->th_unique;
		pri_addr = (psaddr_t) & ts_p->t_pri;
		if (ps_pdwrite(th_p->th_ta_p->ph_p, pri_addr,
				(char *) &ti_pri, sizeof (ti_pri)) != PS_OK) {
			return_val = TD_DBERR;
		}
	} else {
		return_val = TD_ERR;
	}

	rw_unlock(&(th_p->th_ta_p->rwlock));
	return (return_val);
}

/*
* Description:
*   Change the pending signal flag and pending signals
*
* Input:
*   *th_p - thread handle
*   ti_pending_flag - flag that indicates that there are
*	pending signals in the thread.  If this value is not
*	set, the pending signal mask is ignored.
*   ti_pending - new pending signal information.
*
* Output:
*   td_thr_setsigpending - return value
*
* Side effects:
*   Thread corresponding to *th_p has pending signal
* information changed.
*   Imported functions called: ps_pstop,
* ps_pcontinue, ps_pdwrite.
*/
td_err_e
__td_thr_setsigpending(const td_thrhandle_t *th_p,
	const uchar_t ti_pending_flag, const sigset_t ti_pending)
{

	/*
	 * Only the pending signal flag and pending signal mask are set in
	 * the thread struct.
	 */

	td_err_e	return_val = TD_OK;

#ifdef TEST_PS_CALLS
	if (td_noop) {
		(void) ps_pstop(0);
		(void) ps_pcontinue(0);
		(void) ps_pdwrite(0, 0, 0, 0);
		return (TD_OK);
	}
#endif

	/*
	 * Sanity checks.
	 */
	if (th_p == NULL) {
		return_val = TD_BADTH;
		return (return_val);
	}
	if ((th_p->th_ta_p == NULL) || (th_p->th_unique == NULL)) {
		return_val = TD_BADTA;
		return (return_val);
	}

	/*
	 * Get a reader lock.
	 */
	if (rw_rdlock(&(th_p->th_ta_p->rwlock))) {
		return_val = TD_ERR;
	}

	if (th_p->th_ta_p->ph_p == NULL) {
		rw_unlock(&(th_p->th_ta_p->rwlock));
		return_val = TD_BADTA;
		return (return_val);
	}

	/*
	 * More than 1 byte is being written.  Stop the process.
	 */

	if (ps_pstop(th_p->th_ta_p->ph_p) == PS_OK) {
		uthread_t *thr_struct_p;
		/*
		 * Point a thread struct pointer at the address
		 * of the thread struct in the debuggee.  The
		 * address of the pending signal information will
		 * be offset from this thread struct address.
		 */
		thr_struct_p = (uthread_t *)th_p->th_unique;

		/*
		 * Write t_psig.
		 */
		if (ps_pdwrite(th_p->th_ta_p->ph_p, (psaddr_t)
			&(thr_struct_p->t_psig), (char *)&ti_pending,
			sizeof (thr_struct_p->t_psig)) != PS_OK) {
			return_val = TD_DBERR;
		}

		/*
		 * Write t_pending.
		 */
		if (ps_pdwrite(th_p->th_ta_p->ph_p, (psaddr_t)
			&(thr_struct_p->t_pending), (char *)&ti_pending_flag,
			sizeof (thr_struct_p->t_pending)) != PS_OK) {
			return_val = TD_DBERR;
		}

		/*
		 * Continue process.
		 */

		if (ps_pcontinue(th_p->th_ta_p->ph_p) != PS_OK) {
			return_val = TD_DBERR;
		}
	}
	/* ps_pstop succeeded   */
	else {
		return_val = TD_DBERR;
	}

	rw_unlock(&(th_p->th_ta_p->rwlock));
	return (return_val);
}

/*
* Description:
*   Set the floating pointing registers for a given thread.
* The floating registers are only available for a thread executing
* on and LWP.  If the thread is not running on an LWP, this
* function will return TD_NOFPREGS.
*
* Input:
*   *th_p - thread handle for thread on which fp registers are
* being set.
*   *fpregset - floating point register values(see sys/procfs_isa.h)
*
* Output:
*   none
*
* Side effects:
*   Floating point registers in thread corresponding to *th_p
* are set.
*   Imported functions called: ps_pdread,
* ps_pstop, ps_pcontinue, ps_lsetfpregs.
*/
td_err_e
__td_thr_setfpregs(const td_thrhandle_t *th_p, const prfpregset_t * fpregset)
{
	td_err_e	return_val = TD_OK;
	td_err_e	td_return;
	uthread_t	thr_struct;
	struct ps_prochandle *ph_p;
#ifdef  _SYSCALL32_IMPL
	uthread32_t	thr_struct32;
#endif /* _SYSCALL32_IMPL */
	int		model = PR_MODEL_NATIVE;

#ifdef TEST_PS_CALLS
	if (td_noop) {
		(void) ps_pstop(0);
		(void) ps_pcontinue(0);
		(void) ps_pdread(0, 0, 0, 0);

		(void) ps_lsetfpregs(0, 0, 0);
		return (TD_OK);
	}
#endif

	ph_p = th_p->th_ta_p->ph_p;

	/*
	 * Sanity checks.
	 */
	if (th_p == NULL) {
		return_val = TD_BADTH;
		return (return_val);
	}
	if ((th_p->th_ta_p == NULL) || (th_p->th_unique == NULL)) {
		return_val = TD_BADTA;
		return (return_val);
	}
	if (fpregset == NULL) {
		return (TD_ERR);
	}

	/*
	 * Get a reader lock.
	 */
	if (rw_rdlock(&(th_p->th_ta_p->rwlock))) {
		return_val = TD_ERR;
	}

	if (ph_p == NULL) {
		rw_unlock(&(th_p->th_ta_p->rwlock));
		return_val = TD_BADTA;
		return (return_val);
	}

	/*
	 * More than 1 byte is geing read.  Stop the process.
	 */

	if (ps_pstop(ph_p) == PS_OK) {
		/*
		* Extract the thread struct address from the
		* thread handle and read
		* the thread struct.
		*/

		if (__td_pdmodel(ph_p, &model) != 0) {
			return (TD_ERR);
		}
		if (model == PR_MODEL_NATIVE) {
			td_return = __td_read_thr_struct(th_p->th_ta_p,
				th_p->th_unique, &thr_struct);

			if (td_return == TD_OK) {

				/*
				* Write the floating point registers using
				* the imported interface.
				*/

				if (ISVALIDLWP(&thr_struct) &&
						HASVALIDFP(&thr_struct)) {
					if (ps_lsetfpregs(ph_p,
						thr_struct.t_lwpid,
						fpregset) == PS_ERR) {
						return_val = TD_DBERR;
					}
				} else {
					return_val = TD_NOFPREGS;
				}
			} else {
				return_val = TD_ERR;
			}
		}
#ifdef  _SYSCALL32_IMPL
		else {
			td_return = __td_read_thr_struct32(th_p->th_ta_p,
				th_p->th_unique, &thr_struct32);

			if (td_return == TD_OK) {

				/*
				* Write the floating point registers using
				* the imported interface.
				*/

				if (ISVALIDLWP(&thr_struct32) &&
						HASVALIDFP(&thr_struct32)) {
					if (ps_lsetfpregs(ph_p,
						thr_struct32.t_lwpid,
						fpregset) == PS_ERR) {
						return_val = TD_DBERR;
					}
				} else {
					return_val = TD_NOFPREGS;
				}
			} else {
				return_val = TD_ERR;
			}
		}
#endif /* _SYSCALL32_IMPL */
		/*
		 * Continue process.
		 */
		if (ps_pcontinue(ph_p) != PS_OK) {
			return_val = TD_DBERR;
		}
	}
	/* ps_pstop succeeded   */
	else {
		return_val = TD_DBERR;
	}

	rw_unlock(&(th_p->th_ta_p->rwlock));
	return (return_val);
}

#ifdef TD_INTERNAL_TESTS

/*
* Description:
*   Validate the thread information in the
* thread information struct by comparison with original
* threads information.
*
* Input:
*   *ti_p - thread information struct
*
* Output:
*   __td_ti_validate - return value
*
* Side effects:
*   none
*
*/
td_err_e
__td_ti_validate(const td_thrinfo_t *ti_p)
{

	/*
	 * Compare the contents of the thread information struct to that of the
	 * threads in this process.
	 */
	td_err_e	return_val = TD_OK;
	uthread_t	*ts_p;
	td_thr_state_e	mapped_state;

	ts_p = (uthread_t *) ti_p->ti_ro_area;

	if (ts_p->t_usropts != ti_p->ti_user_flags) {
		if (__td_debug)
			diag_print("Compare failed: user flags %x %x\n",
			ts_p->t_usropts, ti_p->ti_user_flags);
		return_val = TD_TBADTH;
	}
	if (ts_p->t_tid != ti_p->ti_tid) {
		if (__td_debug)
			diag_print("Compare failed: tid %x %x\n",
				ts_p->t_tid, ti_p->ti_tid);
		return_val = TD_TBADTH;
	}
	if (ts_p->t_startpc != ti_p->ti_startfunc) {
		if (__td_debug)
			diag_print("Compare failed: start func %p %p\n",
			ts_p->t_startpc, ti_p->ti_startfunc);
		return_val = TD_TBADTH;
	}
	if (ts_p->t_stk != ti_p->ti_stkbase) {
		if (__td_debug)
			diag_print("Compare failed: stkbase %p %p\n",
				ts_p->t_stk, ti_p->ti_stkbase);
		return_val = TD_TBADTH;
	}
	if (ts_p->t_stksize != ti_p->ti_stksize) {
		if (__td_debug)
			diag_print("Compare failed: stksize %x %x\n",
				ts_p->t_stksize, ti_p->ti_stksize);
		return_val = TD_TBADTH;
	}
	if (ts_p->t_flag != ti_p->ti_flags) {
		if (__td_debug)
			diag_print("Compare failed: flags %x %x\n",
				ts_p->t_flag, ti_p->ti_flags);
		return_val = TD_TBADTH;
	}
	if (__td_thr_map_state(ts_p->t_state, &mapped_state) != TD_OK) {
		if (__td_debug)
			diag_print("State mapping failed\n");
	}
	if (mapped_state != ti_p.ti_state) {
		if (__td_debug)
			diag_print("Compare failed: state %x %x\n",
				mapped_state, ti_p->ti_state);
		return_val = TD_TBADTH;
	}
	if (TD_CONVERT_TYPE(*ts_p) != ti_p->ti_type) {
		if (__td_debug)
			diag_print("Compare failed: type %x %x\n",
				TD_CONVERT_TYPE(*ts_p), ti_p->ti_type);
		return_val = TD_TBADTH;
	}
	if (ts_p->t_pc != ti_p->ti_pc) {
		if (__td_debug)
			diag_print("Compare failed: pc %lx %lx\n",
				ts_p.t_pc, ti_p->ti_pc);
		return_val = TD_TBADTH;
	}
	if (ts_p->t_sp != ti_p->ti_sp) {
		if (__td_debug)
			diag_print("Compare failed: sp %lx %lx\n",
				ts_p->t_sp, ti_p->ti_sp);
		return_val = TD_TBADTH;
	}
	if (ts_p->t_pri != ti_p->ti_pri) {
		if (__td_debug)
			diag_print("Compare failed: priority %x %x\n",
				ts_p->t_pri, ti_p->ti_pri);
		return_val = TD_TBADTH;
	}

	/*
	 * lwp id is only correct if thread is on an lwp.
	 */
	if (ISVALIDLWP(ts_p)) {
		if (ts_p->t_lwpid != ti_p->ti_lid) {
			if (__td_debug)
				diag_print("Compare failed: lid %x %x\n",
					ts_p->t_lwpid, ti_p->ti_lid);
			return_val = TD_TBADTH;
		}
	}
	if (ts_p->__sigbits[0] !=
			ti_p->ti_sigmask.__sigbits[0]) {
		if (__td_debug)
			diag_print("Compare failed: sigmask %x %x\n",
				ts_p->t_hold, ti_p->ti_sigmask);
		return_val = TD_TBADTH;
	}

	if (ts_p->t_preempt != ti_p->ti_preemptflag) {
		if (__td_debug)
			diag_print("Compare failed: preemptflag %x %x\n",
				ts_p->t_preempt,
				ti_p->ti_preemptflag);
		return_val = TD_TBADTH;
	}

	if (ts_p->__sigbits[0] !=
			ti_p->ti_pending.__sigbits[0]) {
		if (__td_debug)
			diag_print("Compare failed: pending %x %x\n",
				ts_p->t_psig, ti_p->ti_pending);
		return_val = TD_TBADTH;
	}

	/*
	 * XXX -- Check priority inversion flag when it is present in thread
	 * struct.
	 */

#if TD_DEBUG

	if (!return_val) {
		if (__td_debug)
			diag_print("Compare passed\n");
	}
#endif

	return (return_val);
}
#endif /* TD_INTERNAL_TESTS */


/*
* Description:
*   Suspend the execution of a specified thread.  It remains
* suspended until it is resumed by td_thr_dbresume(). A thread
* that is suspended via td_thr_dbsuspend() is completely different
* from a thread being suspended *	via thr_suspend(). The
* application cannot cause a suspended thread to become runnable.
*
* Input:
*   *th_p - thread handle
*
* Output:
*   td_thr_dbsuspend - return value
*
* Side effects:
*   Thread is suspended.
*   Imported functions called:
*/
td_err_e
__td_thr_dbsuspend(const td_thrhandle_t *th_p)
{
	td_err_e	return_val;
	ps_err_e	ps_ret;
	tdb_ctl_t	agent_ctl;
#ifdef  _SYSCALL32_IMPL
	tdb_ctl32_t	agent_ctl32;
#endif /* _SYSCALL32_IMPL */
	int		model = PR_MODEL_NATIVE;

#ifdef TEST_PS_CALLS
	if (td_noop) {
		return (TD_OK);
	}
#endif

	/*
	 * Sanity checks.
	 */
	if (th_p == NULL) {
		return (TD_BADTH);
	}
	if ((th_p->th_ta_p == NULL) || (th_p->th_unique == NULL)) {
		return (TD_BADTA);
	}

	/*
	 * Get a reader lock.
	 */
	if (rw_rdlock(&(th_p->th_ta_p->rwlock))) {
		return (TD_ERR);
	}

	if (th_p->th_ta_p->ph_p == NULL) {
		rw_unlock(&(th_p->th_ta_p->rwlock));
		return (TD_BADTA);
	}

	if ((ps_ret = ps_pstop(th_p->th_ta_p->ph_p)) == PS_OK) {
		if (__td_pdmodel(th_p->th_ta_p->ph_p, &model) != 0) {
			rw_unlock(&(th_p->th_ta_p->rwlock));
			return (TD_ERR);
		}
		if (model == PR_MODEL_NATIVE) {
			agent_ctl.opcode = THREAD_SUSPEND;
			agent_ctl.u.thr_p = (uthread_t *) th_p->th_unique;
			return_val = __td_agent_send(th_p->th_ta_p, &agent_ctl);
		}
#ifdef  _SYSCALL32_IMPL
		else {
			agent_ctl32.opcode = THREAD_SUSPEND;
			agent_ctl32.u.thr_p = (caddr32_t) th_p->th_unique;
			return_val = __td_agent_send(th_p->th_ta_p,
			    (tdb_ctl_t *)&agent_ctl32);
		}
#endif /* _SYSCALL32_IMPL */
		ps_ret = ps_pcontinue(th_p->th_ta_p->ph_p);
	}

	if (ps_ret != PS_OK)
		return_val = TD_DBERR;
	rw_unlock(&(th_p->th_ta_p->rwlock));
	return (return_val);

}


/*
* Description:
*   Resume the execution of a thread suspended via
* td_thr_dbsuspend().
*
* Input:
*   *th_p - thread handle
*
* Output:
*   td_thr_dbresume - return value
*
* Side effects:
*   Thread is continued.
*   Imported functions called:
*/
td_err_e
__td_thr_dbresume(const td_thrhandle_t *th_p)
{
	td_err_e	return_val;
	ps_err_e	ps_ret;
	tdb_ctl_t	agent_ctl;
#ifdef  _SYSCALL32_IMPL
	tdb_ctl32_t	agent_ctl32;
#endif /* _SYSCALL32_IMPL */
	int		model = PR_MODEL_NATIVE;

#ifdef TEST_PS_CALLS
	if (td_noop) {
		return (TD_OK);
	}
#endif

	/*
	 * Sanity checks.
	 */
	if (th_p == NULL) {
		return_val = TD_BADTH;
		return (return_val);
	}
	if ((th_p->th_ta_p == NULL) || (th_p->th_unique == NULL)) {
		return_val = TD_BADTA;
		return (return_val);
	}

	/*
	 * Get a reader lock.
	 */
	if (rw_rdlock(&(th_p->th_ta_p->rwlock))) {
		return_val = TD_ERR;
	}

	if (th_p->th_ta_p->ph_p == NULL) {
		rw_unlock(&(th_p->th_ta_p->rwlock));
		return_val = TD_BADTA;
		return (return_val);
	}

	if ((ps_ret = ps_pstop(th_p->th_ta_p->ph_p)) == PS_OK) {
		if (__td_pdmodel(th_p->th_ta_p->ph_p, &model) != 0) {
			rw_unlock(&(th_p->th_ta_p->rwlock));
			return (TD_ERR);
		}
		if (model == PR_MODEL_NATIVE) {
			agent_ctl.opcode = THREAD_RESUME;
			agent_ctl.u.thr_p = (uthread_t *) th_p->th_unique;
			return_val = __td_agent_send(th_p->th_ta_p, &agent_ctl);
		}
#ifdef  _SYSCALL32_IMPL
		else {
			agent_ctl32.opcode = THREAD_RESUME;
			agent_ctl32.u.thr_p = (caddr32_t) th_p->th_unique;
			return_val = __td_agent_send(th_p->th_ta_p,
			    (tdb_ctl_t *)&agent_ctl32);
		}
#endif /* _SYSCALL32_IMPL */
		ps_ret = ps_pcontinue(th_p->th_ta_p->ph_p);
	}

	if (ps_ret != PS_OK)
		return_val = TD_DBERR;
	rw_unlock(&(th_p->th_ta_p->rwlock));
	return (return_val);
}


/*
* Description:
*   Check the value in data against the thread id.  If
* it matches, return 1 to terminate interations.
*   This function is used by td_ta_map_id2thr() to map id to
* a thread handle.
*
* Input:
*   *th_p - thread handle
*   *data - thread id being sought
*
* Output:
*   td_mapper_id2thr - returns 1 if thread id is found.
*
* Side effects:
*/
static int
td_mapper_id2thr(const td_thrhandle_t *th_p, td_mapper_param_t *data)
{
	int	return_val = 0;
	td_thrinfo_t ti;
	td_err_e td_return;

	td_return = __td_thr_get_info(th_p, &ti);

	if (td_return == TD_OK) {
		if (data->tid == ti.ti_tid) {
			data->found = TRUE;
			data->th = *th_p;
			return_val = 1;
		}
	}
	return (return_val);
}

/*
* Description:
*   Given a thread identifier, return the corresponding thread
* handle.
*
* Input:
*   *ta_p - thread agent
*   tid - Value of thread identifier(e.g., as
* returned by call to thr_self()).
*
* Output:
*   *th_p - Thread handle
*   td_ta_map_id2thr - return value
*
* Side effects:
*   none
*   Imported functions called: ps_pdread, ps_pstop,
* ps_pcontinue.
*/
td_err_e
__td_ta_map_id2thr(const td_thragent_t *ta_p, thread_t tid,
	td_thrhandle_t *th_p)
{

	td_err_e	return_val;
	td_mapper_param_t	data;

#ifdef TEST_PS_CALLS
	if (td_noop) {
		(void) ps_pstop(0);
		(void) ps_pcontinue(0);
		(void) ps_pdread(0, 0, 0, 0);

		return (TD_OK);
	}
#endif

	/*
	 * Check for bad thread agent pointer.
	 */
	if (ta_p == NULL) {
		return_val = TD_BADTA;
		return (return_val);
	}

	/*
	 * LOCKING EXCEPTION - Locking is not required
	 * here because the locking and checking will be
	 * done in __td_ta_thr_iter.  If __td_ta_thr_iter
	 * is not used or if some use of the thread agent
	 * is made other than the sanity checks, ADD
	 * locking.
	 */

	/*
	 * Check for bad thread handle pointer.
	 */
	if (th_p == NULL) {
		return_val = TD_BADTH;
		return (return_val);
	}

	if (tid != 0) {
		data.tid = tid;
		data.found = NULL;
		return_val = __td_ta_thr_iter(ta_p,
			(td_thr_iter_f *) td_mapper_id2thr, (void *)&data,
			TD_THR_ANY_STATE, TD_THR_LOWEST_PRIORITY,
			TD_SIGNO_MASK, TD_THR_ANY_USER_FLAGS);
		if (return_val == TD_OK) {
			if (data.found != NULL) {
				*th_p = (data.th);
			} else {
				return_val = TD_NOTHR;
				__td_report_po_err(return_val,
					"Thread not found - td_ta_map_id2thr");
			}
		}
	} else {
		return_val = TD_NOTHR;
		__td_report_po_err(return_val,
			"Invalid thread id - td_ta_map_id2thr");
	}

	return (return_val);

}

#ifdef TD_INTERNAL_TESTS

/*
* Description:
*   Dump out selected fields of a thread information struct
*
* Input:
*   *ti_p - thread information struct
*
* Output:
*   none
*
* Side effects:
*   none
*/
void
__td_tsd_dump(const td_thrinfo_t *ti_p, const thread_key_t key)
{
	void		*data;
	td_thrhandle_t	th;

	th.th_unique = ti_p->ti_ro_area;
	th.th_ta_p = ti_p->ti_ta_p;
	td_thr_tsd(&th, key, &data);

	diag_print("Thread Object:\n");
	diag_print("      Id:			%d\n", ti_p->ti_tid);
	diag_print("      TSD key:			%i\n", key);
	diag_print("      Key binding:		%x\n", data);

}

/*
* Description:
*   Dump out a thread information struct
*
* Input:
*   *ti_p - thread information struct
*   full - flag for full dump
*
* Output:
*   none
*
* Side effects:
*   none
*/
void
__td_ti_dump(const td_thrinfo_t *ti_p, int full)
{
	int	i;

	diag_print("Thread Object:\n");
	if (full) {
		diag_print("  Thread agent pointer:	%p\n",
			ti_p->ti_ta_p);
		diag_print("    Thread creation information:\n");
		diag_print("      User flags:		%x\n",
			ti_p->ti_user_flags);
	}
	diag_print("      Id:			%d\n", ti_p->ti_tid);
	diag_print("      Start function address:	%p\n",
		ti_p->ti_startfunc);
	diag_print("      Stack base:		%p\n",
		ti_p->ti_stkbase);
	diag_print("      Stack size:		%d\n", ti_p->stksize);
	if (full) {
		diag_print("      Thread flags:		%x\n",
			ti_p->ti_flags);
		diag_print("      Thread struct address:	%lx\n",
			ti_p->ti_ro_area);
		diag_print("      Thread struct size:	%d\n",
			ti_p->ti_ro_size);
	}
	diag_print("   Thread variable information:\n");
	diag_print("      PC:			%lx\n", ti_p->ti_pc);
	diag_print("      SP:			%lx\n", ti_p->ti_sp);
	diag_print("      State:			%s\n",
		td_thr_state_names[ti_p->ti_state];
	if (full) {
		diag_print("      Debugger suspended:	%d\n",
			ti_p->ti_db_suspended);
		diag_print("      Priority:			%d\n",
			ti_p->ti_pri);
		diag_print("      Associated LWP Id:		%d\n",
			ti_p->ti_lid);
		diag_print("      Signal mask:		%x\n",
			ti_p->ti_sigmask);
		diag_print("      Enabled events:		%x\n",
			ti_p->ti_events.event_bits[1]);
		diag_print("      Trace enabled:		%d\n",
			ti_p->ti_traceme);
		diag_print("      Has been preempted:	%d\n",
			ti_p->ti_preemptflag);
		diag_print("      Priority inheritance done	%d\n",
			ti_p->ti_pirecflag);

		diag_print("      Pending signal information:\n");

		diag_print("        Pending signals:		");
		for (i = 1; i < 4; i++) {
			diag_print("%x ",
				ti_p->ti_pending.__sigbits[i]);
		}
		diag_print("\n");
	}
}


/*
* Description:
*   Dump siginfo_t struct
*
* Input:
*   *sig - siginfo_t struct
*
* Output:
*   none
*
* Side effects:
*   none
*/
static void
td_siginfo_dump(const siginfo_t * sig_p)
{
	siginfo_t	sig;

	sig = *sig_p;

	diag_print("si_signo - signal from signal.h:	%d\n", sig.si_signo);
	diag_print("si_code - code from above:	%d\n", sig.si_code);
	diag_print("si_errno - error from errno.h:	%d\n", sig.si_errno);
	diag_print("union _data:\n");
	diag_print("  struct _proc:\n");
	diag_print("    _pid:			%d\n", sig.si_pid);
	diag_print("    union _data:\n");
	diag_print("      struct _kill:\n");
	diag_print("        _uid:			%d\n", sig.si_pid);
	diag_print("      struct _cld:\n");
	diag_print("        _utime:			%ld\n", sig.si_utime);
	diag_print("        _status:			%d\n", sig.si_status);
	diag_print("        _stime:			%ld\n", sig.si_stime);
	diag_print("  struct _fault - SIGSEGV, SIGBUS, SIGILL and SIGFPE :\n");
	diag_print("    _addr - faulting address:	%p\n", sig.si_addr);
	diag_print("    _trapno - Illegal trap number:%d\n", sig.si_trapno);
	diag_print("  struct _file - SIGPOLL, SIGXFSZ\n");
	diag_print("    _fd - file descriptor:		%d\n", sig.si_fd);
	diag_print("    _band:			%d\n", sig.si_band);
	diag_print("  struct _prof - SIGPROF\n");
	diag_print("     _faddr - last fault address	%lx\n", sig.si_faddr);
	diag_print("     _tstamp - real time stamp:	%ld\n", sig.si_tstamp);
	diag_print("     _syscall - current syscall:	%d\n", sig.si_syscall);
	diag_print("     _nsysarg - no. of arguments:%d\n", sig.si_nsysarg);
	diag_print("     _fault - last fault type:	%d\n", sig.si_fault);
	diag_print("     _sysarg - syscall arguments:(not shown)\n");
	diag_print("     _mstate 			(not shown)\n");

}
#endif

/*
* Description:
*   Compare mask1 and mask2.  If equal, return 0,
* Otherwise, return non-zero.
*
* Input:
*   mask1_p - signal mask
*   mask2_p - signal mask
*
* Output:
*   td_sigmak_are_equal - return value
*
* Side effects:
*   none
*/
int
__td_sigmask_are_equal(sigset_t * mask1_p, sigset_t * mask2_p)
{
	int	return_val = 1;
	int	i, mask_word_cnt;

	if ((mask1_p != NULL) && (mask2_p != NULL)) {
		mask_word_cnt = sizeof (sigset_t) /
			sizeof ((*mask1_p).__sigbits[0]);

		for (i = 0; i < mask_word_cnt; i++) {
			if ((*mask1_p).__sigbits[i] !=
					(*mask2_p).__sigbits[i]) {
				return_val = 0;
				break;
			}
		}
	} else {
		return_val = 0;
	}

	return (return_val);
}
