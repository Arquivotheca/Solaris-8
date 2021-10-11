/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)xtd_po.c	1.28	99/08/10 SMI"

/*
*  Description:
* The functions in this module contain functions that
* interact in a sparc specific way with the process or processes
* that execute the program.
*/

#pragma weak td_ta_map_lwp2thr = __td_ta_map_lwp2thr /* i386 work around */

#include <thread_db.h>
#include "td.h"
#include "td_po.h"
#include "td_to.h"

/*
* Description:
*   Convert an LWP to a thread handle.
*
* Input:
*   *ta_p - thread agent
*   lwpid - lwpid for LWP on which thread
* 	is being requested.
*
* Output:
*   *th_p - thread handle for thread executing
* on LWP with lwpid.
*
* Side effects:
*   none.
*   Imported functions called: ps_lgetregs, ps_pdread.
*/

td_err_e
__td_ta_map_lwp2thr(const td_thragent_t *ta_p, lwpid_t lwpid,
	td_thrhandle_t *th_p)
{
	td_err_e	return_val = TD_OK;
	prgregset_t	gregset;
	ps_err_e	db_return;
	uthread_t	ts;
	uthread_t	*t0val;
	psaddr_t	t0addr;
	struct ps_prochandle	*ph_p;
#ifdef  _SYSCALL32_IMPL
	uthread32_t	ts32;
	uthread32_t	*t0val32;
#endif /* _SYSCALL32_IMPL */
	int		model = PR_MODEL_NATIVE;

#ifdef TEST_PS_CALLS
	if (td_noop) {
		(void) ps_lgetregs(0, 0, 0);
		(void) ps_pdread(0, 0, 0, 0);
		return (TD_OK);
	}
#endif

	/*
	 * Sanity checks.
	 */
	if (ta_p == NULL) {
		return (TD_BADTA);
	}
	/*
	 * Get a reader lock.
	 */
	if (rw_rdlock((rwlock_t *)&(ta_p->rwlock))) {
		return_val = TD_ERR;
		return (return_val);
	}
	if (ta_p->ph_p == NULL) {
		rw_unlock((rwlock_t *)&(ta_p->rwlock));
		return (TD_BADPH);
	}
	ph_p = ta_p->ph_p;

	/*
	 * Make sure libthread has completed initialization;
	 * otherwise, there is no thread to return.
	 */
	/*
	 * Pick up the address of _t0 separately (i.e., not as
	 * part of the _t0 data structure) for now.
	 */
	if (ps_pglobal_lookup(ph_p, TD_LIBTHREAD_NAME,
	    TD_T0_NAME, &t0addr) != PS_OK) {
		rw_unlock((rwlock_t *) &ta_p->rwlock);
		return (TD_DBERR);
	}

	if (__td_pdmodel(ph_p, &model) != 0) {
		return (TD_ERR);
	}
	if (model == PR_MODEL_NATIVE) {
		if (ps_pdread(ph_p,
		    t0addr, (char *) &t0val, sizeof (t0val)) != PS_OK)
			return_val = TD_DBERR;
		else if (t0val == NULL)
			return_val = TD_NOTHR;
		else if (ps_pstop(ph_p) != PS_OK)
			return_val = TD_DBERR;
	}
#ifdef  _SYSCALL32_IMPL
	else {
		if (ps_pdread(ph_p,
		    t0addr, (char *) &t0val32, sizeof (t0val32)) != PS_OK)
			return_val = TD_DBERR;
		else if (t0val32 == NULL)
			return_val = TD_NOTHR;
		else if (ps_pstop(ph_p) != PS_OK)
			return_val = TD_DBERR;
	}
#endif /* _SYSCALL32_IMPL */

	if (return_val != TD_OK) {
		rw_unlock((rwlock_t *)&(ta_p->rwlock));
		return (return_val);
	}

	/*
	 * Extract %g7 from register set.  This is done so that early use of
	 * Interface can be made before all the thread initialization has
	 * been done.  Is %g7 set earlier that the list of all threads?
	 */

	db_return = ps_lgetregs(ph_p, lwpid, gregset);

	if (db_return != PS_OK) {
		return_val = TD_DBERR;
	} else {
		th_p->th_unique = (psaddr_t) gregset[R_G7];

		/*
		 * That thread struct pointer is not NIL
		 */
		if (th_p->th_unique == NULL) {
			return_val = TD_NOTHR;
		} else {
			if (model == PR_MODEL_NATIVE) {
				/*
				 * Check that lwpid matches.
				 */
				if (__td_read_thr_struct(ta_p, th_p->th_unique,
						&ts) == TD_OK) {
					if (ts.t_lwpid == lwpid) {
						th_p->th_ta_p =
							(td_thragent_t *) ta_p;
						return_val = TD_OK;
					} else {
						return_val = TD_NOTHR;
					}
				} else {
					return_val = TD_NOTHR;
				}
			}
#ifdef  _SYSCALL32_IMPL
			else {
				/*
				 * Check that lwpid matches.
				 */
				if (__td_read_thr_struct32(ta_p,
						th_p->th_unique,
						&ts32) == TD_OK) {
					if (ts32.t_lwpid == lwpid) {
						th_p->th_ta_p =
							(td_thragent_t *) ta_p;
						return_val = TD_OK;
					} else {
						return_val = TD_NOTHR;
					}
				} else {
					return_val = TD_NOTHR;
				}
			}
#endif /* _SYSCALL32_IMPL */
		}
	}
	if (ps_pcontinue(ph_p) != PS_OK)
		return_val = TD_DBERR;
	rw_unlock((rwlock_t *)&(ta_p->rwlock));
	return (return_val);

}
