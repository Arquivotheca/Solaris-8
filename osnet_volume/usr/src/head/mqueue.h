/*	Copyright (c) 1993, by Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _MQUEUE_H
#define	_MQUEUE_H

#pragma ident	"@(#)mqueue.h	1.8	98/04/02 SMI"

#include <sys/feature_tests.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/signal.h>
#include <sys/siginfo.h>
#include <time.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef void	*mqd_t;		/* opaque message queue descriptor */

struct mq_attr {
	long	mq_flags;	/* message queue flags */
	long	mq_maxmsg;	/* maximum number of messages */
	long	mq_msgsize;	/* maximum message size */
	long	mq_curmsgs;	/* number of messages currently queued */
	int	mq_pad[12];
};

/*
 * function prototypes
 */
#if	defined(__STDC__)
#if	(_POSIX_C_SOURCE - 0 > 0) && (_POSIX_C_SOURCE - 0 <= 2)
#error	"POSIX Message Passing is not supported in POSIX.1-1990"
#endif
#include <sys/siginfo.h>
mqd_t	mq_open(const char *, int, ...);
int	mq_close(mqd_t);
int	mq_unlink(const char *);
int	mq_send(mqd_t, const char *, size_t, unsigned int);
ssize_t	mq_receive(mqd_t, char *, size_t, unsigned int *);
int	mq_notify(mqd_t, const struct sigevent *);
int	mq_getattr(mqd_t, struct mq_attr *);
int	mq_setattr(mqd_t, const struct mq_attr *, struct mq_attr *);
#else
mqd_t	mq_open();
int	mq_close();
int	mq_unlink();
int	mq_send();
ssize_t	mq_receive();
int	mq_notify();
int	mq_getattr();
int	mq_setattr();
#endif	/* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _MQUEUE_H */
