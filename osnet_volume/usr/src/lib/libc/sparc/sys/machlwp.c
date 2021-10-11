/*
 * Copyright (c) 1992-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)machlwp.c	1.17	98/11/06 SMI"

#pragma weak _lwp_makecontext = __lwp_makecontext

/*LINTLIBRARY*/

#include "synonyms.h"
#include <memory.h>
#include <sys/types.h>
#include <sys/stack.h>
#include <sys/ucontext.h>
#include <sys/lwp.h>
#include "libc.h"

/*
 * It is assumed that the ucontext_t structure has already been filled
 * with valid context information.  Here, we update the structure
 * so that when it is passed to _lwp_create() the newly-created
 * lwp will begin execution in the specified function with the
 * specified stack, properly initialized.
 *
 * However, the ucontext_t structure may contain uninitialized data.
 * We must be sure that this does not cause _lwp_create() to malfunction.
 * _lwp_create() only uses the signal mask and the general registers.
 */
void
_lwp_makecontext(ucontext_t *ucp, void ((*func)(void *)),
    void *arg, void *private, caddr_t stk, size_t stksize)
{
	/*
	 * Top-of-stack must be rounded down to STACK_ALIGN and
	 * there must be a minimum frame for the register window.
	 */
	uintptr_t stack = (((uintptr_t)stk + stksize) & ~(STACK_ALIGN - 1)) -
	    SA(MINFRAME);

	/* clear the top stack frame */
	(void) memset((void *)stack, 0, SA(MINFRAME));

	/* fill in registers of interest */
	ucp->uc_flags |= UC_CPU;
	ucp->uc_mcontext.gregs[REG_PC] = (greg_t)func;
	ucp->uc_mcontext.gregs[REG_nPC] = (greg_t)func + 4;
	ucp->uc_mcontext.gregs[REG_O0] = (greg_t)arg;
	ucp->uc_mcontext.gregs[REG_SP] = (greg_t)(stack - STACK_BIAS);
	ucp->uc_mcontext.gregs[REG_O7] = (greg_t)_lwp_exit - 8;
	ucp->uc_mcontext.gregs[REG_G7] = (greg_t)private;
	/*
	 * clear extra register state information
	 */
	(void) _xregs_clrptr(ucp);
}
