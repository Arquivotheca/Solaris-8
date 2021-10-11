/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)xtd_to.c	1.4	99/08/10 SMI"

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
 *   Convert the general registers from 32-bit to native form.
 * It is not possible to memcpy directly into regset because size
 * is quite different than that of the 32-bit version.
 *
 * Input:
 *   regset32 - Values of general purpose registers from 32-bit client.
 *              (See sys/procfs_isa.h)
 *
 * Output:
 *   regset - Values of general purpose registers(see
 * 		sys/procfs_isa.h)
 *
 * Side effect:
 *   none
 */
static void
regset_32ton(prgregset32_t regset32, prgregset_t regset)
{
	int i;

	for (i = 0; i < 32; i++)
		regset[i] = (greg_t)(uint32_t)regset32[i];
	regset[R_CCR] = (greg_t)(uint32_t)regset32[R_CCR];
	regset[R_PC] = (greg_t)(uint32_t)regset32[R_PC];
	regset[R_nPC] = (greg_t)(uint32_t)regset32[R_nPC];
	regset[R_Y] = (greg_t)(uint32_t)regset32[R_Y];
	regset[R_ASI] = (greg_t)(uint32_t)regset32[R_ASI];
	regset[R_FPRS] = (greg_t)(uint32_t)regset32[R_FPRS];
}

/*
 * Description:
 *   Convert the general registers from native to 32-bit form.
 * It is not possible to pdwrite directly because size of regset
 * is quite different than that of rwindow32.
 *
 * Input:
 *   regset - Values of general purpose registers.
 *              (See sys/procfs_isa.h)
 *
 * Output:
 *   regset32 - Values of general purpose registers from 32-bit client. (See
 * 		sys/procfs_isa.h)
 *
 * Side effect:
 *   none
 */
