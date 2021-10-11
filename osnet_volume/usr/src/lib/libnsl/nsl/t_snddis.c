/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1993-96 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/


#pragma ident	"@(#)t_snddis.c	1.25	98/04/19 SMI"	/* SVr4.0 1.4.3.1 */

#include <rpc/trace.h>
#include <errno.h>
#include <unistd.h>
#include <stropts.h>
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
_tx_snddis(int fd, const struct t_call *call, int api_semantics)
{
	struct T_discon_req dreq;
	struct strbuf ctlbuf;
	struct strbuf databuf;
	struct _ti_user *tiptr;
	sigset_t mask;
	int sv_errno;
	int retval;

	trace2(TR_t_snddis, 0, fd);
	if ((tiptr = _t_checkfd(fd, 0, api_semantics)) == NULL) {
		sv_errno = errno;
		trace2(TR_t_snddis, 1, fd);
		errno = sv_errno;
		return (-1);
	}
	MUTEX_LOCK_THRMASK(&tiptr->ti_lock, mask);

	if (tiptr->ti_servtype == T_CLTS) {
		t_errno = TNOTSUPPORT;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_snddis, 1, fd);
		return (-1);
	}

	if (_T_IS_XTI(api_semantics)) {
		/*
		 * User level state verification only done for XTI
		 * because doing for TLI may break existing applications
		 * Note: This is documented in TLI man page but never
		 * done.
		 */
		if (! (tiptr->ti_state == T_DATAXFER ||
		    tiptr->ti_state == T_OUTCON ||
		    tiptr->ti_state == T_OUTREL ||
		    tiptr->ti_state == T_INREL ||
		    (tiptr->ti_state == T_INCON && tiptr->ti_ocnt > 0))) {
			t_errno = TOUTSTATE;
			MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
			trace2(TR_t_snddis, 1, fd);
			return (-1);
		}

		/*
		 * Following check only done for XTI as it may be a risk
		 * to existing buggy TLI applications.
		 */
	}

	if (call != NULL && call->udata.len) {
		if ((tiptr->ti_ddatasize == T_INVALID /* -2 */) ||
		    ((tiptr->ti_ddatasize != T_INFINITE /* -1*/) &&
			(call->udata.len >
			    (uint32_t)tiptr->ti_ddatasize))) {
			/*
			 * user data not valid with disconnect or it
			 * exceeds the limits specified by the
			 * transport provider
			 */
			t_errno = TBADDATA;
			MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
			trace2(TR_t_snddis, 1, fd);
			return (-1);
		}
	}

	/*
	 * If disconnect is done on a listener, the 'call' parameter
	 * must be non-null
	 */
	if ((tiptr->ti_state == T_INCON) &&
	    (call == NULL)) {
		t_errno = TBADSEQ;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_snddis, 1, fd);
		return (-1);
	}

	/*
	 * look at look buffer to see if there is a discon there
	 */

	if (_t_look_locked(fd, tiptr, 0, api_semantics) == T_DISCONNECT) {
		t_errno = TLOOK;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_snddis, 1, fd);
		return (-1);
	}

	if ((tiptr->ti_lookcnt > 0) && (call == 0))
		_t_flush_lookevents(tiptr); /* flush but not on listener */

	do {
		retval = _ioctl(fd, I_FLUSH, FLUSHW);
	} while (retval < 0 && errno == EINTR);
	if (retval < 0) {
		sv_errno = errno;
		t_errno = TSYSERR;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_snddis, 1, fd);
		errno = sv_errno;
		return (-1);
	}

	ctlbuf.len = (int)sizeof (struct T_discon_req);
	ctlbuf.maxlen = (int)sizeof (struct T_discon_req);
	ctlbuf.buf = (char *)&dreq;

	dreq.PRIM_type = T_DISCON_REQ;
	dreq.SEQ_number = (call? call->sequence: -1);

	databuf.maxlen = (call? call->udata.len: 0);
	databuf.len = (call? call->udata.len: 0);
	databuf.buf = (call? call->udata.buf: NULL);

	/*
	 * Calls to send data (write or putmsg) can potentially
	 * block, for MT case, we drop the lock and enable signals here
	 * and acquire it back
	 */
	MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
	if (putmsg(fd, &ctlbuf, (databuf.len? &databuf: NULL), 0) < 0) {
		sv_errno = errno;

		t_errno = TSYSERR;
		trace2(TR_t_snddis, 1, fd);
		errno = sv_errno;
		return (-1);
	}
	MUTEX_LOCK_THRMASK(&tiptr->ti_lock, mask);

	if (_t_is_ok(fd, tiptr, T_DISCON_REQ) < 0) {
		sv_errno = errno;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_snddis, 1, fd);
		errno = sv_errno;
		return (-1);
	}

	tiptr->ti_flags &= ~(MORE|EXPEDITED);

	if (tiptr->ti_ocnt <= 1) {
		if (tiptr->ti_state == T_INCON) {
			tiptr->ti_ocnt--;
			tiptr->ti_flags &= ~TX_TQFULL_NOTIFIED;
		}
		_T_TX_NEXTSTATE(T_SNDDIS1, tiptr,
				"t_snddis: invalid state event T_SNDDIS1");
	} else {
		if (tiptr->ti_state == T_INCON) {
			tiptr->ti_ocnt--;
			tiptr->ti_flags &= ~TX_TQFULL_NOTIFIED;
		}
		_T_TX_NEXTSTATE(T_SNDDIS2, tiptr,
				"t_snddis: invalid state event T_SNDDIS2");
	}

	MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
	trace2(TR_t_snddis, 1, fd);
	return (0);
}
