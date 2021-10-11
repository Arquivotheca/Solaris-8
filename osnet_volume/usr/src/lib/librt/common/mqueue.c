/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident   "@(#)mqueue.c 1.25     99/08/13     SMI"

/*LINTLIBRARY*/

#include <mqueue.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdarg.h>
#include <limits.h>
#include <pthread.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <inttypes.h>

#include "mqlib.h"
#include "pos4obj.h"
#include "pos4.h"

#define	MQ_ALIGNSIZE	(sizeof (void *))

#define	MQ_ASSERT(x) \
	assert(x);

/* CSTYLED */
#define	MQ_ASSERT_PTR(_m,_p) \
	assert((_p) != NULL && !((uintptr_t) (_p) & (MQ_ALIGNSIZE -1)) && \
	    !((uintptr_t) _m + (uintptr_t) (_p) >= (uintptr_t) _m + \
	    _m->mq_totsize));

#ifndef NDEBUG
/* CSTYLED */
#define	MQ_ASSERT_SEMVAL_LEQ(sem,val) { \
	int _val; \
	(void) sem_getvalue((sem), &_val); \
	assert((_val) <= val); }
#else
/* CSTYLED */
#define	MQ_ASSERT_SEMVAL_LEQ(sem,val)
#endif

#define	MQ_PTR(m, n)	((msghdr_t *)((uintptr_t)m + (uintptr_t)n))
#define	HEAD_PTR(m, n)	(msghdr_t **)((uintptr_t)m + \
			(uintptr_t)(m->mq_headpp + n))
#define	TAIL_PTR(m, n)	(msghdr_t **)((uintptr_t)m + \
			(uintptr_t)(m->mq_tailpp + n))

static	long open_count = 0; /* ref count to track the number of open M.Q.s */
static	mutex_t	mqlock = DEFAULTMUTEX;

static void
mq_init(mqhdr_t *mqhp, size_t msgsize, ssize_t maxmsg)
{
	int	i;
	size_t	temp;
	msghdr_t *currentp;
	msghdr_t *nextp;

	/*
	 * We only need to initialize the non-zero fields.  The use of
	 * ftruncate() on the message queue file assures that the
	 * pages will be zfod.
	 */
	(void) sem_init(&mqhp->mq_exclusive, 1, 1);
	(void) sem_init(&mqhp->mq_rblocked, 1, 0);
	(void) sem_init(&mqhp->mq_notempty, 1, 0);
	(void) sem_init(&mqhp->mq_notfull, 1, (unsigned int)maxmsg);

	mqhp->mq_maxsz = msgsize;
	mqhp->mq_maxmsg = maxmsg;

	/*
	 * As of this writing (1997), there are 32 message queue priorities.
	 * If this is to change, then the size of the mq_mask will also
	 * have to change.  If NDEBUG isn't defined, assert that
	 * MQ_MAXPRIO hasn't changed.
	 */
	mqhp->mq_maxprio = MQ_MAXPRIO;
	MQ_ASSERT(sizeof (mqhp->mq_maxprio) * 8 >= MQ_MAXPRIO);

	mqhp->mq_magic = MQ_MAGIC;

	/*
	 * Since the message queue can be mapped into different
	 * virtual address ranges by different processes, we don't
	 * keep track of pointers, only offsets into the shared region.
	 */
	mqhp->mq_headpp = (msghdr_t **)sizeof (mqhdr_t);
	mqhp->mq_tailpp = mqhp->mq_headpp + mqhp->mq_maxprio;
	mqhp->mq_freep = (msghdr_t *)(mqhp->mq_tailpp + mqhp->mq_maxprio);

	currentp = mqhp->mq_freep;
	MQ_PTR(mqhp, currentp)->msg_next = (msghdr_t *)0;

	temp = (mqhp->mq_maxsz + MQ_ALIGNSIZE - 1) & ~(MQ_ALIGNSIZE - 1);
	for (i = 1; i < mqhp->mq_maxmsg; i++) {
		nextp = (msghdr_t *)((uintptr_t)&currentp[1] + temp);
		MQ_PTR(mqhp, currentp)->msg_next = nextp;
		MQ_PTR(mqhp, nextp)->msg_next = (msghdr_t *)0;
		currentp = nextp;

	}
}

