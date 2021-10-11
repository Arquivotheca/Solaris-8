/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1988, 1996-1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)select_large_fdset.c	1.3	99/09/08 SMI"	/* SVr4.0 1.4   */

/*LINTLIBRARY*/

/*
 * Emulation of select() system call using poll() system call.
 *
 * Assumptions:
 *	polling for input only is most common.
 *	polling for exceptional conditions is very rare.
 *
 * Note that is it not feasible to emulate all error conditions,
 * in particular conditions that would return EFAULT are far too
 * difficult to check for in a library routine.
 *
 * This is the alternate large fd_set select.
 *
 */

/*
 * We do not #redefine the name since the only users of this
 * are external to the libraries and commands.
 *
 *  #ifndef DSHLIB
 *  #pragma weak select_large_fdset = _select_large_fdset
 *  #endif
 */

/*
 * Must precede any include files
 */
#ifdef FD_SETSIZE
#undef FD_SETSIZE
#endif
#define	FD_SETSIZE 65536

#include "synonyms.h"
#include <values.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/poll.h>

#define	DEFAULT_POLL_SIZE 64

static struct pollfd *realloc_fds(int *, struct pollfd **,
	struct pollfd *);

int sz = 0;
int
select_large_fdset(int nfds, fd_set *in0, fd_set *out0, fd_set *ex0,
struct timeval *tv)
{
	long *in, *out, *ex;
	u_long m;	/* bit mask */
	int j;		/* loop counter */
	u_long b;	/* bits to test */
	int n, rv, ms;
	int lastj = -1;
	int nused;

	/*
	 * Rather than have a mammoth pollfd (65K) list on the stack
	 * we start with a small one and then malloc larger chunks
	 * on the heap if necessary.
	 */

	struct pollfd pfd[DEFAULT_POLL_SIZE];
	struct pollfd *p;
	struct pollfd *pfd_list;
	int nfds_on_list;

	fd_set zero;

	/*
	 * Check for invalid conditions at outset
	 * Required for spec1170
	 */
	if (nfds >= 0 && nfds <= FD_SETSIZE) {
		if (tv != NULL) {
			/* tv_usec is invalid */
			if (tv->tv_usec < 0 || tv->tv_usec >= 1000000) {
				errno = EINVAL;
				return (-1);
			}

			/* tv_sec is invalid */
			if ((tv->tv_sec < 0) || (tv->tv_sec > 100000000)) {
				errno = EINVAL;
				return (-1);
			}
		}
	} else {
		errno = EINVAL;
		return (-1);
	}

	/*
	 * If any input args are null, point them at the null array.
	 */
	memset(&zero, 0, sizeof (fd_set));
	if (in0 == NULL)
		in0 = &zero;
	if (out0 == NULL)
		out0 = &zero;
	if (ex0 == NULL)
		ex0 = &zero;

	nfds_on_list = DEFAULT_POLL_SIZE;
	pfd_list = pfd;
	p = pfd_list;
	memset(pfd, 0, sizeof (pfd));
	/*
	 * For each fd, if any bits are set convert them into
	 * the appropriate pollfd struct.
	 */
	in = (long *)in0->fds_bits;
	out = (long *)out0->fds_bits;
	ex = (long *)ex0->fds_bits;
	nused = 0;
	/*
	 * nused reflects the number of pollfd structs currently used
	 * less one. If realloc_fds returns 0 it is because malloc
	 * failed. We expect malloc() to have done the proper
	 * thing with errno.
	 */
	for (n = 0; n < nfds; n += NFDBITS) {
		b = (u_long)(*in | *out | *ex);
		for (j = 0, m = 1; b != 0; j++, b >>= 1, m <<= 1) {
			if (b & 1) {
				p->fd = n + j;
				if (p->fd < nfds) {
					p->events = 0;
					if (*in & m)
						p->events |= POLLRDNORM;
					if (*out & m)
						p->events |= POLLWRNORM;
					if (*ex & m)
						p->events |= POLLRDBAND;
					if (nused < (nfds_on_list - 1)) {
						p++;
					} else {
						p = realloc_fds(
						    &nfds_on_list,
						    &pfd_list, pfd);
						if (p == 0) {
						    if (pfd_list != pfd)
						    (void) free(pfd_list);
						    return (-1);
						}
					}
					nused++;
				} else
					goto done;
			}
		}
		in++;
		out++;
		ex++;
	}
done:
	/*
	 * Convert timeval to a number of millseconds.
	 */
	if (tv == NULL) {
		ms = -1;
	} else if (tv->tv_sec > (MAXINT) / 1000) {
		ms = MAXINT;
	} else {
		/*
		 * spec1170 complains if the value isn't rounded up.
		 */
		ms = (int)((tv->tv_sec * 1000) + (tv->tv_usec / 1000) +
		    (tv->tv_usec % 1000 ? 1 : 0));
	}

