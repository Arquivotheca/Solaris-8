/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1993-96 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/


#pragma ident	"@(#)t_listen.c	1.22	98/04/19 SMI"	/* SVr4.0 1.5 */

#include <rpc/trace.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stropts.h>
#include <sys/stream.h>
#define	_SUN_TPI_VERSION 2
#include <sys/tihdr.h>
#include <sys/timod.h>
#include <xti.h>
#include <syslog.h>
#include "timt.h"
#include "tx.h"

int
_tx_listen(int fd, struct t_call *call, int api_semantics)
{
	struct strbuf ctlbuf;
	struct strbuf databuf;
	int retval;
	union T_primitives *pptr;
	struct _ti_user *tiptr;
	sigset_t mask;
	int sv_errno;
	int didalloc, didralloc;
	int flg = 0;

	trace2(TR_t_listen, 0, fd);
	if ((tiptr = _t_checkfd(fd, 0, api_semantics)) == NULL) {
		sv_errno = errno;
		trace2(TR_t_listen, 1, fd);
		errno = sv_errno;
		return (-1);
	}


	MUTEX_LOCK_THRMASK(&tiptr->ti_lock, mask);

	if (tiptr->ti_servtype == T_CLTS) {
		sv_errno = errno;
		t_errno = TNOTSUPPORT;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_listen, 1, fd);
		errno = sv_errno;
		return (-1);
	}
	if (_T_IS_XTI(api_semantics)) {
		/*
		 * User level state verification only done for XTI
		 * because doing for TLI may break existing applications
		 */
		if (! (tiptr->ti_state == T_IDLE ||
		    tiptr->ti_state == T_INCON)) {
			t_errno = TOUTSTATE;
			MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
			trace2(TR_t_connect, 1, fd);
			return (-1);
		}

		if (tiptr->ti_qlen == 0) {
			t_errno = TBADQLEN;
			MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
			trace2(TR_t_connect, 1, fd);
			return (-1);
		}

		if (tiptr->ti_ocnt == tiptr->ti_qlen) {
			if (!(tiptr->ti_flags & TX_TQFULL_NOTIFIED)) {
				tiptr->ti_flags |= TX_TQFULL_NOTIFIED;
				t_errno = TQFULL;
				MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
				trace2(TR_t_connect, 1, fd);
				return (-1);
			}
		}

	}

	/*
	 * check if something in look buffer
	 */
	if (tiptr->ti_lookcnt > 0) {
		t_errno = TLOOK;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_listen, 1, fd);
		return (-1);
	}

	/*
	 * Acquire ctlbuf for use in sending/receiving control part
	 * of the message.
	 */
	if (_t_acquire_ctlbuf(tiptr, &ctlbuf, &didalloc) < 0) {
		sv_errno = errno;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_listen, 1, fd);
		errno = sv_errno;
		return (-1);
	}
	/*
	 * Acquire databuf for use in sending/receiving data part
	 */
	if (_t_acquire_databuf(tiptr, &databuf, &didralloc) < 0) {
		int sv_errno = errno;

		if (didalloc)
			free(ctlbuf.buf);
		else
			tiptr->ti_ctlbuf = ctlbuf.buf;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_listen, 1, fd);
		errno = sv_errno;
		return (-1);
	}

	/*
	 * This is a call that may block indefinitely so we drop the
	 * lock and allow signals in MT case here and reacquire it.
	 * Error case should roll back state changes done above
	 * (happens to be no state change here)
	 */
	MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
	if ((retval = getmsg(fd, &ctlbuf, &databuf, &flg)) < 0) {
		if (errno == EAGAIN)
			t_errno = TNODATA;
		else
			t_errno = TSYSERR;
		sv_errno = errno;
		MUTEX_LOCK_THRMASK(&tiptr->ti_lock, mask);
		errno = sv_errno;
		goto err_out;
	}
	MUTEX_LOCK_THRMASK(&tiptr->ti_lock, mask);

	if (databuf.len == -1) databuf.len = 0;

	/*
	 * did I get entire message?
	 */
	if (retval > 0) {
		t_errno = TSYSERR;
		errno = EIO;
		goto err_out;
	}

	/*
	 * is ctl part large enough to determine type
	 */
	if (ctlbuf.len < (int)sizeof (t_scalar_t)) {
		t_errno = TSYSERR;
		errno = EPROTO;
		goto err_out;
	}

	pptr = (union T_primitives *)ctlbuf.buf;

	switch (pptr->type) {

	case T_CONN_IND:
		if ((ctlbuf.len < (int)sizeof (struct T_conn_ind)) ||
		    (ctlbuf.len < (int)(pptr->conn_ind.OPT_length
		    + pptr->conn_ind.OPT_offset))) {
			t_errno = TSYSERR;
			errno = EPROTO;
			goto err_out;
		}
		/*
		 * Change state and increment outstanding connection
		 * indication count and instantiate "sequence" return
		 * parameter.
		 * Note: It is correct semantics accoring to spec to
		 * do this despite possibility of TBUFOVFLW error later.
		 * The spec treats TBUFOVFLW error in general as a special case
		 * which can be ignored by applications that do not
		 * really need the stuff returned in 'netbuf' structures.
		 */
		_T_TX_NEXTSTATE(T_LISTN, tiptr,
				"t_listen:invalid state event T_LISTN");
		tiptr->ti_ocnt++;
		call->sequence = pptr->conn_ind.SEQ_number;

		if (_T_IS_TLI(api_semantics) || call->addr.maxlen > 0) {
			if (TLEN_GT_NLEN(pptr->conn_ind.SRC_length,
			    call->addr.maxlen)) {
				t_errno = TBUFOVFLW;
				goto err_out;
			}
			(void) memcpy(call->addr.buf, ctlbuf.buf +
			    (size_t)pptr->conn_ind.SRC_offset,
			(unsigned int)pptr->conn_ind.SRC_length);
			call->addr.len = pptr->conn_ind.SRC_length;
		}
		if (_T_IS_TLI(api_semantics) || call->opt.maxlen > 0) {
			if (TLEN_GT_NLEN(pptr->conn_ind.OPT_length,
			    call->opt.maxlen)) {
				t_errno = TBUFOVFLW;
				goto err_out;
			}
			(void) memcpy(call->opt.buf, ctlbuf.buf +
			    pptr->conn_ind.OPT_offset,
			    (size_t)pptr->conn_ind.OPT_length);
			call->opt.len = pptr->conn_ind.OPT_length;
		}
		if (_T_IS_TLI(api_semantics) || call->udata.maxlen > 0) {
			if (databuf.len > (int)call->udata.maxlen) {
				t_errno = TBUFOVFLW;
				goto err_out;
			}
			(void) memcpy(call->udata.buf, databuf.buf,
			    (size_t)databuf.len);
			call->udata.len = databuf.len;
		}

		if (didalloc)
			free(ctlbuf.buf);
		else
			tiptr->ti_ctlbuf = ctlbuf.buf;
		if (didralloc)
			free(databuf.buf);
		else
			tiptr->ti_rcvbuf = databuf.buf;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_listen, 1, fd);
		return (0);

	case T_DISCON_IND:
		/*
		 * Append to the events in the "look buffer"
		 * list of events. This routine may block
		 * process signal mask in MT case.
		 */
		if (_t_register_lookevent(tiptr, databuf.buf,
					databuf.len, ctlbuf.buf,
					ctlbuf.len) < 0) {
			t_errno = TSYSERR;
			errno = ENOMEM;
			goto err_out;
		}
		t_errno = TLOOK;
		goto err_out;

	default:
		break;
	}

	t_errno = TSYSERR;
	errno = EPROTO;
err_out:
	sv_errno = errno;

	if (didalloc)
		free(ctlbuf.buf);
	else
		tiptr->ti_ctlbuf = ctlbuf.buf;
	if (didralloc)
		free(databuf.buf);
	else
		tiptr->ti_rcvbuf = databuf.buf;
	MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
	trace2(TR_t_listen, 1, fd);
	errno = sv_errno;
	return (-1);
}
