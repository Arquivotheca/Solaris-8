/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1993-96 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/

#pragma ident	"@(#)t_unbind.c	1.22	98/04/19 SMI"	/* SVr4.0 1.3.4.1 */

#include <rpc/trace.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stropts.h>
#include <sys/stream.h>
#define	_SUN_TPI_VERSION 2
#include <sys/tihdr.h>
#include <sys/timod.h>
#include <xti.h>
#include <signal.h>
#include <syslog.h>
#include "timt.h"
#include "tx.h"


int
_tx_unbind(int fd, int api_semantics)
{
	struct _ti_user *tiptr;
	sigset_t procmask;
	int sv_errno, retval, didalloc;
	struct strbuf ctlbuf;

	trace2(TR_t_unbind, 0, fd);
	if ((tiptr = _t_checkfd(fd, 0, api_semantics)) == NULL) {
		sv_errno = errno;
		trace2(TR_t_unbind, 1, fd);
		errno = sv_errno;
		return (-1);
	}

	if (_T_IS_XTI(api_semantics)) {
		/*
		 * User level state verification only done for XTI
		 * because doing for TLI may break existing applications
		 */
		if (tiptr->ti_state != T_IDLE) {
			t_errno = TOUTSTATE;
			trace2(TR_t_unbind, 1, fd);
			return (-1);
		}
	}

	/*
	 * We need to block all signals later for the process.
	 * We do it early to avoid doing it again.
	 * Since unbind is not an idempotent operation, we
	 * block signals around the call.
	 */
	MUTEX_LOCK_PROCMASK(&tiptr->ti_lock, procmask);
	/*
	 * Acquire buffer for use in sending/receiving the message.
	 * Note: assumes (correctly) that ti_ctlsize is large enough
	 * to hold sizeof (struct T_unbind_req/ack)
	 */
	if (_t_acquire_ctlbuf(tiptr, &ctlbuf, &didalloc) < 0) {
		sv_errno = errno;
		MUTEX_UNLOCK_PROCMASK(&tiptr->ti_lock, procmask);
		trace2(TR_t_bind, 1, fd);
		errno = sv_errno;
		return (-1);
	}

	retval = _tx_unbind_locked(fd, tiptr, &ctlbuf);

	sv_errno = errno;
	if (didalloc)
		free(ctlbuf.buf);
	else
		tiptr->ti_ctlbuf = ctlbuf.buf;
	MUTEX_UNLOCK_PROCMASK(&tiptr->ti_lock, procmask);
	trace2(TR_t_unbind, 1, fd);
	errno = sv_errno;
	return (retval);
}

int
_tx_unbind_locked(int fd, struct _ti_user *tiptr, struct strbuf *ctlbufp)
{
	struct T_unbind_req *unbind_reqp;
	int sv_errno, retlen;

	trace2(TR_t_unbind_locked, 0, fd);
	if (_t_is_event(fd, tiptr) < 0) {
		sv_errno = errno;
		trace2(TR_t_unbind, 1, fd);
		errno = sv_errno;
		return (-1);
	}

	unbind_reqp = (struct T_unbind_req *)ctlbufp->buf;
	unbind_reqp->PRIM_type = T_UNBIND_REQ;

	if (_t_do_ioctl(fd, (char *)unbind_reqp,
	    (int)sizeof (struct T_unbind_req), TI_UNBIND, &retlen) < 0) {
		goto err_out;
	}

	if (_ioctl(fd, I_FLUSH, FLUSHRW) < 0) {
		t_errno = TSYSERR;
		goto err_out;
	}

	/*
	 * clear more and expedited data bits
	 */
	tiptr->ti_flags &= ~(MORE|EXPEDITED);

	_T_TX_NEXTSTATE(T_UNBIND, tiptr,
			"t_unbind: invalid state event T_UNBIND");

	trace2(TR_t_unbind_locked, 1, fd);
	return (0);

err_out:
	sv_errno = errno;
	trace2(TR_t_unbind_locked, 1, fd);
	errno = sv_errno;
	return (-1);
}
