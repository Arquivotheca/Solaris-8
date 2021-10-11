/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1993-96 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/

#pragma ident	"@(#)_conn_util.c	1.23	98/04/19 SMI"
		/* SVr4.0 1.6.3.2	*/

#include <sys/param.h>
#include <sys/types.h>
#include <rpc/trace.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stropts.h>
#include <sys/stream.h>
#define	_SUN_TPI_VERSION 2
#include <sys/tihdr.h>
#include <sys/timod.h>
#include <xti.h>
#include <signal.h>
#include <assert.h>
#include "timt.h"
#include "tx.h"


/*
 * Snd_conn_req - send connect request message to transport provider.
 * All signals for the process blocked during the call to simplify design.
 * (This is OK for a bounded amount of time this routine is expected to
 * execute)
 * For MT case, assumes tiptr->ti_lock is held.
 */
int
_t_snd_conn_req(
	struct _ti_user *tiptr,
	const struct t_call *call,
	struct strbuf *ctlbufp)
{
	struct T_conn_req *creq;
	int size, sv_errno;
	int fd;

	trace2(TR__t_snd_conn_req, 0, fd);
	assert(MUTEX_HELD(&tiptr->ti_lock));
	fd = tiptr->ti_fd;

	if (tiptr->ti_servtype == T_CLTS) {
		t_errno = TNOTSUPPORT;
		trace2(TR__t_snd_conn_req, 1, fd);
		return (-1);
	}

	if (_t_is_event(fd, tiptr) < 0) {
		sv_errno = errno;
		trace2(TR__t_snd_conn_req, 1, fd);
		errno = sv_errno;
		return (-1);
	}

	creq = (struct T_conn_req *)ctlbufp->buf;
	creq->PRIM_type = T_CONN_REQ;
	creq->DEST_length = call->addr.len;
	creq->DEST_offset = 0;
	creq->OPT_length = call->opt.len;
	creq->OPT_offset = 0;
	size = (int)sizeof (struct T_conn_req); /* size without any buffers */

	if (call->addr.len) {
		if (_t_aligned_copy(ctlbufp, call->addr.len, size,
		    call->addr.buf, &creq->DEST_offset) < 0) {
			/*
			 * Aligned copy will overflow buffer allocated based
			 * based on transport maximum address size.
			 * return error.
			 */
			t_errno = TBADADDR;
			trace2(TR__t_snd_conn_req, 1, fd);
			return (-1);
		}
		size = creq->DEST_offset + creq->DEST_length;
	}
	if (call->opt.len) {
		if (_t_aligned_copy(ctlbufp, call->opt.len, size,
		    call->opt.buf, &creq->OPT_offset) < 0) {
			/*
			 * Aligned copy will overflow buffer allocated based
			 * on maximum option size in transport.
			 * return error.
			 */
			t_errno = TBADOPT;
			trace2(TR__t_snd_conn_req, 1, fd);
			return (-1);
		}
		size = creq->OPT_offset + creq->OPT_length;
	}
	if (call->udata.len) {
		if ((tiptr->ti_cdatasize == T_INVALID /* -2 */) ||
		    ((tiptr->ti_cdatasize != T_INFINITE /* -1 */) &&
			(call->udata.len > (uint32_t)tiptr->ti_cdatasize))) {
			/*
			 * user data not valid with connect or it
			 * exceeds the limits specified by the transport
			 * provider.
			 */
			t_errno = TBADDATA;
			trace2(TR__t_snd_conn_req, 1, fd);
			return (-1);
		}
	}

	ctlbufp->len = size;

	/*
	 * Assumes signals are blocked so putmsg() will not block
	 * indefinitely
	 */
	if (putmsg(fd, ctlbufp,
	    (struct strbuf *)(call->udata.len? &call->udata: NULL), 0) < 0) {
		sv_errno = errno;
		t_errno = TSYSERR;
		trace2(TR__t_snd_conn_req, 1, fd);
		errno = sv_errno;
		return (-1);
	}

	if (_t_is_ok(fd, tiptr, T_CONN_REQ) < 0) {
		sv_errno = errno;
		trace2(TR__t_snd_conn_req, 1, fd);
		errno = sv_errno;
		return (-1);
	}
	trace2(TR__t_snd_conn_req, 1, fd);
	return (0);
}



/*
 * Rcv_conn_con - get connection confirmation off
 * of read queue
 * Note:
 *      - called holding the tiptr->ti_lock  in the MT case.
 *      - we pass mask as parameter because it needs to drop
 *        and reacquire lock using MUTEX_(UN)LOCK_THRMASK.
 *        for processing performed in this routine.
 */
