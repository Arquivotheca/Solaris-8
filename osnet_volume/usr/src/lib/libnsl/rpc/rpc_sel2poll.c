/*
 * Copyright (c) 1991, 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)rpc_sel2poll.c	1.9	97/10/30 SMI"

#include <sys/select.h>
#include <sys/types.h>
#include <rpc/trace.h>
#include <sys/time.h>
#include <sys/poll.h>
#include "rpc_mt.h"


/*
 *	Given an fd_set pointer and the number of bits to check in it,
 *	initialize the supplied pollfd array for RPC's use (RPC only
 *	polls for input events).  We return the number of pollfd slots
 *	we initialized.
 */
int
__rpc_select_to_poll(
	int	fdmax,		/* number of bits we must test */
	fd_set	*fdset,		/* source fd_set array */
	struct pollfd	*p0)	/* target pollfd array */
{
	/* register declarations ordered by expected frequency of use */
	register int j;		/* loop counter */
	register int n;
	register struct pollfd	*p = p0;

	/*
	 * For each fd, if the appropriate bit is set convert it into
	 * the appropriate pollfd struct.
	 */
	trace2(TR___rpc_select_to_poll, 0, fdmax);
	j = ((fdmax >= FD_SETSIZE) ? FD_SETSIZE : fdmax);
	for (n = 0; n < j; n++) {
		if (FD_ISSET(n, fdset)) {
			p->fd = n;
			p->events = MASKVAL;
			p->revents = 0;
			p++;
		}
	}
	trace2(TR___rpc_select_to_poll, 1, fdmax);
	return (p - p0);
}

/*
 *	Arguments are similar to rpc_select_to_poll() except that
 *	the second argument is pointer to an array of pollfd_t
 *	which is the source array which will be compressed and
 *	copied to the target array in p0.  The size of the
 *	source argument is given by pollfdmax. The array can be
 *	sparse. The space for the target is allocated before
 *	calling this function. It should have atleast pollfdmax
 *	elements.  This function scans the source pollfd array
 *	and copies only the valid ones to the target p0.
 */
int
__rpc_compress_pollfd(int pollfdmax, pollfd_t *srcp, pollfd_t *p0)
{
	int n;
	pollfd_t *p = p0;

	trace2(TR___rpc_compress_pollfd, 0, p0);

	for (n = 0; n < pollfdmax; n++) {
		if (POLLFD_ISSET(n, srcp)) {
			p->fd = srcp[n].fd;
			p->events = MASKVAL;
			p->revents = 0;
			p++;
		}
	}

	trace2(TR___rpc_compress_pollfd, 1, pollfdmax);
	return (p - p0);
}

/*
 *	Convert from timevals (used by select) to milliseconds (used by poll).
 */
int
__rpc_timeval_to_msec(struct timeval *t)
{
	int	t1, tmp;

	/*
	 * We're really returning t->tv_sec * 1000 + (t->tv_usec / 1000)
	 * but try to do so efficiently.  Note:  1000 = 1024 - 16 - 8.
	 */
	trace1(TR___rpc_timeval_to_msec, 0);
	tmp = (int)t->tv_sec << 3;
	t1 = -tmp;
	t1 += t1 << 1;
	t1 += tmp << 7;
	if (t->tv_usec)
		t1 += t->tv_usec / 1000;

	trace1(TR___rpc_timeval_to_msec, 1);
	return (t1);
}
