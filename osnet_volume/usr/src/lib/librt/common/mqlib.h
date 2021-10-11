/*
 * Copyright (c) 1993-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MQLIB_H
#define	_MQLIB_H

#pragma ident	"@(#)mqlib.h	1.8	98/12/22 SMI"

/*
 * mqlib.h - Header file for POSIX.4 message queue
 */

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/feature_tests.h>

#include <sys/types.h>
#include <synch.h>
#include <sys/types.h>
#include <signal.h>
#include <mqueue.h>
#include <semaphore.h>

/*
 * Default values per message queue
 */
#define	MQ_MAXMSG	128
#define	MQ_MAXSIZE	1024
#define	MQ_MAXPRIO	((size_t)sysconf(_SC_MQ_PRIO_MAX))
#define	MQ_MAXOPEN	((int)sysconf(_SC_MQ_OPEN_MAX))

#define	MQ_MAGIC	0x4d534751		/* "MSGQ" */
#define	MQ_SIZEMASK	(~0x3)

/*
 * Message header which is part of messages in link list
 */
typedef struct mq_msg_hdr {
	struct mq_msg_hdr 	*msg_next;	/* next message in the link */
	ssize_t			msg_len;	/* length of the message */
} msghdr_t;

/*
 * message queue descriptor structure
 */
typedef struct mq_des {
	size_t		mqd_magic;	/* magic # to identify mq_des */
	struct mq_header *mqd_mq;	/* address pointer of message Q */
	size_t		mqd_flags;	/* operation flag per open */
	struct mq_dn	*mqd_mqdn;	/* open	description */
} mqdes_t;

/*
 * message queue description
 */
struct mq_dn {
	size_t		mqdn_flags;	/* open description flags */
};


/*
 * message queue common header which is part of mmap()ed file.
 */
typedef struct mq_header {
	/* first field should be mq_totsize, DO NOT insert before this	*/
	ssize_t		mq_totsize;	/* total size of the Queue */
	size_t		mq_magic;	/* support more implementations */
	size_t		mq_flag;	/* various mqueue flags */
	ssize_t		mq_maxsz;	/* max size of each message */
	ssize_t		mq_maxmsg;	/* max messages in the queue */
	ssize_t		mq_count;	/* current count of messages */
	ssize_t		mq_waiters;	/* current count of receivers */
	size_t		mq_maxprio;	/* maximum mqueue priority */
	size_t		mq_curmaxprio;	/* current maximum MQ priority */
	int32_t		mq_mask;	/* priority bitmask */
	msghdr_t	*mq_freep;	/* free message's head pointer */
	msghdr_t	**mq_headpp;	/* pointer to head pointers */
	msghdr_t	**mq_tailpp;	/* pointer to tail pointers */
	signotify_id_t	mq_sigid;	/* notification id */
	mqdes_t		*mq_des;	/* pointer to msg Q descriptor */
	sem_t		mq_exclusive;	/* acquire for exclusive access */
	sem_t		mq_rblocked;	/* number of processes rblocked */
	sem_t		mq_notfull;	/* mq_send()'s block on this */
	sem_t		mq_notempty;	/* mq_receive()'s block on this */
	ssize_t		mq_pad[4];	/* reserved for future */
} mqhdr_t;

/* prototype for signotify system call. unexposed to user */
int __signotify(int cmd, siginfo_t *sigonfo, signotify_id_t *sn_id);

#ifdef	__cplusplus
}
#endif

#endif	/* _MQLIB_H */
