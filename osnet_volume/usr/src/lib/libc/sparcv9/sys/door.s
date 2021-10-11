/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)door.s	1.11	97/08/21 SMI"


	.file "door.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(_door_create,function)
	ANSI_PRAGMA_WEAK(_door_call,function)
	ANSI_PRAGMA_WEAK(_door_return,function)
	ANSI_PRAGMA_WEAK(_door_revoke,function)
	ANSI_PRAGMA_WEAK(_door_info,function)
	ANSI_PRAGMA_WEAK(_door_cred,function)
	ANSI_PRAGMA_WEAK(_door_bind,function)
	ANSI_PRAGMA_WEAK(_door_unbind,function)

#include <sys/door.h>
#include "SYS.h"

/*
 * Offsets within struct door_results
 */
#define	DOOR_COOKIE	(SA(MINFRAME) + STACK_BIAS + 0)
#define	DOOR_DATA_PTR	(SA(MINFRAME) + STACK_BIAS + 8)
#define	DOOR_DATA_SIZE	(SA(MINFRAME) + STACK_BIAS + 16)
#define	DOOR_DESC_PTR	(SA(MINFRAME) + STACK_BIAS + 24)
#define	DOOR_DESC_SIZE	(SA(MINFRAME) + STACK_BIAS + 32)
#define	DOOR_PC		(SA(MINFRAME) + STACK_BIAS + 40)
#define	DOOR_SERVERS	(SA(MINFRAME) + STACK_BIAS + 48)
#define	DOOR_INFO_PTR	(SA(MINFRAME) + STACK_BIAS + 56)

/*
 * Pointer to server create function
 */
	.section	".bss"
	.common	__door_server_func, 8, 8
	.common	__thr_door_server_func, 8, 8
	.common __door_create_pid, 4, 4

/*
 * int
 * _door_create(void (*)(), void *, u_int)
 */
	ENTRY(_door_create)
	mov	DOOR_CREATE, %o5	! subcode
	SYSTRAP(door)
	SYSCERROR
	retl
	nop
	SET_SIZE(_door_create)

/*
 * int
 * _door_revoke(int)
 */
	ENTRY(_door_revoke)
	mov	DOOR_REVOKE, %o5	! subcode
	SYSTRAP(door)
	SYSCERROR
	retl
	nop
	SET_SIZE(_door_revoke)
/*
 * int
 * _door_info(int, door_info_t *)
 */
	ENTRY(_door_info)
	mov	DOOR_INFO, %o5	! subcode
	SYSTRAP(door)
	SYSCERROR
	retl
	nop
	SET_SIZE(_door_info)

/*
 * int
 * _door_cred(door_cred_t *)
 */
	ENTRY(_door_cred)
	mov	DOOR_CRED, %o5	! subcode
	SYSTRAP(door)
	SYSCERROR
	retl
	nop
	SET_SIZE(_door_cred)

/*
 * int
 * _door_bind(int d)
 */
	ENTRY(_door_bind)
	mov	DOOR_BIND, %o5	! subcode
	SYSTRAP(door)
	SYSCERROR
	retl
	nop
	SET_SIZE(_door_bind)

/*
 * int
 * _door_unbind()
 */
	ENTRY(_door_unbind)
	mov	DOOR_UNBIND, %o5 ! subcode
	SYSTRAP(door)
	SYSCERROR
	retl
	nop
	SET_SIZE(_door_unbind)

/*
 * int
 * _door_call(int d, struct door_args *dp)
 */
	ENTRY(_door_call);
	mov	DOOR_CALL, %o5 ! subcode
	SYSTRAP(door)
	SYSCERROR
	retl
	nop
	SET_SIZE(_door_call)

/*
 * _door_return(char *, int, door_desc_t *, int, caddr_t)
 */
	ENTRY(_door_return)
	/*
	 * Curthread sits on top of the thread stack area.
	 */
	sub	%g7, SA(MINFRAME), %o4
door_restart:
	mov	DOOR_RETURN, %o5	! subcode
	SYSTRAP(door)
	/*
	 * All new invocations come here (unless there is an error
	 * in the door_return).
	 *
	 * on return, we're serving a door_call. Our stack looks like this:
	 *
	 *		descriptors (if any)
	 *		data (if any)
	 *		struct door_results
	 *		MINFRAME
	 *	sp ->
	 */
	bcs	2f				! errno is set
	ld	[%sp + DOOR_SERVERS], %g1
	tst	%g1				! (delay) test nservers
	bg	1f				! everything looks o.k.
	ldn	[%sp + DOOR_COOKIE], %o0	! (delay) load cookie
	/*
	 * this is the last server thread - call creation func for more
	 */
	save	%sp, -SA(MINFRAME), %sp
#if defined(PIC)
	PIC_SETUP(g1)
	sethi	%hi(__thr_door_server_func), %g5
	or	%g5, %lo(__thr_door_server_func), %g5
	ldn	[%g5 + %g1], %g5
	ldn	[%g5], %g1
#else
	setnhi	__thr_door_server_func, %g5, %g1
	ldn	[%g1 + %lo(__thr_door_server_func)], %g1
#endif	/* defined(PIC) */
	jmpl	%g1, %o7
	ldn	[%fp + DOOR_INFO_PTR], %o0	! (delay) load door_info ptr
	restore
1:
	ldn	[%sp + DOOR_DATA_PTR], %o1
	ldn	[%sp + DOOR_DATA_SIZE], %o2
	ldn	[%sp + DOOR_DESC_PTR], %o3
	ldn	[%sp + DOOR_DESC_SIZE], %o4
	ldn	[%sp + DOOR_PC], %g1
	jmpl	%g1, %o7	/* Do the call */
	mov	%g0, %i6	/* The stack trace stops here */
	call	thr_exit	/* Exit the thread if we return here */
	nop
	/* NOTREACHED */
2:
	/*
	 * Error during door_return call.  Restart the system call if
	 * the error code is EINTR and this lwp is still part of the
	 * same process.
	 */
	cmp	%o0, EINTR		! interrupted while waiting?
	bne	3f
	nop

	save	%sp, -SA(MINFRAME), %sp
	call	_getpid			! get current pid
	nop
#if defined(PIC)
	PIC_SETUP(g1)
	sethi	%hi(__door_create_pid), %g5
	or	%g5, %lo(__door_create_pid), %g5
	ldn	[%g1 + %g5], %g5
	ld	[%g5], %g1
#else
	setnhi	__door_create_pid, %g5, %g1
	ld	[%g1 + %lo(__door_create_pid)], %g1
#endif	/* defined(PIC) */
	cmp	%o0, %g1		! are we still the same process?
	beq	door_restart		! if yes, restart
	restore

3:	mov	%o7, %g1
	call	_cerror			! returns to caller of door_return
	mov	%g1, %o7
	/* NOTREACHED */
	SET_SIZE(_door_return)
