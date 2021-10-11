/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1993-96 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/

#pragma ident	"@(#)t_connect.c	1.25	98/04/19 SMI"	/* SVr4.0 1.7 */

#include <rpc/trace.h>
#include <stropts.h>
#include <stdlib.h>
#include <sys/timod.h>
#define	_SUN_TPI_VERSION 2
#include <sys/tihdr.h>
#include <xti.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <syslog.h>
#include "timt.h"
#include "tx.h"


/*
 * If a system call fails with EINTR after T_CONN_REQ is sent out,
 * we change state for caller to continue with t_rcvconnect(). This
 * semantics is not documented for TLI but is the direction taken with
 * XTI so we adopt it. With this the call establishment is completed
 * by calling t_rcvconnect() even for synchronous endpoints.
 */
int
_tx_connect(
	int fd,
	const struct t_call *sndcall,
	struct t_call *rcvcall,
	int api_semantics
)
{
	int fctlflg;
	struct _ti_user *tiptr;
	sigset_t mask, procmask;
	struct strbuf ctlbuf;
	int sv_errno;
	int didalloc;

	trace2(TR_t_connect, 0, fd);
	if ((tiptr = _t_checkfd(fd, 0, api_semantics)) == NULL) {
		sv_errno = errno;
		trace2(TR_t_connect, 1, fd);
		errno = sv_errno;
		return (-1);
	}

	MUTEX_LOCK_THRMASK(&tiptr->ti_lock, mask);
	if (_T_IS_XTI(api_semantics)) {
		/*
		 * User level state verification only done for XTI
		 * because doing for TLI may break existing applications
		 */
		if (tiptr->ti_state != T_IDLE) {
			t_errno = TOUTSTATE;
			MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
			trace2(TR_t_connect, 1, fd);
			return (-1);
		}
	}

	/*
	 * Acquire ctlbuf for use in sending/receiving control part
	 * of the message.
	 */
	if (_t_acquire_ctlbuf(tiptr, &ctlbuf, &didalloc) < 0) {
		sv_errno = errno;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_connect, 1, fd);
		errno = sv_errno;
		return (-1);
	}
	/*
	 * Block all signals until T_CONN_REQ sent and
	 * acked with T_OK_ACK/ERROR_ACK
	 */
	_t_blockallsignals(&procmask);
	if (_t_snd_conn_req(tiptr, sndcall, &ctlbuf) < 0) {
		sv_errno = errno;
		_t_restoresigmask(&procmask);
		errno = sv_errno;
		/*
		 * At the TPI level, the error returned in a T_ERROR_ACK
		 * received in response to a T_CONN_REQ for an attempt to
		 * establish a duplicate conection has changed to a
		 * new t_errno code introduced with XTI (ADDRBUSY).
		 * We need to adjust TLI error code to be same as before.
		 */
		if (_T_IS_TLI(api_semantics) && t_errno == TADDRBUSY)
			/* TLI only */
			t_errno = TBADADDR;

		goto err_out;
	}
	_t_restoresigmask(&procmask);

	if ((fctlflg = _fcntl(fd, F_GETFL, 0)) < 0) {
		t_errno = TSYSERR;
		goto err_out;
	}

	if (fctlflg & (O_NDELAY | O_NONBLOCK)) {
		_T_TX_NEXTSTATE(T_CONNECT2, tiptr,
				"t_connect: invalid state event T_CONNECT2");
		t_errno = TNODATA;
		goto err_out;
	}

	/*
	 * Note: The following call to _t_rcv_conn_con blocks awaiting
	 * T_CONN_CON from remote client. Therefore it drops the
	 * tiptr->lock during the call (and reacquires it)
	 */
	if (_t_rcv_conn_con(tiptr, rcvcall, &mask,
	    &ctlbuf, api_semantics) < 0) {
		if ((t_errno == TSYSERR && errno == EINTR) ||
		    t_errno == TLOOK) {
			_T_TX_NEXTSTATE(T_CONNECT2, tiptr,
				"t_connect: invalid state event T_CONNECT2");
		} else if (t_errno == TBUFOVFLW) {
			_T_TX_NEXTSTATE(T_CONNECT1, tiptr,
				"t_connect: invalid state event T_CONNECT1");
		}
		goto err_out;
	}
	_T_TX_NEXTSTATE(T_CONNECT1, tiptr,
				"t_connect: invalid state event T_CONNECT1");
	/*
	 * Update attributes which may have been negotiated during
	 * connection establishment for protocols where we suspect
	 * such negotiation is likely (e.g. OSI). We do not do it for
	 * all endpoints for performance reasons. Also, this code is
	 * deliberately done after user level state changes so even
	 * the (unlikely) failure case reflects a connected endpoint.
	 */
	if (tiptr->ti_tsdusize != 0) {
		if (_t_do_postconn_sync(fd, tiptr) < 0)
		    goto err_out;
	}


	if (didalloc)
		free(ctlbuf.buf);
	else
		tiptr->ti_ctlbuf = ctlbuf.buf;
	MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
	trace2(TR_t_connect, 1, fd);
	return (0);

err_out:
	sv_errno = errno;
	if (didalloc)
		free(ctlbuf.buf);
	else
		tiptr->ti_ctlbuf = ctlbuf.buf;
	MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);

	trace2(TR_t_connect, 1, fd);
	errno = sv_errno;
	return (-1);
}
