/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_MSG_H
#define	_SYS_MSG_H

#pragma ident	"@(#)msg.h	1.31	99/04/14 SMI"

#include <sys/ipc.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * IPC Message Facility.
 */

/*
 * Implementation Constants.
 */

/*
 * Permission Definitions.
 */

#define	MSG_R	0400	/* read permission */
#define	MSG_W	0200	/* write permission */

/*
 * ipc_perm Mode Definitions.
 */

#define	MSG_RWAIT	01000	/* a reader is waiting for a message */
#define	MSG_WWAIT	02000	/* a writer is waiting to send */

/*
 * Message Operation Flags.
 */

#define	MSG_NOERROR	010000	/* no error if big message */

typedef unsigned long msgqnum_t;
typedef unsigned long msglen_t;

/*
 * There is one msg structure for each message that may be in the system.
 */
struct msg {
	struct msg	*msg_next;	/* ptr to next message on q */
	long		msg_type;	/* message type */
	size_t		msg_size;	/* message text size */
	void		*msg_addr;	/* message text address */
};

/*
 * There is one msg queue id data structure for each q in the system.
 */

/*
 * Applications that read /dev/mem must be built like the kernel. A new
 * symbol "_KMEMUSER" is defined for this purpose.
 */

#if defined(_KERNEL) || defined(_KMEMUSER)

#include <sys/t_lock.h>

/* expanded msqid_ds structure */

struct msqid_ds {
	struct ipc_perm msg_perm;	/* operation permission struct */
	struct msg	*msg_first;	/* ptr to first message on q */
	struct msg	*msg_last;	/* ptr to last message on q */
	msglen_t	msg_cbytes;	/* current # bytes on q */
	msgqnum_t	msg_qnum;	/* # of messages on q */
	msglen_t	msg_qbytes;	/* max # of bytes on q */
	pid_t		msg_lspid;	/* pid of last msgsnd */
	pid_t		msg_lrpid;	/* pid of last msgrcv */
#if defined(_LP64)
	time_t		msg_stime;	/* last msgsnd time */
	time_t		msg_rtime;	/* last msgrcv time */
	time_t		msg_ctime;	/* last change time */
#else
	time_t		msg_stime;	/* last msgsnd time */
	int32_t		msg_pad1;	/* reserved for time_t expansion */
	time_t		msg_rtime;	/* last msgrcv time */
	int32_t		msg_pad2;	/* time_t expansion */
	time_t		msg_ctime;	/* last change time */
	int32_t		msg_pad3;	/* time expansion */
#endif
	kcondvar_t	msg_cv;
	kcondvar_t	msg_qnum_cv;
	long		msg_pad4[3];	/* reserve area */
};

#else	/* user definition */

struct msqid_ds {
	struct ipc_perm	msg_perm;	/* operation permission struct */
	struct msg	*msg_first;	/* ptr to first message on q */
	struct msg	*msg_last;	/* ptr to last message on q */
	msglen_t	msg_cbytes;	/* current # bytes on q */
	msgqnum_t	msg_qnum;	/* # of messages on q */
	msglen_t	msg_qbytes;	/* max # of bytes on q */
	pid_t		msg_lspid;	/* pid of last msgsnd */
	pid_t		msg_lrpid;	/* pid of last msgrcv */
#if defined(_LP64)
	time_t		msg_stime;	/* last msgsnd time */
	time_t		msg_rtime;	/* last msgrcv time */
	time_t		msg_ctime;	/* last change time */
#else
	time_t		msg_stime;	/* last msgsnd time */
	int32_t		msg_pad1;	/* reserved for time_t expansion */
	time_t		msg_rtime;	/* last msgrcv time */
	int32_t		msg_pad2;	/* time_t expansion */
	time_t		msg_ctime;	/* last change time */
	int32_t		msg_pad3;	/* time_t expansion */
#endif
	short		msg_cv;
	short		msg_qnum_cv;
	long		msg_pad4[3];	/* reserve area */
};

#endif  /* _KERNEL */

#if defined(_KERNEL)

/*
 * Size invariant version of SVR3 structure. Only kept around
 * to support old binaries. Perhaps this can go away someday.
 */
