/*	Copyright (c) 1984 by Sun Microsystems, Inc.		*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/


.ident	"@(#)SYS_CANCEL.h	1.10	99/12/08 SMI"	/* SVr4.0 1.9	*/

#ifndef _LIBTHREAD_CANCEL_SYS_H
#define	_LIBTHREAD_CANCEL_SYS_H

#include "i386/SYS.h"
#include "assym.s"

/*
 * This wrapper provides cancellation point to calling function.
 * It is assumed that function "name" is interposbale and has
 * "newname" defined in respective library, for example libc defines
 * "read" as weak as well as "_read" as strong symbols.
 *
 * This wrapper turns on cancelabilty before calling "new" version of
 * call, restore the the cancelabilty type after return.
 *
 * 	name:
 *		if (cancellation is DISBALED)
 *			go to newname; /returns directly to caller
 *		save previous cancel type
 *		make cancel type = ASYNC (-1) /both ops atomic
 *		if (cancel pending)
 *			_thr_exit(PTHREAD_CANCELED);
 *		newname;
 *		retore previous type;
 *		return;
 *
 * For x86, arguments need to be copied while calling newname.
 * The common wrapper has no idea about number of arguments
 * to be copied, hence, we require a different wrapper based
 * on # of args passed.
 */

#define	PRE_SYSCALL(name) \
	ENTRY_NP(name); \
	PIC_SETUP(.pic/**/name)			/* save ebx 		*/; \
	movl    %gs:0, %eax			/* load curthread	*/; \
	movsbl	T_CANTYPE(%eax), %ecx		/* type = t->t_can_type	*/; \
	pushl	%ecx				/* save type		*/; \
	cmpb	$TC_DISABLE, T_CANSTATE(%eax)	/* if (state is DISABLE) */; \
	je	.call/**/name			/* 	call newname	*/; \
	lock					/* type = ASYNC (-1) has */; \
	orb	$TC_ASYNCHRONOUS, T_CANTYPE(%eax) /* to be atomic	*/; \
	cmpb	$TC_PENDING, T_CANPENDING(%eax)	/* if (pending not set)	*/; \
	jne	.call/**/name			/* 	call newname 	*/; \
	/* The stack frame has to be saved before calling the */;\
	/* exit routine. Before doing so, we have to cleanup the stack */;\
	/* which contains ecx pushed earlier and  ebx pushed  in PIC_SETUP */;\
	/* See bug 4229175. */;\
	popl    %ecx				/* pop ecx pushed earlier */;\
	addl    $0x4, %esp		/* advance stack pushed by ebx */;\
	pushl   %ebp				/* save the stack frame */;\
	movl    %esp, %ebp;						\
	subl    $0x4, %esp			/* room for return status */;\
	movl	$PTHREAD_CANCELED, (%esp)	/* sts=PTHREAD_CANCELED	*/; \
	call	fcnref(_pthread_exit)		/* exit thread with cancel */; \
.call/**/name: ;

#define	POST_SYSCALL(name) \
	movl	%gs:0, %ebx			/* load curthread	*/; \
	popl	%ecx				/* restore type		*/; \
	movb	%cl, T_CANTYPE(%ebx)		/* t->t_can_type = type	*/; \
	PIC_EPILOG				/* restore ebx		*/; \
	ret					/* return		*/; \
	SET_SIZE(name)

#define	SYSCALL_CANCELPOINT_0(name, newname) \
	PRE_SYSCALL(name); \
	call	fcnref(newname)			/* call newname w/o args*/; \
	POST_SYSCALL(name)

#define	SYSCALL_CANCELPOINT_1(name, newname) \
	PRE_SYSCALL(name); \
	pushl	12(%esp)			/* copy one argument	*/; \
	call	fcnref(newname)			/* call newname with 1 arg */; \
	addl	$4, %esp			/* restore sp		*/; \
	POST_SYSCALL(name)

#define	SYSCALL_CANCELPOINT_2(name, newname) \
	PRE_SYSCALL(name); \
	pushl   16(%esp)			/* copy second argument	*/; \
	pushl   16(%esp)			/* copy first argument	*/; \
	call	fcnref(newname)			/* call newname with 2 args */; \
	addl	$8, %esp			/* restore sp		*/; \
	POST_SYSCALL(name)

#define	SYSCALL_CANCELPOINT_3(name, newname) \
	PRE_SYSCALL(name); \
	pushl   20(%esp)			/* copy third argument	*/; \
	pushl   20(%esp)			/* copy second argument	*/; \
	pushl   20(%esp)			/* copy first argument	*/; \
	call	fcnref(newname)			/* call newname with 3 args */; \
	addl	$12, %esp			/* restore sp		*/; \
	POST_SYSCALL(name)

#define	SYSCALL_CANCELPOINT_4(name, newname) \
	PRE_SYSCALL(name); \
	pushl   24(%esp)			/* copy fourth argument	*/; \
	pushl   24(%esp)			/* copy third argument	*/; \
	pushl   24(%esp)			/* copy second argument	*/; \
	pushl   24(%esp)			/* copy first argument	*/; \
	call	fcnref(newname)			/* call newname with 4 args */; \
	addl	$16, %esp			/* restore sp		*/; \
	POST_SYSCALL(name)

#define	SYSCALL_CANCELPOINT_5(name, newname) \
	PRE_SYSCALL(name); \
	pushl   28(%esp)			/* copy fifth argument	*/; \
	pushl   28(%esp)			/* copy fourth argument	*/; \
	pushl   28(%esp)			/* copy third argument	*/; \
	pushl   28(%esp)			/* copy second argument	*/; \
	pushl   28(%esp)			/* copy first argument	*/; \
	call	fcnref(newname)			/* call newname with 5 args */; \
	addl	$20, %esp			/* restore sp		*/; \
	POST_SYSCALL(name)

#endif	/* _LIBTHREAD_SYS_CANCEL_H */
