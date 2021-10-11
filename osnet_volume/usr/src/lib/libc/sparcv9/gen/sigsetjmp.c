/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright(c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)sigsetjmp.c	1.15	97/04/01 SMI"

#pragma	weak	sigsetjmp = _sigsetjmp

/*LINTLIBRARY*/

#include "synonyms.h"

#define _getcontext __getcontext	/* to get prototype right */

#include <sys/types.h>
#include <sys/stack.h>
#include <ucontext.h>
#include <setjmp.h>
#include "libc.h"


/*
 * The following structure MUST match the ABI size specifier _SIGJBLEN.
 * This is 19 (words). The ABI value for _JBLEN is 12 (words). A sigset_t
 * is 16 bytes and a stack_t is 12 bytes.
 */
typedef struct sigjmp_struct {
	int		sjs_flags;	/* JBUF[ 0]	*/
	greg_t		sjs_sp;		/* JBUF[ 1]	*/
	greg_t		sjs_pc;		/* JBUF[ 2]	*/	
	u_long		sjs_pad[_JBLEN-3];
	sigset_t	sjs_sigmask;
	stack_t		sjs_stack;
} sigjmp_struct_t;

#define	JB_SAVEMASK	0x1

int
sigsetjmp(sigjmp_buf env, int savemask)
{
	ucontext_t uc;
	sigjmp_struct_t *bp = (sigjmp_struct_t *)env;
	greg_t sp = _getsp() + STACK_BIAS;

	/*
	 * Get the current machine context.
	 */
	uc.uc_flags = UC_STACK | UC_SIGMASK;
	(void) __getcontext(&uc);

	/*
	 * Note that the pc and former sp (fp) from the stack are valid
	 * because the call to __getcontext must flush the user windows
	 * to the stack.
	 */
	bp->sjs_flags = 0;
	bp->sjs_sp    = *((greg_t *)sp+14);
	bp->sjs_pc    = *((greg_t *)sp+15) + 0x8;
	bp->sjs_stack = uc.uc_stack;
	if (savemask) {
		bp->sjs_flags |= JB_SAVEMASK;
		bp->sjs_sigmask = uc.uc_sigmask;
	}

	return (0);
}

void
_libc_siglongjmp(sigjmp_buf env, int val)
{
	ucontext_t uc;
	greg_t *reg = uc.uc_mcontext.gregs;
	sigjmp_struct_t *bp = (sigjmp_struct_t *)env;

	/*
	 * Get the current context to use as a starting point to construct
	 * the sigsetjmp context. It might perhaps be more precise to
	 * constuct an entire context from scratch, but this method is
	 * closer to old (undocumented) symantics.
	 */
	uc.uc_flags = UC_ALL;
	(void) __getcontext(&uc);

	/*
	 * Use the information in the sigjmp_buf to modify the current
	 * context to execute as though we returned from sigsetjmp().
	 */
	uc.uc_stack = bp->sjs_stack;
	if (bp->sjs_flags & JB_SAVEMASK)
		uc.uc_sigmask = bp->sjs_sigmask;
	reg[REG_PC] = bp->sjs_pc;
	reg[REG_nPC] = reg[REG_PC] + 0x4;
	reg[REG_SP] = bp->sjs_sp;
	if (val)
		reg[REG_O0] = (greg_t)val;
	else
		reg[REG_O0] = (greg_t)1;

	(void) setcontext(&uc);
}
