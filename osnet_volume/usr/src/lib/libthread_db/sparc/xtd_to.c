/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)xtd_to.c	1.38	99/08/10 SMI"

/*
*  Description:
* This module contains functions for interacting with
* the threads within the program.
*/

#pragma weak td_thr_getgregs = __td_thr_getgregs
#pragma weak td_thr_setgregs = __td_thr_setgregs
#pragma weak td_thr_getxregsize = __td_thr_getxregsize
#pragma weak td_thr_setxregs = __td_thr_setxregs
#pragma weak td_thr_getxregs = __td_thr_getxregs

#include <thread_db.h>
#include <signal.h>
#include "td.h"
#include "xtd_to.h"
#include "td.extdcl.h"

/* These are used to keep lines <80 characters. */
#define	XTDT_M1 "Writing rwin to stack: td_thr_setregs"
#define	XTDT_M3 "Reading rwin from stack: td_thr_getregs"

/*
* Description:
*   Get the general registers for a given thread.  For a
* thread that is currently executing on an LWP, (td_thr_state_e)
* TD_THR_ACTIVE, all registers in regset will be read for the
* thread.  For a thread not executing on an LWP, only the
* following registers will be read.
*
*   %i0-%i7,
*   %l0-%l7,
*   %g7, %pc, %sp(%o6).
*
* %pc and %sp will be the program counter and stack pointer
* at the point where the thread will resume execution
* when it becomes active, (td_thr_state_e) TD_THR_ACTIVE.
*
* Input:
*   *th_p - thread handle
*
* Output:
*   *regset - Values of general purpose registers(see
* 		sys/procfs_isa.h)
*   td_thr_getgregs - return value
*
* Side effect:
*   none
*   Imported functions called: ps_pstop, ps_pcontinue, ps_pdread, ps_lgetregs.
*/
td_err_e
__td_thr_getgregs(const td_thrhandle_t *th_p, prgregset_t regset)
{
	td_err_e	return_val = TD_OK;
	td_err_e	td_return;
	uthread_t	thr_struct;
	struct rwindow	reg_window;
	psaddr_t	thr_sp;
	struct ps_prochandle	*ph_p;

#ifdef TEST_PS_CALLS
	if (td_noop) {
		(void) ps_pstop(0);
		(void) ps_pcontinue(0);
		(void) ps_pdread(0, 0, 0, 0);
		(void) ps_lgetregs(0, 0, 0);
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
	if (regset == NULL) {
		return (TD_ERR);
	}

	/*
	 * Get a reader lock.
	 */
	if (rw_rdlock(&(th_p->th_ta_p->rwlock))) {
		return_val = TD_ERR;
		return (return_val);
	}

	if (th_p->th_ta_p->ph_p == NULL) {
		rw_unlock(&(th_p->th_ta_p->rwlock));
		return_val = TD_BADTA;
		return (return_val);
	}

	ph_p = th_p->th_ta_p->ph_p;

	/*
	 * More than 1 byte is being read.  Stop the process.
	 */

	if (ps_pstop(ph_p) == PS_OK) {

		/*
		 * Extract the thread struct address
		 * from the thread handle and read
		 * the thread struct.
		 */

		td_return = __td_read_thr_struct(th_p->th_ta_p,
		    th_p->th_unique, &thr_struct);
		if (td_return == TD_OK) {
			if (ISVALIDLWP(&thr_struct)) {
				if (ps_lgetregs(ph_p, thr_struct.t_lwpid,
				    regset) == PS_ERR) {
					return_val = TD_DBERR;
				}
			} else {
				/*
				 * Set all regset to zero so that values not
				 * set below are zero.  This is a friendly
				 * value.
				 */
				memset(regset, 0, sizeof (prgregset_t));
				return_val = TD_PARTIALREG;

				/*
				 * Get G7 from thread handle,
				 * SP from thread struct
				 * and FP from register window.
				 * Register window is
				 * saved on the stack.
				 */
				regset[R_G7] = th_p->th_unique;
				regset[R_O6] = thr_struct.t_sp;
				regset[R_PC] = thr_struct.t_pc;
				thr_sp = thr_struct.t_sp;
				if (thr_sp != NULL) {
					if (ps_pdread(
					    ph_p,
					    thr_sp + STACK_BIAS,
					    (char *) &reg_window,
					    sizeof (reg_window)) != PS_OK) {
						return_val = TD_DBERR;
						__td_report_po_err(return_val,
						    XTDT_M3);
					} else {
						(void) memcpy(&regset[R_L0],
						    &reg_window,
						    sizeof (reg_window));
					}
				}
			}
		} else {
			return_val = TD_ERR;
		}

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
*   Set the general registers for a given
* thread.  For a thread that is currently executing on
* an LWP, (td_thr_state_e) TD_THR_ACTIVE, all registers
* in regset will be written for the thread.  For a thread
* not executing on an LWP, only the following registers
* will be written
*
*   %i0-%i7,
*   %l0-%l7,
*   %pc, %sp(%o6).
*
* %pc and %sp will be the program counter and stack pointer
* at the point where the thread will resume execution
* when it becomes active, (td_thr_state_e) TD_THR_ACTIVE.
*
* Input:
*   *th_p -  thread handle
*   *regset - Values of general purpose registers(see
* 	sys/procfs_isa.h)
*
* Output:
*   td_thr_setgregs - return value
*
* Side effect:
*   The general purpose registers for the thread are changed.
*   Imported functions called: ps_pstop, ps_pcontinue, ps_pdread, ps_lsetregs
*
*/

td_err_e
__td_thr_setgregs(const td_thrhandle_t *th_p, const prgregset_t regset)
{
	td_err_e	return_val = TD_OK;
	td_err_e	td_return;
	uthread_t	thr_struct;
	psaddr_t	thr_sp;
	struct ps_prochandle	*ph_p;

#ifdef TEST_PS_CALLS
	if (td_noop) {
		(void) ps_pstop(0);
		(void) ps_pcontinue(0);
		(void) ps_pdread(0, 0, 0, 0);
		(void) ps_pdwrite(0, 0, 0, 0);
		(void) ps_lsetregs(0, 0, 0);
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
	if (regset == NULL) {
		return (TD_ERR);
	}

	/*
	 * Get a reader lock.
	 */
	if (rw_rdlock(&(th_p->th_ta_p->rwlock))) {
		return_val = TD_ERR;
		return (return_val);
	}

	if (th_p->th_ta_p->ph_p == NULL) {
		rw_unlock(&(th_p->th_ta_p->rwlock));
		return_val = TD_BADTA;
		return (return_val);
	}

	ph_p = th_p->th_ta_p->ph_p;

	/*
	 * More than 1 byte is geing read.  Stop the process.
	 */

	if (ps_pstop(ph_p) == PS_OK) {

		/*
		 * Extract the thread struct address
		 * from the thread handle and read
		 * the thread struct.
		 */

		td_return = __td_read_thr_struct(th_p->th_ta_p,
			th_p->th_unique, &thr_struct);
		if (td_return == TD_OK) {
			if (ISVALIDLWP(&thr_struct)) {
				/*
				 * Thread has an associated lwp.
				 * Write regsiters
				 * back to lwp.
				 */
				if (ps_lsetregs(ph_p,
				    thr_struct.t_lwpid, regset) == PS_ERR) {
					return_val = TD_DBERR;
				}
			} else {
				thr_sp = thr_struct.t_sp;

				if (thr_sp) {

					/*
					 * Write back the local and in register
					 * values to the stack.
					 */
					if (ps_pdwrite(ph_p,
					    thr_sp + STACK_BIAS,
					    (char *) &regset[R_L0],
					    sizeof (struct rwindow)) != PS_OK) {
						return_val = TD_DBERR;
						__td_report_po_err(return_val,
						    XTDT_M1);
					}

					/*
					 * Thread does not have associated lwp.
					 * Modify thread %i and %o registers.
					 */

					/*
					XXXX
					Don 't change the values of
					the struct
					thread pointer and stack
					point in the thread
					handle nor thread struct.
					We may do something
					later if required.
					th_p->th_unique = regset[R_G7];
					thr_struct.t_sp = regset[R_O6];
					*/
				}	/* Good stack pointer  */
				else {
					return_val = TD_ERR;
				}
			}   /*   Thread not on lwp  */
		}   /*   Read thread data ok  */
		else {
			return_val = TD_ERR;
		}

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
*   Get the size of the extra register set for the given thread.
* The extra registers are only available for a thread executing
* on and LWP.  If the thread is not running on an LWP, this
* function will return TD_NOXREGS.
*
* Input:
*   *th_p - thread handle for which the extra register set size
* is being requested.
*
* Output:
*   *xregsize - size of the extra register set.
*   td_thr_getxregsize - return value.
*
* Side Effect:
*   none
*   Imported functions called:
*/
td_err_e
__td_thr_getxregsize(const td_thrhandle_t *th_p, int *xregsize)
{

	td_err_e	return_val = TD_OK;
	td_err_e	td_return;
	uthread_t	thr_struct;
	struct ps_prochandle	*ph_p;

#ifdef TEST_PS_CALLS
	if (td_noop) {
		(void) ps_pstop(0);
		(void) ps_pcontinue(0);
		(void) ps_pdread(0, 0, 0, 0);
		(void) ps_pdwrite(0, 0, 0, 0);
		(void) ps_lgetxregsize(0, 0, 0);
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
	if (xregsize == NULL) {
		return_val = TD_ERR;
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

	ph_p = th_p->th_ta_p->ph_p;

	/*
	 * More than 1 byte is geing read.  Stop the process.
	 */

	if (ps_pstop(ph_p) == PS_OK) {

		/*
		* Extract the thread struct address
		* from the thread handle and read
		* the thread struct.
		*/

		td_return = __td_read_thr_struct(th_p->th_ta_p,
			th_p->th_unique, &thr_struct);

		if (td_return == TD_OK) {

			/*
			* Read the extra registers using the imported
			* interface.
			*/

			if (ISVALIDLWP(&thr_struct)) {
				if (ps_lgetxregsize(ph_p,
				    thr_struct.t_lwpid, xregsize) ==
				    PS_ERR) {
					return_val = TD_DBERR;
				}
				if (!HASVALIDFP(&thr_struct)) {
					return_val = TD_NOFPREGS;
				}
			} else {
				return_val = TD_NOFPREGS;
			}
		} else {
			return_val = TD_ERR;
		}
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
*   Get the extra registers for the given thread.  Extra register can
* only be gotten for a thread executing on an LWP.  This
* operation will return TD_NOFPREGS
* for thread not on an LWP.
*
* Input:
*   *th_p - thread handle for thread on which extra registers
* are being requested.
*
* Output:
*   *xregset - extra register set, see sys/procfs_isa.h.
*   td_thr_getxregs - return value
*
* Side Effect:
*   None
*   Imported function called: ps_pstop, ps_pcontinue, ps_pdread,
* ps_pdwrite, ps_lgetxregs.
*/
td_err_e
__td_thr_getxregs(const td_thrhandle_t *th_p, void *xregset)
{
	td_err_e	return_val = TD_OK;
	td_err_e	td_return;
	uthread_t	thr_struct;
	struct ps_prochandle	*ph_p;

#ifdef TEST_PS_CALLS
	if (td_noop) {
		(void) ps_pstop(0);
		(void) ps_pcontinue(0);
		(void) ps_pdread(0, 0, 0, 0);
		(void) ps_pdwrite(0, 0, 0, 0);
		(void) ps_lgetxregs(0, 0, 0);
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
	if (xregset == NULL) {
		return (TD_ERR);
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

	ph_p = th_p->th_ta_p->ph_p;

	/*
	 * More than 1 byte is geing read.  Stop the process.
	 */

	if (ps_pstop(ph_p) == PS_OK) {
		/*
		* Extract the thread struct address
		* from the thread handle and read
		* the thread struct.
		*/

		td_return = __td_read_thr_struct(th_p->th_ta_p,
			th_p->th_unique, &thr_struct);
		if (td_return == TD_OK) {

			/*
			* Read the x registers using the imported
			* interface.
			*/

			if (ISVALIDLWP(&thr_struct)) {
				if (ps_lgetxregs(ph_p,
					thr_struct.t_lwpid, xregset)
					== PS_ERR) {
					return_val = TD_DBERR;
				}
				if (!HASVALIDFP(&thr_struct)) {
					/*
					 * Let user know floating pointer
					 * registers are not available.
					 */
					return_val = TD_NOFPREGS;
				}
			} else {
				return_val = TD_NOXREGS;
			}
		} else {
			return_val = TD_ERR;
		}

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
*   Set the extra registers for the given thread.  Extra register can
* only be set on a thread in on an LWP.  This operation will return
* TD_NOFPREGS for thread not on and LWP.
*
* Input:
*   *th_p - thread handle for thread on which extra registers
* are being set.
*   *xregset - extra register set values, see sys / procfs_isa.h.
*
* Output:
*   td_thr_setxregs - return value.  TD_NOCAPAB will be
* returned if x registers are not available from the
* thread.
*
* Side Effect:
*   Extra registers in the thread corresponding to
* *th_p are set.
*   Imported functions called: ps_pdread, ps_pdwrite, ps_lsetxregs.
* ps_pstop, ps_pcontinue.
*/
td_err_e
__td_thr_setxregs(const td_thrhandle_t *th_p, const void *xregset)
{
	td_err_e	return_val = TD_OK;
	td_err_e	td_return;
	uthread_t	thr_struct;
	struct ps_prochandle	*ph_p;

#ifdef TEST_PS_CALLS
	if (td_noop) {
		(void) ps_pstop(0);
		(void) ps_pcontinue(0);
		(void) ps_pdread(0, 0, 0, 0);
		(void) ps_pdwrite(0, 0, 0, 0);
		(void) ps_lsetxregs(0, 0, 0);
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
	if (xregset == NULL) {
		return (TD_ERR);
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

	ph_p = th_p->th_ta_p->ph_p;

	/*
	 * More than 1 byte is geing read.  Stop the process.
	 */

	if (ps_pstop(ph_p) == PS_OK) {

		/*
		* Extract the thread struct address from
		* the thread handle and read
		* the thread struct.
		*/

		td_return =
			__td_read_thr_struct(th_p->th_ta_p,
			th_p->th_unique,
			&thr_struct);

		if (td_return == TD_OK) {
			if (ISVALIDLWP(&thr_struct)) {

				/*
				* Write the x registers
				* using the imported interface.
				*/
				if (ps_lsetxregs(ph_p,
					thr_struct.t_lwpid,
					(caddr_t)xregset) == PS_ERR) {
					return_val = TD_DBERR;
				}
				if (!HASVALIDFP(&thr_struct)) {
					/*
					 * Let user now that floating
					 * point registeres are not
					 * available.
					 */
					return_val = TD_NOFPREGS;
				}
			} else {
				return_val = TD_NOXREGS;
			}
		} else {
			return_val = TD_ERR;
		}

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
