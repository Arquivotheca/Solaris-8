/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)msgsys.c	1.4	98/02/27 SMI"
#ifdef __STDC__
#pragma weak msgctl = _msgctl
#pragma weak msgget = _msgget
#pragma weak msgrcv = _msgrcv
#pragma weak msgsnd = _msgsnd
#endif

#pragma	weak _libc_msgrcv = _msgrcv
#pragma	weak _libc_msgsnd = _msgsnd

#include	"synonyms.h"
#include	"sys/types.h"
#include	"sys/ipc.h"
#include	"sys/msg.h"

#define	MSGSYS	49

#define	MSGGET	0
#define	MSGCTL	1
#define	MSGRCV	2
#define	MSGSND	3

extern long syscall();

int
msgget(key, msgflg)
key_t key;
int msgflg;
{
	return (syscall(MSGSYS, MSGGET, key, msgflg));
}

int
msgctl(int msqid, int cmd, struct msqid_ds *buf)
{
	return (syscall(MSGSYS, MSGCTL, msqid, cmd, buf));
}

ssize_t
msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg)
{
	return (syscall(MSGSYS, MSGRCV, msqid, msgp, msgsz, msgtyp, msgflg));
}

int
msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg)
{
	return (syscall(MSGSYS, MSGSND, msqid, msgp, msgsz, msgflg));
}