static void
regset_nto32(const prgregset_t regset, prgregset32_t regset32)
{
	int i;

	for (i = 0; i < 32; i++)
		regset32[i] = (greg32_t)regset[i];
	regset32[R_CCR] = (greg32_t)regset[R_CCR];
	regset32[R_PC] = (greg32_t)regset[R_PC];
	regset32[R_nPC] = (greg32_t)regset[R_nPC];
	regset32[R_Y] = (greg32_t)regset[R_Y];
	regset32[R_ASI] = (greg32_t)regset[R_ASI];
	regset32[R_FPRS] = (greg32_t)regset[R_FPRS];
}

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
	uthread32_t	thr_struct32;
	prgregset32_t	regset32;
	int		model = PR_MODEL_NATIVE;

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

		if (__td_pdmodel(ph_p, &model) != 0) {
			return (TD_ERR);
		}
		if (model == PR_MODEL_NATIVE) {
			td_return = __td_read_thr_struct(th_p->th_ta_p,
			    th_p->th_unique, &thr_struct);
			if (td_return == TD_OK) {
				if (ISVALIDLWP(&thr_struct)) {
					if (ps_lgetregs(ph_p,
						thr_struct.t_lwpid,
						regset) == PS_ERR) {
						return_val = TD_DBERR;
					}
				} else {
					/*
					 * Set all regset to zero so that
					 * values not set below are zero.
					 * This is a friendly value.
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
						if (ps_pdread(ph_p,
						    thr_sp + STACK_BIAS,
						    (char *)&regset[R_L0],
						    sizeof (struct rwindow))
						    != PS_OK) {
							return_val = TD_DBERR;
							__td_report_po_err(
							    return_val,
							    XTDT_M3);
						}
					}
				}
			} else {
				return_val = TD_ERR;
			}
		} else {
			td_return = __td_read_thr_struct32(th_p->th_ta_p,
			    th_p->th_unique, &thr_struct32);
			if (td_return == TD_OK) {
				if (ISVALIDLWP(&thr_struct32)) {
					if (ps_lgetregs(ph_p,
						thr_struct32.t_lwpid,
						regset) == PS_ERR) {
						return_val = TD_DBERR;
					}
				} else {
					/*
					 * Set all regset to zero so that
					 * values not set below are zero.
					 * This is a friendly value.
					 */
					memset(regset32, 0,
					    sizeof (prgregset32_t));
					return_val = TD_PARTIALREG;

					/*
					 * Get G7 from thread handle,
					 * SP from thread struct
					 * and FP from register window.
					 * Register window is
					 * saved on the stack.
					 */
					regset32[R_G7] = th_p->th_unique;
					regset32[R_O6] = thr_struct32.t_sp;
					regset32[R_PC] = thr_struct32.t_pc;
					thr_sp = (psaddr_t)(uint32_t)
						thr_struct32.t_sp;
					if (thr_sp != NULL) {
						if (ps_pdread(ph_p,
						    thr_sp,
						    (char *)&regset32[R_L0],
						    sizeof (struct rwindow32))
						    != PS_OK) {
							return_val = TD_DBERR;
							__td_report_po_err(
							    return_val,
							    XTDT_M3);
						}
					}
					regset_32ton(regset32, regset);
				}
			} else {
				return_val = TD_ERR;
			}
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
	uthread32_t	thr_struct32;
	prgregset32_t	regset32;
	int		model = PR_MODEL_NATIVE;

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

		if (__td_pdmodel(ph_p, &model) != 0) {
			return (TD_ERR);
		}
		if (model == PR_MODEL_NATIVE) {
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
					    thr_struct.t_lwpid,
					    regset) == PS_ERR) {
						return_val = TD_DBERR;
					}
				} else {
					thr_sp = thr_struct.t_sp;

					if (thr_sp) {

						/*
						 * Write back the local and in
						 * register values to the
						 * stack.
						 */
						if (ps_pdwrite(ph_p,
						    thr_sp + STACK_BIAS,
						    (char *) &regset[R_L0],
						    sizeof (struct rwindow))
						    != PS_OK) {
							return_val = TD_DBERR;
							__td_report_po_err(
							    return_val,
							    XTDT_M1);
						}

						/*
						 * Thread does not have
						 * associated lwp. Modify
						 * thread %i and %o registers.
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
		} else {
			/*
			 * Extract the thread struct address
			 * from the thread handle and read
			 * the thread struct.
			 */

			td_return = __td_read_thr_struct32(th_p->th_ta_p,
				th_p->th_unique, &thr_struct32);
			if (td_return == TD_OK) {
				if (ISVALIDLWP(&thr_struct32)) {
					/*
					 * Thread has an associated lwp.
					 * Write regsiters
					 * back to lwp.
					 */
					if (ps_lsetregs(ph_p,
					    thr_struct32.t_lwpid,
					    regset) == PS_ERR) {
						return_val = TD_DBERR;
					}
				} else {
					thr_sp = (psaddr_t) thr_struct32.t_sp;

					if (thr_sp) {

						/*
						 * Write back the local and in
						 * register values to the
						 * stack.
						 */
						regset_nto32(regset,
						    regset32);
						if (ps_pdwrite(ph_p,
						    thr_sp,
						    (char *) &regset32[R_L0],
						    sizeof (struct rwindow32))
						    != PS_OK) {
							return_val = TD_DBERR;
							__td_report_po_err(
							    return_val,
							    XTDT_M1);
						}

						/*
						 * Thread does not have
						 * associated lwp. Modify
						 * thread %i and %o registers.
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
* Not applicable on sparcv9 because getting the upper half of registers
* is not necessary when running a 64-bit OS.  Return an error instead.
*/
td_err_e
__td_thr_getxregsize(const td_thrhandle_t *th_p, int *xregsize)
{
	return (TD_NOXREGS);
}

/*
* Description:
*   Get the extra registers for the given thread.
* Not applicable on sparcv9 because getting the upper half of registers
* is not necessary when running a 64-bit OS.  Return an error instead.
*/
td_err_e
__td_thr_getxregs(const td_thrhandle_t *th_p, void *xregset)
{
	return (TD_NOXREGS);
}
/*
* Description:
*   Set the extra registers for the given thread.
* Not applicable on sparcv9 because getting the upper half of registers
* is not necessary when running a 64-bit OS.  Return an error instead.
*/
td_err_e
__td_thr_setxregs(const td_thrhandle_t *th_p, const void *xregset)
{
	return (TD_NOXREGS);
}
