/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 *	Copyright (c) 1996, Sun Microsystems Inc
 *	All Rights Reserved.
*/


#pragma	ident "@(#)msgsys.c	1.15	98/02/27 SMI" /* SVr4.0 1.6.1.7	*/
/*LINTLIBRARY*/
#include	"synonyms.h"
#include	<sys/types.h>
#include	<sys/ipc.h>
#include	<sys/msg.h>
#include	<sys/syscall.h>

#ifdef	_LP64
#error	"not suitable for 64-bit compilation"
#endif

#pragma weak msgctl = _msgctl
#pragma weak msgget = _msgget
#pragma weak msgrcv = _msgrcv
#pragma weak msgsnd = _msgsnd

#pragma weak _libc_msgrcv = _msgrcv
#pragma weak _libc_msgsnd = _msgsnd

#define	MSGGET	0
#define	MSGCTL	1
#define	MSGRCV	2
#define	MSGSND	3

int
msgget(key_t key, int msgflg)
{
	return (syscall(SYS_msgsys, MSGGET, key, msgflg));
}

int
msgctl(int msqid, int cmd, struct msqid_ds *buf)
{
	return (syscall(SYS_msgsys, MSGCTL, msqid, cmd, buf));
}

ssize_t
msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg)
{
	return ((ssize_t)syscall(SYS_msgsys, MSGRCV, msqid, msgp,
	    msgsz, msgtyp, msgflg));
}

int
msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg)
{
	return (syscall(SYS_msgsys, MSGSND, msqid, msgp, msgsz, msgflg));
}
