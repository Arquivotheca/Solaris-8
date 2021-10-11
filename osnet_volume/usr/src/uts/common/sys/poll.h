/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1995, 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_POLL_H
#define	_SYS_POLL_H

#pragma ident	"@(#)poll.h	1.28	98/11/23 SMI"	/* SVr4.0 11.9 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Structure of file descriptor/event pairs supplied in
 * the poll arrays.
 */
typedef struct pollfd {
	int fd;				/* file desc to poll */
	short events;			/* events of interest on fd */
	short revents;			/* events that occurred on fd */
} pollfd_t;

typedef unsigned long	nfds_t;

/*
 * Testable select events
 */
#define	POLLIN		0x0001		/* fd is readable */
#define	POLLPRI		0x0002		/* high priority info at fd */
#define	POLLOUT		0x0004		/* fd is writeable (won't block) */
#define	POLLRDNORM	0x0040		/* normal data is readable */
#define	POLLWRNORM	POLLOUT
#define	POLLRDBAND	0x0080		/* out-of-band data is readable */
#define	POLLWRBAND	0x0100		/* out-of-band data is writeable */

#define	POLLNORM	POLLRDNORM

/*
 * Non-testable poll events (may not be specified in events field,
 * but may be returned in revents field).
 */
#define	POLLERR		0x0008		/* fd has error condition */
#define	POLLHUP		0x0010		/* fd has been hung up on */
#define	POLLNVAL	0x0020		/* invalid pollfd entry */

#define	POLLREMOVE	0x0800	/* remove a cached poll fd from /dev/poll */

#ifdef _KERNEL

/*
 * Additional private poll flags supported only by strpoll().
 * Must be bit-wise distinct from the above POLL flags.
 */
#define	POLLRDDATA	0x0200	/* Wait for M_DATA; ignore M_PROTO only msgs */
#define	POLLNOERR	0x0400	/* Ignore POLLERR conditions */

#define	POLLCLOSED	0x8000	/* a (cached) poll fd has been closed */

#endif /* _KERNEL */

#if defined(_KERNEL) || defined(_KMEMUSER)

#include <sys/thread.h>

/*
 * XXX We are forced to use a forward reference here because including
 * file.h here will break i386 build. The real solution is to fix the
 * broken parts in usr/src/stand/lib/fs.
 */
struct fpollinfo;

/*
 * Poll list head structure.  A pointer to this is passed to
 * pollwakeup() from the caller indicating an event has occurred.
 * Only the ph_list field is used, but for DDI compliance, we can't
 * change the size of the structure.
 */
typedef struct pollhead {
	struct polldat		*ph_list;	/* list of pollers */
	void			*ph_pad1;	/* unused -- see above */
	short			ph_pad2;	/* unused -- see above */
} pollhead_t;

#if defined(_KERNEL)

/*
 * Routine called to notify a process of the occurrence
 * of an event.
 */
extern void pollwakeup(pollhead_t *, short);

/*
 * Internal routines.
 */
extern void polllock(pollhead_t *, kmutex_t *);
extern int pollunlock(void);
extern void pollrelock(int);
extern void pollcleanup(void);
extern void pollblockexit(struct fpollinfo *);
extern void pollcacheclean(struct fpollinfo *, int);

/*
 * public poll head interface:
 *
 *  pollhead_clean      clean up all polldats on a pollhead list
 */
extern void pollhead_clean(pollhead_t *);

#endif /* defined(_KERNEL) */

#endif /* defined(_KERNEL) || defined(_KMEMUSER) */

#if !defined(_KERNEL)
#if defined(__STDC__)
int poll(struct pollfd *, nfds_t, int);
#else
int poll();
#endif /* __STDC__ */
#endif /* !_KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_POLL_H */
