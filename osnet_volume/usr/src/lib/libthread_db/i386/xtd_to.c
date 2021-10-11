/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)xtd_to.c	1.28	99/08/10 SMI"

/*
*  Description:
* This module contains functions for interacting with
* the threads within the program.
*/

#pragma weak td_thr_getgregs = __td_thr_getgregs /* i386 work around */
#pragma weak td_thr_setgregs = __td_thr_setgregs /* i386 work around */
#pragma weak td_thr_getxregsize = __td_thr_getxregsize
#pragma weak td_thr_setxregs = __td_thr_setxregs
#pragma weak td_thr_getxregs = __td_thr_getxregs

#include <thread_db.h>
#include <signal.h>
#include "td.h"
#include "xtd_to.h"
#include "td.extdcl.h"
#include "xtd.extdcl.h"

#define	XTDT_M1 "Writing thread struct: td_thr_setregs"

/*
* Description:
* 	Get the general registers for a given
* thread.
*
* Input: *th_p - thread handle
*
* Output: *regset - Values of general purpose registers - see
* 		sys/procfs_isa.h
* 	td_thr_getgregs - return value
*
* Side effect:
* 	Imported functions called: ps_pstop, ps_pcontinue, ps_pdread,
* ps_lgetregs.
*/

td_err_e
__td_thr_getgregs(const td_thrhandle_t *th_p, prgregset_t regset)
{
	td_err_e	return_val = TD_OK;
	td_err_e	td_return;
	uthread_t	thr_struct;

	psaddr_t		thr_sp;
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

	/*
	 * More than 1 byte is geing read.  Stop the process.
	 */

	if (ps_pstop(th_p->th_ta_p->ph_p) == PS_OK) {


		/*
		 * Extract the thread struct address
		 * from the thread handle and read
		 * the thread struct.
		 */

		td_return = __td_read_thr_struct(th_p->th_ta_p,
			th_p->th_unique, &thr_struct);


		if (td_return == TD_OK) {
			if (ISVALIDLWP(&thr_struct)) {
				if (ps_lgetregs(th_p->th_ta_p->ph_p,
						thr_struct.t_lwpid,
						regset) == PS_ERR) {
					return_val = TD_DBERR;
				}
			} else {

				/*
				 * Zero all os regset so that register not
				 * saved are zero.
				 */
				memset(regset, 0, sizeof (prgregset_t));
				/*
				 * i386 only these registers are saved.
				 */
				regset[EBP] = (thr_struct).t_resumestate.rs_bp;
				regset[EDI] = (thr_struct).t_resumestate.rs_edi;
				regset[ESI] = (thr_struct).t_resumestate.rs_esi;
				regset[EBX] = (thr_struct).t_resumestate.rs_ebx;
				regset[UESP] =
					(thr_struct).t_resumestate.rs_uesp;
				regset[EIP] = (thr_struct).t_resumestate.rs_pc;

				return_val = TD_PARTIALREG;
			}
		} else {
			return_val = TD_ERR;
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
*   Set the general registers for a given
* thread.
*
* Input:
*   *th_p -  thread handle
*   *regset - Values of general purpose registers - see
* sys/procfs_isa.h
*
* Output:
*   td_thr_setgregs - return value
*
* Side effect:
*   The general purpose registers for the thread are changed
* if the thread is on an lwp.
*   Imported functions called: ps_pstop, ps_pcontinue, ps_pdread,
* ps_lgetregs.
*/

td_err_e
__td_thr_setgregs(const td_thrhandle_t *th_p, const prgregset_t regset)
{
	td_err_e	return_val = TD_OK;
	td_err_e	td_return;
	uthread_t	thr_struct;
	psaddr_t	thr_sp;

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


	/*
	 * More than 1 byte is geing read.  Stop the process.
	 */

	if (ps_pstop(th_p->th_ta_p->ph_p) == PS_OK) {

		/*
		 * Extract the thread struct address
		 * from the thread handle and read
		 * the thread struct.
		 */

		td_return = __td_read_thr_struct(th_p->th_ta_p,
			th_p->th_unique, &thr_struct);


		if (td_return == TD_OK) {
			if (ISONPROC(&thr_struct) || ISBOUND(&thr_struct) ||
					ISPARKED(&thr_struct)) {

				/*
				 * Thread has an associated lwp.
				 * Write regsiters
				 * back to lwp.
				 */
				if (ps_lsetregs(th_p->th_ta_p->ph_p,
						thr_struct.t_lwpid,
						regset) == PS_ERR) {
					return_val = TD_DBERR;
				}
			} else {

				thr_sp = (thr_struct).t_resumestate.rs_uesp;

				if (thr_sp) {

					/*
					 * Thread does not
					 * have associated lwp.
					 * Modify registers held
					 * in thread struct.
					 */

					/*
					 * i386 only these registers are saved.
					 */
					(thr_struct).t_resumestate.rs_bp =
						regset[EBP];
					(thr_struct).t_resumestate.rs_edi =
						regset[EDI];
					(thr_struct).t_resumestate.rs_esi =
						regset[ESI];
					(thr_struct).t_resumestate.rs_ebx =
						regset[EBX];
					(thr_struct).t_resumestate.rs_uesp =
						regset[UESP];
					(thr_struct).t_resumestate.rs_pc =
						regset[EIP];

					/*
					 * Write back the thread struct.
					 */
					td_return = __td_write_thr_struct(
						th_p->th_ta_p,
						th_p->th_unique,
						&thr_struct);
					if (td_return != TD_OK) {
						return_val = TD_DBERR;
						__td_report_po_err(return_val,
							XTDT_M1);
					} else {
						return_val = TD_PARTIALREG;
					}
				}
			}
		} else {
			return_val = TD_ERR;
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
* Get the size of the SPARC-specific extra register set for the given thread.
* On non-SPARC architectures, return an error code.
*/
td_err_e
__td_thr_getxregsize(const td_thrhandle_t *th_p, int *xregsize)
{
	return (TD_NOXREGS);
}

/*
* Description:
* Get the SPARC-specific extra registers for the given thread.  On
* non-SPARC architectures, return an error code.
*/
td_err_e
__td_thr_getxregs(const td_thrhandle_t *th_p, void *xregset)
{
	return (TD_NOXREGS);
}

/*
* Description:
* Set the SPARC-specific extra registers for the given thread.  On
* non-SPARC architectures, return an error code.
*/

td_err_e
__td_thr_setxregs(const td_thrhandle_t *th_p, const void *xregset)
{
	return (TD_NOXREGS);
}
