/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)msgsys.s	1.5	98/02/27 SMI"

/*
 * int msgget(key_t key, int msgflg);
 * int msgctl(int msqid, int cmd, struct msqid_ds *buf );
 * ssize_t msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg);
 * int msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg);
 */
	.file	"msgsys.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(msgget,function)
	ANSI_PRAGMA_WEAK(msgctl,function)

#include "SYS.h"
	.weak   _libc_msgrcv;
	.type   _libc_msgrcv, #function
	_libc_msgrcv = _msgrcv 

	.weak   _libc_msgsnd;
	.type   _libc_msgsnd, #function
	_libc_msgsnd = _msgsnd 

#define	SUBSYS_msgget		0
#define	SUBSYS_msgctl		1
#define	SUBSYS_msgrcv		2
#define	SUBSYS_msgsnd		3

	ENTRY(msgget)
	mov	%o1, %o2
	mov	%o0, %o1
	b	.sys
	mov	SUBSYS_msgget, %o0

	ENTRY(msgctl)
	mov	%o2, %o3
	mov	%o1, %o2
	mov	%o0, %o1
	b	.sys
	mov	SUBSYS_msgctl, %o0

	ENTRY(msgrcv)
	mov	%o4, %o5
	mov	%o3, %o4
	mov	%o2, %o3
	mov	%o1, %o2
	mov	%o0, %o1
	b	.sys
	mov	SUBSYS_msgrcv, %o0

	
	ENTRY(msgsnd)
	mov	%o3, %o4
	mov	%o2, %o3
	mov	%o1, %o2
	mov	%o0, %o1
	mov	SUBSYS_msgsnd, %o0

.sys:
	SYSTRAP(msgsys)
	SYSCERROR
	RET

	SET_SIZE(msgget)
	SET_SIZE(msgctl)
	SET_SIZE(msgrcv)
	SET_SIZE(msgsnd)
