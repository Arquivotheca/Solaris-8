/*
 * Copyright (c) 1996-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_LIBC_SPARCV9_INC_SYS_H
#define	_LIBC_SPARCV9_INC_SYS_H

#ident	"@(#)SYS.h	1.8	97/04/25 SMI"


/*
 * This file defines common code sequences for system calls.
 */

#include <sys/asm_linkage.h>
#include <sys/syscall.h>
#include <sys/errno.h>
#include "synonyms.h"

#ifdef PIC
#include "PIC.h"
#endif

/*
 * Trap number for system calls
 */
#define	SYSCALL_TRAPNUM	64

/*
 * Define the external symbol _cerror for all files.
 */
	.global	_cerror

/*
 * SYSTRAP provides the actual trap sequence. It assumes that an entry
 * of the form SYS_name exists (probably from syscall.h).
 */
#define	SYSTRAP(name)						\
	mov	SYS_/**/name, %g1;				\
	t	SYSCALL_TRAPNUM

/*
 * SYSCERROR provides the sequence to branch to _cerror if an error is
 * indicated by the carry-bit being set upon return from a trap.
 */
#ifdef PIC
#define	SYSCERROR		\
	bcc,pt	%icc, 1f;	\
	mov	%o7, %g1;	\
	call	_cerror;	\
	mov	%g1, %o7;	\
1:
#else
#define	SYSCERROR						\
	bcs,pn	%icc, _cerror;					\
	nop;
#endif

/*
 * SYSLWPERR provides the sequence to return 0 on a successful trap
 * and the error code if unsuccessful.
 * Error is indicated by the carry-bit being set upon return from a trap.
 */
#define	SYSLWPERR						\
	bcc,a,pt %icc, 1f;					\
	clr	%o0;						\
	cmp	%o0, ERESTART;					\
	move	%icc, EINTR, %o0;				\
1:

#define	SAVE_OFFSET	(STACK_BIAS + 8 * 16)

/*
 * SYSREENTRY provides the entry sequence for restartable system calls.
 */
#define	SYSREENTRY(name)					\
	ENTRY(name);						\
	stx	%o0, [%sp + SAVE_OFFSET];			\
.restart_/**/name:

/*
 * SYSRESTART provides the error handling sequence for restartable
 * system calls.
 */
#ifdef PIC
#define	SYSRESTART(name)					\
	bcc,pt	%icc, 1f;					\
	cmp	%o0, ERESTART;					\
	be,a,pn	%icc, name;					\
	ldx	[%sp + SAVE_OFFSET], %o0;			\
	mov	%o7, %g1;					\
	call	_cerror;					\
	mov	%g1, %o7;					\
1:
#else
#define	SYSRESTART(name)					\
	bcc,pt	%icc, 1f;					\
	cmp	%o0, ERESTART;					\
	be,a,pn	%icc, name;					\
	ldx	[%sp + SAVE_OFFSET], %o0;			\
	ba	%icc, _cerror;					\
	nop;							\
1:
#endif

/*
 * SYSINTR_RESTART provides the error handling sequence for restartable
 * system calls in case of EINTR or ERESTART.
 */
#define	SYSINTR_RESTART(name)					\
	bcc,a,pt %icc, 1f;					\
	clr	%o0;						\
	cmp	%o0, ERESTART;					\
	be,a,pn	%icc, name;					\
	ldx	[%sp + SAVE_OFFSET], %o0;			\
	cmp	%o0, EINTR;					\
	be,a,pn	%icc, name;					\
	ldx	[%sp + SAVE_OFFSET], %o0;			\
1:

/*
 * SYSCALL provides the standard (i.e.: most common) system call sequence.
 */
#define	SYSCALL(name)						\
	ENTRY(name);						\
	SYSTRAP(name);						\
	SYSCERROR

/*
 * SYSCALL_RESTART provides the most common restartable system call sequence.
 */
#define	SYSCALL_RESTART(name)					\
	SYSREENTRY(name);					\
	SYSTRAP(name);						\
	SYSRESTART(.restart_/**/name)

/*
 * SYSCALL2 provides a common system call sequence when the entry name
 * is different than the trap name.
 */
#define	SYSCALL2(entryname, trapname)				\
	ENTRY(entryname);					\
	SYSTRAP(trapname);					\
	SYSCERROR

/*
 * SYSCALL2_RESTART provides a common restartable system call sequence when the
 * entry name is different than the trap name.
 */
#define	SYSCALL2_RESTART(entryname, trapname)			\
	SYSREENTRY(entryname);					\
	SYSTRAP(trapname);					\
	SYSRESTART(.restart_/**/entryname)

/*
 * SYSCALL_NOERROR provides the most common system call sequence for those
 * system calls which don't check the error reture (carry bit).
 */
#define	SYSCALL_NOERROR(name)					\
	ENTRY(name);						\
	SYSTRAP(name)

/*
 * Standard syscall return sequence.
 */
#define	RET							\
	retl;							\
	nop

/*
 * Syscall return sequence with return code forced to zero.
 */
#define	RETC							\
	retl;							\
	clr	%o0

#endif	/* _LIBC_SPARCV9_INC_SYS_H */