	/*
	 * Now do the poll.
	 */
retry:
	rv = poll(pfd_list, (u_long)nused, ms);
	if (rv < 0) {		/* no need to set bit masks */
		if (errno == EAGAIN)
			goto retry;
		if (pfd_list != pfd)
			(void) free(pfd_list);
		return (rv);
	} else if (rv == 0) {
		/*
		 * Clear out bit masks, just in case.
		 * On the assumption that usually only
		 * one bit mask is set, use three loops.
		 */
		if (in0 != &zero) {
			in = (long *)in0->fds_bits;
			for (n = 0; n < nfds; n += NFDBITS)
				*in++ = 0;
		}
		if (out0 != &zero) {
			out = (long *)out0->fds_bits;
			for (n = 0; n < nfds; n += NFDBITS)
				*out++ = 0;
		}
		if (ex0 != &zero) {
			ex = (long *)ex0->fds_bits;
			for (n = 0; n < nfds; n += NFDBITS)
				*ex++ = 0;
		}
		if (pfd_list != pfd)
			(void) free(pfd_list);
		return (0);
	}

	/*
	 * Check for EINVAL error case first to avoid changing any bits
	 * if we're going to return an error.
	 */
	for (p = pfd_list, j = nused; j-- > 0; p++) {
		/*
		 * select will return EBADF immediately if any fd's
		 * are bad.  poll will complete the poll on the
		 * rest of the fd's and include the error indication
		 * in the returned bits.  This is a rare case so we
		 * accept this difference and return the error after
		 * doing more work than select would've done.
		 */
		if (p->revents & POLLNVAL) {
			errno = EBADF;
			if (pfd_list != pfd)
				(void) free(pfd_list);
			return (-1);
		}
		/*
		 * We would like to make POLLHUP available to select,
		 * checking to see if we have pending data to be read.
		 * BUT until we figure out how not to break Xsun's
		 * dependencies on select's existing features...
		 * This is what we _thought_ would work ... sigh!
		 */
		/*
		 * if ((p->revents & POLLHUP) &&
		 *	!(p->revents & (POLLRDNORM|POLLRDBAND))) {
		 *	errno = EINTR;
		 *	return (-1);
		 * }
		 */
	}

	/*
	 * Convert results of poll back into bits
	 * in the argument arrays.
	 *
	 * We assume POLLRDNORM, POLLWRNORM, and POLLRDBAND will only be set
	 * on return from poll if they were set on input, thus we don't
	 * worry about accidentally setting the corresponding bits in the
	 * zero array if the input bit masks were null.
	 *
	 * Must return number of bits set, not number of ready descriptors
	 * (as the man page says, and as poll() does).
	 */
	rv = 0;
	for (p = pfd_list; nused-- > 0; p++) {
		j = (int)(p->fd / NFDBITS);
		/* have we moved into another word of the bit mask yet? */
		if (j != lastj) {
			/* clear all output bits to start with */
			in = (long *)&in0->fds_bits[j];
			out = (long *)&out0->fds_bits[j];
			ex = (long *)&ex0->fds_bits[j];
			/*
			 * In case we made "zero" read-only (e.g., with
			 * cc -R), avoid actually storing into it.
			 */
			if (in0 != &zero)
				*in = 0;
			if (out0 != &zero)
				*out = 0;
			if (ex0 != &zero)
				*ex = 0;
			lastj = j;
		}
		if (p->revents) {
			m = 1L << (p->fd % NFDBITS);
			if (p->revents & POLLRDNORM) {
				*in |= m;
				rv++;
			}
			if (p->revents & POLLWRNORM) {
				*out |= m;
				rv++;
			}
			if (p->revents & POLLRDBAND) {
				*ex |= m;
				rv++;
			}
			/*
			 * Only set this bit on return if we asked about
			 * input conditions.
			 */
			if ((p->revents & (POLLHUP|POLLERR)) &&
			    (p->events & POLLRDNORM)) {
				if ((*in & m) == 0)
					rv++;	/* wasn't already set */
				*in |= m;
			}
			/*
			 * Only set this bit on return if we asked about
			 * output conditions.
			 */
			if ((p->revents & (POLLHUP|POLLERR)) &&
			    (p->events & POLLWRNORM)) {
				if ((*out & m) == 0)
					rv++;	/* wasn't already set */
				*out |= m;
			}
			/*
			 * Only set this bit on return if we asked about
			 * output conditions.
			 */
			if ((p->revents & (POLLHUP|POLLERR)) &&
			    (p->events & POLLRDBAND)) {
				if ((*ex & m) == 0)
					rv++;   /* wasn't already set */
				*ex |= m;
			}
		}
	}
	if (pfd_list != pfd)
		(void) free(pfd_list);
	return (rv);
}
/*
 * Reallocate buffers of pollfds for our list. We malloc a new buffer
 * and, in the case where the old buffer does not match what is passed
 * in orig, free the buffer after copying the contents.
 */
struct pollfd *
realloc_fds(int *num, struct pollfd **list_head, struct pollfd *orig)
{
	struct pollfd *b;
	int nta;
	int n2;
	int i;

	n2 = *num * 2;
	nta = n2 * sizeof (struct pollfd);
	b = (struct pollfd *) malloc(nta);
	if (b) {
		(void) memset(b, 0, (size_t) nta);
		(void) memcpy(b, *list_head, nta / 2);
		if (*list_head != orig)
			(void) free (*list_head);
		*list_head = b;
		b += *num;
		*num = n2;
	}
	return (b);
}