static size_t
mq_getmsg(mqhdr_t *mqhp, char *msgp, unsigned int *msg_prio)
{
	msghdr_t *currentp;
	msghdr_t *curbuf;
	msghdr_t **headpp;
	msghdr_t **tailpp;

	MQ_ASSERT_SEMVAL_LEQ(&mqhp->mq_exclusive, 0);

	/*
	 * Get the head and tail pointers for the queue of maximum
	 * priority.  We shouldn't be here unless there is a message for
	 * us, so it's fair to assert that both the head and tail
	 * pointers are non-NULL.
	 */
	headpp = HEAD_PTR(mqhp, mqhp->mq_curmaxprio);
	tailpp = TAIL_PTR(mqhp, mqhp->mq_curmaxprio);
	MQ_ASSERT_PTR(mqhp, headpp);
	MQ_ASSERT_PTR(mqhp, tailpp);

	if (msg_prio != NULL)
		*msg_prio = (int)mqhp->mq_curmaxprio;

	currentp = *headpp;
	MQ_ASSERT_PTR(mqhp, currentp);

	curbuf = MQ_PTR(mqhp, currentp);
	MQ_ASSERT_PTR(mqhp, curbuf);

	if ((*headpp = curbuf->msg_next) == NULL) {
		/*
		 * We just nuked the last message in this priority's queue.
		 * Twiddle this priority's bit, and then find the next bit
		 * tipped.
		 */
		size_t prio = mqhp->mq_curmaxprio;

		mqhp->mq_mask &= ~(1 << prio);

		for (; prio != 0; prio--)
			if (mqhp->mq_mask & (1 << prio))
				break;
		mqhp->mq_curmaxprio = prio;

		*tailpp = NULL;
	}

	/*
	 * Copy the message, and put the buffer back on the free list.
	 */
	(void) memcpy(msgp, (char *) &curbuf[1], curbuf->msg_len);
	curbuf->msg_next = mqhp->mq_freep;
	mqhp->mq_freep = currentp;

	return (curbuf->msg_len);
}


static void
mq_putmsg(mqhdr_t *mqhp, const char *msgp, ssize_t len, size_t prio)
{
	msghdr_t *currentp;
	msghdr_t *curbuf;
	msghdr_t **headpp;
	msghdr_t **tailpp;

	MQ_ASSERT_SEMVAL_LEQ(&mqhp->mq_exclusive, 0);

	/*
	 * Grab a free message block, and link it in.  We shouldn't
	 * be here unless there is room in the queue for us;  it's
	 * fair to assert that the free pointer is non-NULL.
	 */
	currentp = mqhp->mq_freep;
	curbuf = MQ_PTR(mqhp, currentp);
	MQ_ASSERT_PTR(mqhp, curbuf);

	/*
	 * Remove a message from the free list, and copy in the new contents.
	 */
	mqhp->mq_freep = curbuf->msg_next;
	curbuf->msg_next = NULL;
	(void) memcpy((char *) &curbuf[1], msgp, len);
	curbuf->msg_len = len;

	headpp = HEAD_PTR(mqhp, prio);
	tailpp = TAIL_PTR(mqhp, prio);

	MQ_ASSERT_PTR(mqhp, headpp);
	MQ_ASSERT_PTR(mqhp, tailpp);

	if (*tailpp == NULL) {
		/*
		 * This is the first message on this queue.  Set the
		 * head and tail pointers, and tip the appropriate bit
		 * in the priority mask.
		 */
		*headpp = currentp;
		*tailpp = currentp;
		mqhp->mq_mask |= (1 << prio);
		if (prio > mqhp->mq_curmaxprio)
			mqhp->mq_curmaxprio = prio;
	} else {
		MQ_ASSERT_PTR(mqhp, *tailpp);
		MQ_PTR(mqhp, *tailpp)->msg_next = currentp;
		*tailpp = currentp;
	}
}

