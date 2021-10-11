/*	Copyright (c) 1993 SMI	*/
/*	 All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident   "@(#)SYS.h 1.9     96/08/09     SMI"


#ifndef	_LIBC_I86_INC_SYS_H
#define	_LIBC_I86_INC_SYS_H


/*
 * This file defines common code sequences for system calls.
 */
#include <sys/asm_linkage.h>
#include <sys/syscall.h>
#include <sys/errno.h>

#define	SYSTRAP(name) \
	movl	$SYS_/**/name,%eax; \
	lcall   $SYSCALL_TRAPNUM,$0

#ifdef PIC
/*
 * Trap number for system calls
 */
#define SYSCALL_TRAPNUM 0x27

#define	UNSAFE_PIC_SETUP(name) \
	call	name/**/pic_setup; \
name/**/pic_setup: \
	popl	%ebx; \
	addl	$_GLOBAL_OFFSET_TABLE_+[.-name/**/pic_setup],%ebx;

#define	PIC_SETUP(name) \
	pushl	%ebx; \
	call	name/**/pic_setup; \
name/**/pic_setup: \
	popl	%ebx; \
	addl	$_GLOBAL_OFFSET_TABLE_+[.-name/**/pic_setup],%ebx

#define	fcnref(name) \
	name@PLT

#define	dataaddr(name) \
	name@GOT(%ebx)

#define	PIC_EPILOG \
	popl	%ebx

#define	JMPCERROR(name) \
	PIC_SETUP(name); \
	pushl	%eax; \
	jmp	fcnref(__Cerror);
#else
/*
 * Trap number for system calls
 */
#define	SYSCALL_TRAPNUM	0x7

#define	UNSAFE_PIC_SETUP(name)
#define	PIC_SETUP(name)
#define	fcnref(name) \
	name
#define	dataaddr(name) \
	$name
#define	PIC_EPILOG
#define	JMPCERROR(name) \
	jmp	fcnref(__Cerror)
#endif

#define	RET \
	ret

/*
 * Macro to declare a weak symbol alias.  This is similar to
 *	#pragma weak	wsym = sym
 */
#define	PRAGMA_WEAK(wsym, sym) \
	.weak	wsym; \
	wsym = sym

#define	fwdef(name)	\
	.globl	_/**/name;\
	.weak	name; \
	.set	name,_/**/name; \
	.type	name,@function;\
	.type	_/**/name,@function;\
_/**/name:

#define	SYSLWPERR(name)	\
	jae	name/**/_noerror; \
	cmpb	$ ERESTART, %al; \
	je	name; \
	cmpb	$ EINTR, %al; \
	je	name; \
	JMPCERROR(name); \
name/**/_noerror:

/*
 * SYSRESTART provides the error handling sequence for restartable
 * system calls.
 */
#define	SYSRESTART(name) \
	jae	name/**/_noerror; \
	cmpb	$ ERESTART, %al; \
	je	name; \
	JMPCERROR(name); \
name/**/_noerror:

/*
 * SYSINTR_RESTART provides the error handling sequence for restartable
 * system calls in case of EINTR or ERESTART.
 */
#define	SYSINTR_RESTART(name) \
	SYSLWPERR(name)

#define	SYSCERROR(name)	\
	jae	name/**/_noerror; \
	JMPCERROR(name); \
name/**/_noerror:

#endif	/* _LIBC_I86_INC_SYS_H */
