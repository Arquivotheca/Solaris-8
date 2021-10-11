/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_CANCEL_H
#define	_SYS_CANCEL_H

#pragma ident	"@(#)SYS_CANCEL.h	1.12	99/12/08 SMI"

#include <sys/asm_linkage.h>
#include <sys/stack.h>
#include <assym.h>

/*
 * This wrapper provides cancellation point to calling function.
 * It is assumed that function "name" is interposbale and has
 * "_name" defined in respective library, for example libc defines
 * "read" as weak as well as "_read" as strong symbols.
 *
 * This wrapper turns on cancelabilty before calling "-" version of
 * call, restore the the cancelabilty type after return.
 *
 * 	name:
 *		if (cancellation is DISBALED)
 *			go to _name; /returns directly to caller
 *		save previous cancel type
 *		make cancel type = ASYNC (-1) /both ops atomic
 *		if (cancel pending)
 *			_thr_exit(PTHREAD_CANCELED);
 *		_name;
 *		retore previous type;
 *		return;
 *
 * For sparc, we need temp storage to save previous type and %o7.
 * We use 6 words of callee's stack which are used to save arguments
 * in case address is required. These words start from ARGPUSH (stack.h).
 * FIRST_ARG is used by libc for restartable system calls. We will leave
 * SECOND_ARG for future use by libc.
 */

#define	FIRST_ARG	ARGPUSH		/* used by libc */
#define	SECOND_ARG	ARGPUSH+4	/* future use by libc */
#define	THIRD_ARG	ARGPUSH+8	/* to store cancel type */
#define	FOURTH_ARG	ARGPUSH+12	/* to store caller's ret addr */
#define	FIFTH_ARG	ARGPUSH+16	/* not used */
#define	SIXTH_ARG	ARGPUSH+20	/* not used */

#define	SYSCALL_CANCELPOINT(name, newname) \
	ENTRY_NP(name); \
	ldsb	[%g7 + T_CANSTATE], %g1	/* state = t->t_can_state	*/; \
	cmp	%g1, TC_DISABLE		/* if (state == DISABLE)	*/; \
	be,a	2f			/* 	go to 2 		*/; \
	mov	%o7, %g1		/* 	*save ret addr of caller */; \
	ldstub	[%g7 + T_CANTYPE], %g1	/* type = t->t_can_type		*/; \
					/* t->t_can_type = ASYNC(-1)	*/; \
	stb	%g1, [%sp + THIRD_ARG]	/* save type 			*/; \
	ldsb	[%g7 + T_CANPENDING], %g1 /* pending = t->t_can_pending	*/; \
	cmp	%g1, TC_PENDING		/* if (pending is not set)	*/; \
	bne,a	1f			/* 	go to 1f		*/; \
	st	%o7, [%sp + FOURTH_ARG]	/* 	*save ret addr of caller */; \
	save	%sp, -SA(MINFRAME), %sp /* save the frame while exiting  */; \
					/* see bug id 4229175 for details */;\
	call	_pthread_exit		/* exit thread with status	*/; \
	mov	PTHREAD_CANCELED, %o0	/* *status = PTHREAD_CANCELED	*/; \
1:; \
	call	newname			/* call newname			*/; \
	nop; \
	ld	[%sp + FOURTH_ARG], %o7	/* restore ret addr of caller	*/; \
	ldub	[%sp + THIRD_ARG], %g1	/* restore type			*/; \
	retl				/* return with			*/; \
	stb	%g1, [%g7 + T_CANTYPE]	/* *t->t_can_type = type	*/; \
2:;\
	call	newname			/* call newname with old ret addr */; \
	mov	%g1, %o7		/* so that it returns to caller	*/; \
	SET_SIZE(name)

/*
 * Macro to declare a weak symbol alias.  This is similar to
 *	#pragma weak wsym = sym
 */

#define	PRAGMA_WEAK(wsym, sym) \
	.weak	wsym; \
/* CSTYLED */ \
wsym	= sym

#endif	/* _SYS_CANCEL_H */