struct o_msqid_ds32 {
	struct o_ipc_perm32 msg_perm;	/* operation permission struct */
	caddr32_t 	msg_first;	/* ptr to first message on q */
	caddr32_t 	msg_last;	/* ptr to last message on q */
	uint16_t	msg_cbytes;	/* current # bytes on q */
	uint16_t	msg_qnum;	/* # of messages on q */
	uint16_t	msg_qbytes;	/* max # of bytes on q */
	o_pid_t		msg_lspid;	/* pid of last msgsnd */
	o_pid_t		msg_lrpid;	/* pid of last msgrcv */
	time32_t	msg_stime;	/* last msgsnd time */
	time32_t	msg_rtime;	/* last msgrcv time */
	time32_t	msg_ctime;	/* last change time */
};

#endif	/* _KERNEL */

#if defined(_SYSCALL32)

/* Kernel's view of the user ILP32 msqid_ds structure */

struct msqid_ds32 {
	struct ipc_perm32 msg_perm;	/* operation permission struct */
	caddr32_t	msg_first;	/* ptr to first message on q */
	caddr32_t	msg_last;	/* ptr to last message on q */
	uint32_t	msg_cbytes;	/* current # bytes on q */
	uint32_t	msg_qnum;	/* # of messages on q */
	uint32_t	msg_qbytes;	/* max # of bytes on q */
	pid32_t		msg_lspid;	/* pid of last msgsnd */
	pid32_t		msg_lrpid;	/* pid of last msgrcv */
	time32_t	msg_stime;	/* last msgsnd time */
	int32_t		msg_pad1;	/* reserved for time_t expansion */
	time32_t	msg_rtime;	/* last msgrcv time */
	int32_t		msg_pad2;	/* time_t expansion */
	time32_t	msg_ctime;	/* last change time */
	int32_t		msg_pad3;	/* time expansion */
	int16_t		msg_cv;
	int16_t		msg_qnum_cv;
	int32_t		msg_pad4[3];	/* reserve area */
};


/* Kernel's view of the user ILP32 msgbuf structure */

struct ipcmsgbuf32 {
	int32_t	mtype;		/* message type */
	char	mtext[1];	/* message text */
};

#endif	/* _SYSCALL32 */

/*
 * User message buffer template for msgsnd and msgrecv system calls.
 */

#ifdef _KERNEL
struct ipcmsgbuf {
#else
struct msgbuf {
#endif /* _KERNEL */
#if defined(_XOPEN_SOURCE)
	long	_mtype;		/* message type */
	char	_mtext[1];	/* message text */
#else
	long	mtype;		/* message type */
	char	mtext[1];	/* message text */
#endif
};

/*
 * Message information structure.
 */

struct msginfo {
	size_t		msgmax;	/* max message size */
	size_t		msgmnb;	/* max # bytes on queue */
	int		msgmni;	/* # of message queue identifiers */
	int		msgtql;	/* # of system message headers */
};

/*
 * We have to be able to lock a message queue since we can
 * sleep during message processing due to a page fault in
 * copyin/copyout or iomove.  We cannot add anything to the
 * msqid_ds structure since this is used in user programs
 * and any change would break object file compatibility.
 * Therefore, we allocate a parallel array, msglock, which
 * is used to lock a message queue.  The array is defined
 * in the msg master file.  The following macro takes a
 * pointer to a message queue and returns a pointer to the
 * lock entry.  The argument must be a pointer to a msgqid
 * structure.
 */

#define	MSGLOCK(X)	&msglock[X - msgque]

#if !defined(_KERNEL)
#if defined(__STDC__)
int msgctl(int, int, struct msqid_ds *);
int msgget(key_t, int);
ssize_t msgrcv(int, void *, size_t, long, int);
int msgsnd(int, const void *, size_t, int);
#else /* __STDC __ */
int msgctl();
int msgget();
int msgrcv();
int msgsnd();
#endif /* __STDC __ */
#endif /* ! _KERNEL */

#ifdef _KERNEL

struct msglock {
	kmutex_t msglock_lock;
};

/*
 * Defined in space.c, allocated/initialized in msg.c
 */
extern caddr_t		msg;		/* base address of message buffer */
extern struct msg	*msgh;		/* message headers */
extern struct msqid_ds	*msgque;	/* msg queue headers */
extern struct msglock	*msglock; 	/* locks for the message queues */
extern struct msg	*msgfp;		/* ptr to head of free header list */
extern struct msginfo	msginfo;	/* message parameters */

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MSG_H */