mqd_t
mq_open(const char *path, int oflag, /* mode_t mode, mq_attr *attr */ ...)
{
	va_list	ap;
	mode_t	mode;
	struct	mq_attr *attr;
	int	fd;
	int	err;
	int	cr_flag = 0;
	size_t	total_size;
	size_t	msgsize;
	ssize_t	maxmsg;
	size_t	temp;
	mqdes_t	*mqdp;
	mqhdr_t	*mqhp;
	struct mq_dn	*mqdnp;
	long	max_mq_val;

	if (__pos4obj_check(path) == -1) {
		return ((mqd_t)-1);
	}

	/* check that we do not exceed the max M.Q.s for this process */
	(void) mutex_lock(&mqlock);
	max_mq_val = sysconf(_SC_MQ_OPEN_MAX);
	if ((max_mq_val == -1) || (open_count >= max_mq_val)) {
		errno = EMFILE;
		(void) mutex_unlock(&mqlock);
		return ((mqd_t)-1);
	}
	(void) mutex_unlock(&mqlock);

	/* acquire MSGQ lock to have atomic operation */
	if (__pos4obj_lock(path, MQ_LOCK_TYPE) < 0) {
		return ((mqd_t)-1);
	}

	va_start(ap, oflag);
	/* filter oflag to have READ/WRITE/CREATE modes only */
	oflag = oflag & (O_RDONLY|O_WRONLY|O_RDWR|O_CREAT|O_EXCL|O_NONBLOCK);
	if ((oflag & O_CREAT) != 0) {
		mode = va_arg(ap, mode_t);
		attr = va_arg(ap, struct mq_attr *);
	}
	va_end(ap);

	if ((fd = __pos4obj_open(path, MQ_PERM_TYPE, oflag,
	    mode, &cr_flag)) < 0) {
		goto out;
	}

	/* closing permission file */
	(void) close(fd);

	/* Try to open/create data file */
	if (cr_flag) {
		cr_flag = PFILE_CREATE;
		if (attr == NULL) {
			maxmsg = MQ_MAXMSG;
			msgsize = MQ_MAXSIZE;
		} else if ((attr->mq_maxmsg <= 0) ||
					(attr->mq_msgsize <= 0)) {
			goto out;

		} else {
			maxmsg = attr->mq_maxmsg;
			msgsize = attr->mq_msgsize;
		}

		/* adjust for message size at word boundary */
		temp = (msgsize+ MQ_ALIGNSIZE - 1)
			& ~(MQ_ALIGNSIZE - 1);

		total_size = sizeof (mqhdr_t) +
			maxmsg * (temp + sizeof (msghdr_t)) +
			2 * MQ_MAXPRIO * sizeof (msghdr_t *);

		/*
		 * data file is opened with read/write to those
		 * who have read or write permission
		 */
		mode = mode | (mode & 0444) >> 1 | (mode & 0222) << 1;
		if ((fd = __pos4obj_open(path, MQ_DATA_TYPE,
			(O_RDWR|O_CREAT|O_EXCL), mode, &err)) < 0)
			goto out;

		cr_flag |= DFILE_CREATE | DFILE_OPEN;

		/* force permissions to avoid umask effect */
		if (fchmod(fd, mode) < 0)
			goto out;

		if (ftruncate(fd, (off_t)total_size) < 0)
			goto out;

	} else {
		if ((fd = __pos4obj_open(path, MQ_DATA_TYPE,
					O_RDWR, 0666, &err)) < 0)
			goto out;

		cr_flag = DFILE_OPEN;
		if (read(fd, &total_size, sizeof (size_t)) <= 0)
			goto out;

		/* Message queue has not been initialized yet */
		if (total_size == 0) {
			errno = ENOENT;
			goto out;
		}
	}

	if ((mqdp = (mqdes_t *)malloc(sizeof (mqdes_t))) == NULL) {
		errno = ENOMEM;
		goto out;
	}
	cr_flag |= ALLOC_MEM;

	/* LINTED */
	if ((mqhp = (mqhdr_t *)mmap(0, total_size, PROT_READ|PROT_WRITE,
	    MAP_SHARED, fd, 0)) == MAP_FAILED)
		goto out;
	cr_flag |= DFILE_MMAP;

	/* closing data file */
	(void) close(fd);
	cr_flag &= ~DFILE_OPEN;

	/*
	 * create, unlink, size, mmap, and close description file
	 * all for a flag word in anonymous shared memory
	 */
	if ((fd = __pos4obj_open(path, MQ_DSCN_TYPE, O_RDWR | O_CREAT,
	    0666, &err)) < 0)
		goto out;
	cr_flag |= DFILE_OPEN;
	(void) __pos4obj_unlink(path, MQ_DSCN_TYPE);
	if (ftruncate(fd, sizeof (struct mq_dn)) < 0)
		goto out;

	/* LINTED */
	if ((mqdnp = (struct mq_dn *)mmap(0, sizeof (struct mq_dn),
	    PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED)
		goto out;
	(void) close(fd);
	cr_flag &= ~DFILE_OPEN;

	/*
	 * we follow the same strategy as filesystem open() routine,
	 * where fcntl.h flags are changed to flags defined in file.h.
	 */
	mqdp->mqd_flags = (oflag - FOPEN) & (FREAD|FWRITE);
	mqdnp->mqdn_flags = (oflag - FOPEN) & (FNONBLOCK);

	/* new message queue requires initialization */
	if ((cr_flag & DFILE_CREATE) != 0) {
		/* message queue header has to be initialized */
		mq_init(mqhp, msgsize, maxmsg);
		mqhp->mq_totsize = total_size;
	}
	(void) mutex_lock(&mqlock);
	open_count++;
	(void) mutex_unlock(&mqlock);
	mqdp->mqd_mq = mqhp;
	mqdp->mqd_mqdn = mqdnp;
	mqdp->mqd_magic = MQ_MAGIC;
	if (__pos4obj_unlock(path, MQ_LOCK_TYPE) < 0)
		return (mqd_t)(-1);

	return ((mqd_t)mqdp);

out:
	err = errno;
	if ((cr_flag & DFILE_OPEN) != 0)
		(void) close(fd);
	if ((cr_flag & DFILE_CREATE) != 0)
		(void) __pos4obj_unlink(path, MQ_DATA_TYPE);
	if ((cr_flag & PFILE_CREATE) != 0)
		(void) __pos4obj_unlink(path, MQ_PERM_TYPE);
	if ((cr_flag & ALLOC_MEM) != 0)
		free((void *)mqdp);
	if ((cr_flag & DFILE_MMAP) != 0)
		(void) munmap((void *)mqhp, total_size);
	errno = err;
	(void) __pos4obj_unlock(path, MQ_LOCK_TYPE);
	return (mqd_t)(-1);
}

int
mq_close(mqd_t mqdes)
{
	mqdes_t *mqdp = (mqdes_t *)mqdes;
	mqhdr_t *mqhp;
	struct mq_dn *mqdnp;

	if (mqdp == NULL || (uintptr_t)mqdp == -1 ||
		mqdp->mqd_magic != MQ_MAGIC) {
			errno = EBADF;
			return (-1);
	}

	mqhp = mqdp->mqd_mq;
	mqdnp = mqdp->mqd_mqdn;

	while (sem_wait(&mqhp->mq_exclusive) == -1 && errno == EINTR)
		continue;

	if (mqhp->mq_des == mqdp && mqhp->mq_sigid.sn_pid == getpid()) {
		/* Notification is set for this descriptor, remove it */
		(void) __signotify(SN_CANCEL, NULL, &mqhp->mq_sigid);
		mqhp->mq_sigid.sn_pid = 0;
		mqhp->mq_des = 0;
	}
	(void) sem_post(&mqhp->mq_exclusive);

	(void) mutex_lock(&mqlock);
	open_count--;
	(void) mutex_unlock(&mqlock);
	/* Invalidate the descriptor before freeing it */
	mqdp->mqd_magic = 0;
	free(mqdp);

	(void) munmap((caddr_t)mqdnp, sizeof (struct mq_dn));
	return (munmap((caddr_t)mqhp, mqhp->mq_totsize));
}

int
mq_unlink(const char *path)
{
	int err;

	if (__pos4obj_check(path) < 0)
		return (-1);

	if (__pos4obj_lock(path, MQ_LOCK_TYPE) < 0) {
		return (-1);
	}

	err = __pos4obj_unlink(path, MQ_PERM_TYPE);

	if (err == 0 || (err == -1 && errno == EEXIST)) {
		errno = 0;
		err = __pos4obj_unlink(path, MQ_DATA_TYPE);
	}

	if (__pos4obj_unlock(path, MQ_LOCK_TYPE) < 0)
		return (-1);

	return (err);

}

int
mq_send(mqd_t mqdes, const char *msg_ptr, size_t msg_len, unsigned int msg_prio)
{
	mqdes_t *mqdp = (mqdes_t *)mqdes;
	mqhdr_t *mqhp;
	int notify = 0;

	if (mqdp == NULL || (uintptr_t)mqdp == -1 ||
		(mqdp->mqd_magic != MQ_MAGIC) ||
		((mqdp->mqd_flags & FWRITE) == 0)) {
		errno = EBADF;
		return (-1);
	}

	mqhp = mqdp->mqd_mq;

	if ((size_t)msg_prio >= mqhp->mq_maxprio) {
		errno = EINVAL;
		return (-1);
	}
	if (msg_len > mqhp->mq_maxsz) {
		errno = EMSGSIZE;
		return (-1);
	}

	if ((mqdp->mqd_mqdn->mqdn_flags & O_NONBLOCK) != 0) {
		if (sem_trywait(&mqhp->mq_notfull) == -1) {
			/*
			 * errno has been set to EAGAIN or EINTR by
			 * sem_trywait(), so we can just return.
			 */
			return (-1);
		}
	} else {
		if (sem_wait(&mqhp->mq_notfull) == -1)
			return (-1);
	}

	/*
	 * By the time we're here, we know that we've got the capacity
	 * to add to the queue...now acquire the exclusive lock.
	 */
	if (sem_wait(&mqhp->mq_exclusive) == -1) {
		/*
		 * We must have been interrupted by a signal.  Post
		 * on mq_notfull so someone else can take our slot.
		 */
		(void) sem_post(&mqhp->mq_notfull);
		errno = EINTR;
		return (-1);
	}

	/*
	 * Now determine if we want to kick the notification.  POSIX
	 * requires that if a process has registered for notification,
	 * we must kick it when the queue makes an empty to non-empty
	 * transition, and there are no blocked receivers.  Note that
	 * this mechanism does _not_ guarantee that the kicked process
	 * will be able to receive a message without blocking;  another
	 * receiver could intervene in the meantime.  Thus,
	 * the notification mechanism is inherently racy;  all we can
	 * do is hope to minimize the window as much as possible.  In
	 * general, we want to avoid kicking the notification when
	 * there are clearly receivers blocked.  We'll determine if we
	 * want to kick the notification before the mq_putmsg(), but the
	 * actual signotify() won't be done until the message is on
	 * the queue.
	 */
	if (mqhp->mq_sigid.sn_pid != 0) {
		int nmessages, nblocked;
		(void) sem_getvalue(&mqhp->mq_notempty, &nmessages);
		(void) sem_getvalue(&mqhp->mq_rblocked, &nblocked);

		if (nmessages == 0 && nblocked == 0)
			notify = 1;
	}

	mq_putmsg(mqhp, msg_ptr, (ssize_t)msg_len, msg_prio);

	/*
	 * The ordering here is important.  We want to make sure that
	 * one has to have mq_exclusive before being able to kick
	 * mq_notempty.
	 */
	(void) sem_post(&mqhp->mq_notempty);

	if (notify) {
		(void) __signotify(SN_SEND, NULL, &mqhp->mq_sigid);
		mqhp->mq_sigid.sn_pid = 0;
		mqhp->mq_des = 0;
	}

	(void) sem_post(&mqhp->mq_exclusive);
	MQ_ASSERT_SEMVAL_LEQ(&mqhp->mq_notempty, ((int)mqhp->mq_maxmsg));

	return (0);
}

ssize_t
mq_receive(mqd_t mqdes, char *msg_ptr, size_t msg_len, unsigned int *msg_prio)
{
	mqdes_t *mqdp = (mqdes_t *)mqdes;
	mqhdr_t *mqhp;
	ssize_t	msg_size;

	if (mqdp == NULL || (uintptr_t)mqdp == -1 ||
		(mqdp->mqd_magic != MQ_MAGIC) ||
		((mqdp->mqd_flags & FREAD) == 0)) {
			errno = EBADF;
			return (ssize_t)(-1);
	}

	mqhp = mqdp->mqd_mq;

	if (msg_len < mqhp->mq_maxsz) {
		errno = EMSGSIZE;
		return (ssize_t)(-1);
	}

	/*
	 * The semaphoring scheme for mq_receive is a little hairier
	 * thanks to POSIX.1b's arcane notification mechanism.  First,
	 * we try to take the common case and do a sem_trywait().
	 * If that doesn't work, and O_NONBLOCK hasn't been set,
	 * then note that we're going to sleep by incrementing the rblocked
	 * semaphore.  We decrement that semaphore after waking up.
	 */
	if (sem_trywait(&mqhp->mq_notempty) == -1) {
		if ((mqdp->mqd_mqdn->mqdn_flags & O_NONBLOCK) != 0)
			/*
			 * errno has been set to EAGAIN or EINTR by
			 * sem_trywait(), so we can just return.
			 */
			return (ssize_t)(-1);
		/*
		 * If we're here, then we're probably going to block...
		 * increment the rblocked semaphore.
		 */
		(void) sem_post(&mqhp->mq_rblocked);

		if (sem_wait(&mqhp->mq_notempty) == -1) {
			/*
			 * Took a signal while waiting on mq_notempty...
			 * decrement the rblocked count, and blow out
			 * of Dodge.
			 */
			while (sem_wait(&mqhp->mq_rblocked) == -1)
				continue;
			return (ssize_t)(-1);
		}
		while (sem_wait(&mqhp->mq_rblocked) == -1)
			continue;
	}

	if (sem_wait(&mqhp->mq_exclusive) == -1) {
		/*
		 * We must have been interrupted by a signal.  Post
		 * on mq_notfull so someone else can take our message.
		 */
		(void) sem_post(&mqhp->mq_notempty);
		errno = EINTR;
		return (-1);
	}

	msg_size = mq_getmsg(mqhp, msg_ptr, msg_prio);

	(void) sem_post(&mqhp->mq_exclusive);
	(void) sem_post(&mqhp->mq_notfull);
	MQ_ASSERT_SEMVAL_LEQ(&mqhp->mq_notfull, ((int)mqhp->mq_maxmsg));

	return (msg_size);
}

int
mq_notify(mqd_t mqdes, const struct sigevent *notification)
{
	mqdes_t *mqdp = (mqdes_t *)mqdes;
	mqhdr_t *mqhp;
	siginfo_t mq_siginfo;

	if (mqdp == NULL || (uintptr_t)mqdp == -1 ||
		mqdp->mqd_magic != MQ_MAGIC) {
			errno = EBADF;
			return (-1);
	}

	mqhp = mqdp->mqd_mq;

	(void) sem_wait(&mqhp->mq_exclusive);

	if (notification == NULL) {
		if (mqhp->mq_des == mqdp &&
				mqhp->mq_sigid.sn_pid == getpid()) {
			/*
			 * Remove signotify_id if queue is registered with
			 * this process
			 */
			(void) __signotify(SN_CANCEL, NULL, &mqhp->mq_sigid);
			mqhp->mq_sigid.sn_pid = 0;
			mqhp->mq_des = 0;
		} else {
			/*
			 * if registered with another process or mqdes
			 */
			errno = EBUSY;
			goto bad;
		}
	} else {
		/*
		 * Register notification with this process.
		 */

		switch (notification->sigev_notify) {
		case SIGEV_NONE:
			mq_siginfo.si_signo = 0;
			mq_siginfo.si_code = SI_MESGQ;
			break;
		case SIGEV_SIGNAL:
			mq_siginfo.si_signo = notification->sigev_signo;
			mq_siginfo.si_value = notification->sigev_value;
			mq_siginfo.si_code = SI_MESGQ;
			break;
		case SIGEV_THREAD:
			errno = ENOSYS;
			goto bad;
		default:
			errno = EINVAL;
			goto bad;
		}

		/*
		 * Either notification is not present, or if
		 * notification is already present, but the process
		 * which registered notification does not exist then
		 * kernel can register notification for current process.
		 */

		if (__signotify(SN_PROC, &mq_siginfo, &mqhp->mq_sigid) < 0)
			goto bad;
		mqhp->mq_des = mqdp;
	}

	(void) sem_post(&mqhp->mq_exclusive);

	return (0);

bad:
	(void) sem_post(&mqhp->mq_exclusive);
	return (-1);
}


int
mq_setattr(mqd_t mqdes, const struct mq_attr *mqstat, struct mq_attr *omqstat)
{
	mqdes_t *mqdp = (mqdes_t *)mqdes;
	mqhdr_t *mqhp;
	uint_t	flag = 0;

	if (mqdp == 0 || (uintptr_t)mqdp == -1 || mqdp->mqd_magic != MQ_MAGIC) {
		errno = EBADF;
		return (-1);
	}

	/* store current attributes */
	if (omqstat != NULL) {
		int	count;

		mqhp = mqdp->mqd_mq;
		omqstat->mq_flags = mqdp->mqd_mqdn->mqdn_flags;
		omqstat->mq_maxmsg = mqhp->mq_maxmsg;
		omqstat->mq_msgsize = mqhp->mq_maxsz;
		(void) sem_getvalue(&mqhp->mq_notempty, &count);
		omqstat->mq_curmsgs = count;
	}

	/* set description attributes */
	if ((mqstat->mq_flags & O_NONBLOCK) != 0)
		flag = FNONBLOCK;
	mqdp->mqd_mqdn->mqdn_flags = flag;

	return (0);
}

int
mq_getattr(mqd_t mqdes, struct mq_attr *mqstat)
{
	mqdes_t *mqdp = (mqdes_t *)mqdes;
	mqhdr_t *mqhp;
	int count;

	if (mqdp == NULL || (uintptr_t)mqdp == -1 ||
		mqdp->mqd_magic != MQ_MAGIC) {
			errno = EBADF;
			return (-1);
	}

	mqhp = mqdp->mqd_mq;

	mqstat->mq_flags = mqdp->mqd_mqdn->mqdn_flags;
	mqstat->mq_maxmsg = mqhp->mq_maxmsg;
	mqstat->mq_msgsize = mqhp->mq_maxsz;
	(void) sem_getvalue(&mqhp->mq_notempty, &count);
	mqstat->mq_curmsgs = count;
	return (0);
}
