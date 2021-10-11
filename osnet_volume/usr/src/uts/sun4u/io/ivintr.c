/*
 * Copyright (c) 1994-1996,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ivintr.c	1.24	99/03/03 SMI"

/*
 * Interrupt Vector Table Configuration
 */

#include <sys/cpuvar.h>
#include <sys/ivintr.h>
#include <sys/intreg.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/privregs.h>
#include <sys/debug.h>

/*
 * nullify an interrupt handler entry
 */
#define	nullify_intr(x) \
	x->iv_handler = NULL; x->iv_arg = 0; x->iv_pil = 0; \
	x->iv_pending = 0;

/*
 * fill in an interrupt vector entry
 */
#define	fill_intr(a, b, c, d) \
	a->iv_pil = b; a->iv_pending = 0; \
	a->iv_arg = d; a->iv_handler = c;

extern u_int swinum_base;
extern u_int maxswinum;
extern kmutex_t soft_iv_lock;

/*
 * add_ivintr() - add an interrupt handler to the system
 *	This routine is not protected by the lock; it's the caller's
 *	responsibility to make sure <source>_INR.INT_EN = 0
 *	and <source>_ISM != PENDING before the routine is called.
 */
void
add_ivintr(u_int inum, u_int pil, intrfunc intr_handler, caddr_t intr_arg)
{
	struct intr_vector *inump;

	ASSERT(inum < MAXIVNUM);
	ASSERT(pil <= PIL_MAX);
	ASSERT((uintptr_t)intr_handler > KERNELBASE);

	inump = &intr_vector[inum];

	fill_intr(inump, (u_short)pil, intr_handler, intr_arg);
}

/*
 * rem_ivintr() - remove an interrupt handler from intr_vector[]
 *	This routine is not protected by the lock; it's the caller's
 *	responsibility to make sure <source>_INR.INT_EN = 0
 *	and <source>_ISM != PENDING before the routine is called.
 */
void
rem_ivintr(u_int inum, struct intr_vector *iv_return)
{
	struct intr_vector *inump;

	ASSERT(inum != NULL && inum < MAXIVNUM);

	inump = &intr_vector[inum];

	/*
	 * the caller requires the current entry to be returned
	 */
	if (iv_return) {
		fill_intr(iv_return, inump->iv_pil, inump->iv_handler,
		    inump->iv_arg);
	}

	/*
	 * nullify the current entry
	 */
	nullify_intr(inump);
}

/*
 * add_softintr() - add a software interrupt handler to the system
 */
u_int
add_softintr(u_int pil, intrfunc intr_handler, caddr_t intr_arg)
{
	struct intr_vector *inump;
	register u_int i;

	mutex_enter(&soft_iv_lock);

	for (i = swinum_base; i < maxswinum; i++) {
		inump = &intr_vector[i];
		if (inump->iv_handler == NULL)
			break;
	}

	if (inump->iv_handler != NULL) {
		cmn_err(CE_PANIC, "add_softintr: exceeded %d handlers",
			maxswinum - swinum_base);
	}

	add_ivintr(i, pil, intr_handler, intr_arg);

	mutex_exit(&soft_iv_lock);

	return (i);
}

/*
 * rem_softintr() - remove a software interrupt handler from the system
 */
void
rem_softintr(u_int inum)
{
	ASSERT(swinum_base <= inum && inum < MAXIVNUM);

	mutex_enter(&soft_iv_lock);
	rem_ivintr(inum, NULL);
	mutex_exit(&soft_iv_lock);
}

/*ARGSUSED*/
/*
 * This is the v9 version of nullintr() in v7/io/nullintr().
 */
u_int
nullintr(caddr_t intrg)
{
	return (0);	/* = return (DDI_INTR_UNCLAIMED) */
}