int
_t_rcv_conn_con(
	struct _ti_user *tiptr,
	struct t_call *call,
	sigset_t *maskp,
	struct strbuf *ctlbufp,
	int api_semantics)
{
	struct strbuf databuf;
	union T_primitives *pptr;
	int retval, fd, sv_errno;
	int didralloc;

	int flg = 0;

	trace2(TR__t_rcv_conn_con, 0, fd);

	fd = tiptr->ti_fd;

	if (tiptr->ti_servtype == T_CLTS) {
		t_errno = TNOTSUPPORT;
		trace2(TR__t_rcv_conn_con, 1, fd);
		return (-1);
	}

	/*
	 * see if there is something in look buffer
	 */
	if (tiptr->ti_lookcnt > 0) {
		t_errno = TLOOK;
		trace2(TR__t_rcv_conn_con, 1, fd);
		return (-1);
	}

	ctlbufp->len = 0;
	/*
	 * Acquire databuf for use in sending/receiving data part
	 */
	if (_t_acquire_databuf(tiptr, &databuf, &didralloc) < 0) {
		sv_errno = errno;
		trace2(TR__t_rcv_conn_con, 1, fd);
		errno = sv_errno;
		return (-1);
	}

	/*
	 * This is a call that may block indefinitely so we drop the
	 * lock and allow signals in MT case here and reacquire it.
	 * Error case should roll back state changes done above
	 * (happens to be no state change here)
	 */
	MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, *maskp);
	if ((retval = getmsg(fd, ctlbufp, &databuf, &flg)) < 0) {
		sv_errno = errno;
		if (errno == EAGAIN)
			t_errno = TNODATA;
		else
			t_errno = TSYSERR;
		MUTEX_LOCK_THRMASK(&tiptr->ti_lock, *maskp);
		errno = sv_errno;
		goto err_out;
	}
	MUTEX_LOCK_THRMASK(&tiptr->ti_lock, *maskp);

	if (databuf.len == -1) databuf.len = 0;

	/*
	 * did we get entire message
	 */
	if (retval > 0) {
		t_errno = TSYSERR;
		errno = EIO;
		goto err_out;
	}

	/*
	 * is cntl part large enough to determine message type?
	 */
	if (ctlbufp->len < (int)sizeof (t_scalar_t)) {
		t_errno = TSYSERR;
		errno = EPROTO;
		goto err_out;
	}

	pptr = (union T_primitives *)ctlbufp->buf;

	switch (pptr->type) {

	case T_CONN_CON:

		if ((ctlbufp->len < (int)sizeof (struct T_conn_con)) ||
		    (pptr->conn_con.OPT_length != 0 &&
		    (ctlbufp->len < (int)(pptr->conn_con.OPT_length +
		    pptr->conn_con.OPT_offset)))) {
			t_errno = TSYSERR;
			errno = EPROTO;
			goto err_out;
		}

		if (call != NULL) {
			/*
			 * Note: Buffer overflow is an error in XTI
			 * only if netbuf.maxlen > 0
			 */
			if (_T_IS_TLI(api_semantics) || call->addr.maxlen > 0) {
				if (TLEN_GT_NLEN(pptr->conn_con.RES_length,
				    call->addr.maxlen)) {
					t_errno = TBUFOVFLW;
					goto err_out;
				}
				(void) memcpy(call->addr.buf,
				    ctlbufp->buf + pptr->conn_con.RES_offset,
				    (size_t)pptr->conn_con.RES_length);
				call->addr.len = pptr->conn_con.RES_length;
			}
			if (_T_IS_TLI(api_semantics) || call->opt.maxlen > 0) {
				if (TLEN_GT_NLEN(pptr->conn_con.OPT_length,
				    call->opt.maxlen)) {
					t_errno = TBUFOVFLW;
					goto err_out;
				}
				(void) memcpy(call->opt.buf,
				    ctlbufp->buf + pptr->conn_con.OPT_offset,
				    (size_t)pptr->conn_con.OPT_length);
				call->opt.len = pptr->conn_con.OPT_length;
			}
			if (_T_IS_TLI(api_semantics) ||
			    call->udata.maxlen > 0) {
				if (databuf.len > (int)call->udata.maxlen) {
					t_errno = TBUFOVFLW;
					goto err_out;
				}
				(void) memcpy(call->udata.buf, databuf.buf,
				    (size_t)databuf.len);
				call->udata.len = databuf.len;
			}
			/*
			 * since a confirmation seq number
			 * is -1 by default
			 */
			call->sequence = (int)-1;
		}
		if (didralloc)
			free(databuf.buf);
		else
			tiptr->ti_rcvbuf = databuf.buf;
		trace2(TR__t_rcv_conn_con, 1, fd);
		return (0);

	case T_DISCON_IND:

		/*
		 * if disconnect indication then append it to
		 * the "look bufffer" list.
		 * This may result in MT case for the process
		 * signal mask to be temporarily masked to
		 * ensure safe memory allocation.
		 */

		if (_t_register_lookevent(tiptr, databuf.buf, databuf.len,
					ctlbufp->buf, ctlbufp->len) < 0) {
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
	trace2(TR__t_rcv_conn_con, 1, fd);
	errno = EPROTO;
err_out:
	sv_errno = errno;
	if (didralloc)
		free(databuf.buf);
	else
		tiptr->ti_rcvbuf = databuf.buf;
	trace2(TR__t_rcv_conn_con, 1, fd);
	errno = sv_errno;
	return (-1);
}
