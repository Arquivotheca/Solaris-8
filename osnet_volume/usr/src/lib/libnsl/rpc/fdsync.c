/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)fdsync.c 1.6	99/11/11 SMI"

/*
 * This file contains functions which enables rpc library to synchronize
 * between various thread while they compete for a particular file descriptor
 */

#include "rpc_mt.h"
#include <rpc/rpc.h>
#include <rpc/trace.h>
#include <errno.h>
#include <sys/poll.h>
#include <syslog.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>

extern int lock_value;
#ifdef DEBUG
int mydb = 1;
#endif

/* Special mutex specific for RPC file descriptors */

typedef struct {
    thread_t owner;		/* lock owner			*/
    mutex_t mon;		/* monitor for recursive lock	*/
    unsigned int busy;		/* busy count			*/
    cond_t notbusy;		/* indicates not busy		*/
} rmutex_t;

/*
* A block holds an array of maxBlockSize cell and associated locks
*/

#define	CELLTBLSZ	1024

typedef struct rpcfd_block {
	struct rpcfd_block 	*next; /* Next Block */
	struct rpcfd_block 	*prev; /* prev Block */
	int 	end;  /* fd of last lock in the list */
	rmutex_t	lock[CELLTBLSZ];
} rpcfd_block_t;

mutex_t	rpc_fd_list_lock = DEFAULTMUTEX;	/* protects list manipulation */

/* Special mutex access methods */
static void
rmutex_init(rmutex_t *rlock, int type, void *arg)
{
	rlock->busy = 0;
	rlock->owner = -1;
	mutex_init(&rlock->mon, type, arg);
	cond_init(&rlock->notbusy, type, arg);
}

static int
rmutex_lock(rmutex_t *rlock)
{
	mutex_lock(&rlock->mon);
	if (rlock->busy != 0) {
		/* lock is busy. is thread the owner/ */
		if (rlock->owner != thr_self()) {
			/* not owner, wait until lock is not busy */
			do {
				cond_wait(&rlock->notbusy, &rlock->mon);
			} while (rlock->busy != 0);
		}
	}
	rlock->owner = thr_self();
	rlock->busy++;
	mutex_unlock(&rlock->mon);
	return (0);
}

static int
rmutex_trylock(rmutex_t *rlock)
{
	mutex_lock(&rlock->mon);
	if ((rlock->busy != 0) && (rlock->owner != thr_self())) {
		/* can't take the lock now -> return EBUSY */
		mutex_unlock(&rlock->mon);
		return (EBUSY);
	} else {
		/* take the lock */
		rlock->owner = thr_self();
		rlock->busy++;
		mutex_unlock(&rlock->mon);
		return (0);
	}
}

/*
 * OK to call cond_signal after dropping the lock since we
 * can't have a missed wakeup here; since rmutex_lock always
 * checks the busy count before calling cond_wait, it will
 * see that the count has dropped to 0 even if the signal was
 * delivered early.
 */

static int
rmutex_unlock(rmutex_t *rlock)
{
	unsigned int ownershipcnt;

	mutex_lock(&rlock->mon);
	ownershipcnt = --rlock->busy;
	mutex_unlock(&rlock->mon);
	if (ownershipcnt == 0)
		cond_signal(&rlock->notbusy);
	return (0);
}

/* Following functions create and manipulates the dgfd lock object */

static rmutex_t *search(const void *handle, int fd);
static rpcfd_block_t *create_block(const void *handle, int fd);

void *
rpc_fd_init(void)
{
	/*
	 * Create first chunk of CELLTBLSZ
	 */
	return (create_block(NULL, 0));
}

int
rpc_fd_lock(const void *handle, int fd)
{
	rmutex_t *mp;
	rpcfd_block_t *p;

	mp = search(handle, fd);

	if (mp == NULL) {
		p = create_block(handle, fd);
		if (p == NULL)
			return (ENOMEM);

		mp = &p->lock[fd % CELLTBLSZ];
	}

	return (rmutex_lock(mp));
}

int
rpc_fd_unlock(const void *handle, int fd)
{
	rmutex_t *mp;

	mp = search(handle, fd);
	if (mp == NULL) {
		/*
		 * Unlocking a bogus fd!
		 */
		return (EINVAL);
	}

	return (rmutex_unlock(mp));
}

int
rpc_fd_trylock(const void *handle, int fd)
{
	rmutex_t *mp;
	rpcfd_block_t *p;

	mp = search(handle, fd);

	if (mp == NULL) {
		p = create_block(handle, fd);
		if (p == NULL)
			return (ENOMEM);

		mp = &p->lock[fd % CELLTBLSZ];
	}

	return (rmutex_trylock(mp));
}

int
rpc_fd_destroy(const void *handle, int fd)
{
/*
 * Don't allow destroy
 */
	return (0);
}


static rpcfd_block_t *
create_block(const void *handle, int fd)
{
	rpcfd_block_t *l, *lprev;
	rpcfd_block_t *p;
	int i;

	p = malloc(sizeof (rpcfd_block_t));
	if (p == (rpcfd_block_t *)NULL) {
		return (NULL);
	}

	for (i = 0; i < CELLTBLSZ; i++) {
		rmutex_init(&p->lock[i], USYNC_THREAD, NULL);
	}
	p->end = (((fd + CELLTBLSZ) / CELLTBLSZ) * CELLTBLSZ) - 1;
	mutex_lock(&rpc_fd_list_lock);
	lprev = NULL;
	for (l = (rpcfd_block_t *)handle; l; l = l->next) {
		lprev = l;
		if (fd < l->end) {
			break;
		}
	}

	p->next = l;
	p->prev = lprev;
	if (lprev) lprev->next = p;
	if (l) l->prev = p;

	mutex_unlock(&rpc_fd_list_lock);

	return (p);
}


static rmutex_t *
search(const void *handle, int fd)
{
	rpcfd_block_t *p;
	bool_t	found = FALSE;

	mutex_lock(&rpc_fd_list_lock);
	for (p = (rpcfd_block_t *)handle; p; p = p->next) {
		if (fd <= p->end) {
			found = TRUE;
			break;
		}
	}
	mutex_unlock(&rpc_fd_list_lock);

	if (found == TRUE)
		return (&p->lock[fd % CELLTBLSZ]);
	return (NULL);
}
