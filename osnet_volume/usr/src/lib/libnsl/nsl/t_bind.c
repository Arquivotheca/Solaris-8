/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1993-96 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/


#pragma ident	"@(#)t_bind.c	1.22	98/04/19 SMI"	/* SVr4.0 1.3.4.1 */

#include <stdlib.h>
#include <rpc/trace.h>
#include <errno.h>
#include <string.h>
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
_tx_bind(
	int fd,
	const struct t_bind *req,
	struct t_bind *ret,
	int api_semantics
)
{
	struct T_bind_req *bind_reqp;
	struct T_bind_ack *bind_ackp;
	int size, sv_errno, retlen;
	struct _ti_user *tiptr;
	sigset_t  procmask;

	int didalloc;
	int use_xpg41tpi;
	struct strbuf ctlbuf;

	trace2(TR_t_bind, 0, fd);
	if ((tiptr = _t_checkfd(fd, 0, api_semantics)) == NULL) {
		sv_errno = errno;
		trace2(TR_t_bind, 1, fd);
		errno = sv_errno;
		return (-1);
	}

	/*
	 * We need to block the process signals later.
	 * We do it early here so that we don't have to do the per
	 * thread blocking followed by per process blocking.
	 *
	 * We block all signals since TI_BIND which sends a TPI
	 * message O_T_BIND_REQ down which is not an idempotetent
	 * operation
	 */
	MUTEX_LOCK_PROCMASK(&tiptr->ti_lock, procmask);
	if (_T_IS_XTI(api_semantics)) {
		/*
		 * User level state verification only done for XTI
		 * because doing for TLI may break existing applications
		 */
		if (tiptr->ti_state != T_UNBND) {
			t_errno = TOUTSTATE;
			MUTEX_UNLOCK_PROCMASK(&tiptr->ti_lock, procmask);
			trace2(TR_t_bind, 1, fd);
			return (-1);
		}
	}
	/*
	 * Acquire buffer for use in sending/receiving the message.
	 * Note: assumes (correctly) that ti_ctlsize is large enough
	 * to hold sizeof (struct T_bind_req/ack)
	 */
	if (_t_acquire_ctlbuf(tiptr, &ctlbuf, &didalloc) < 0) {
		sv_errno = errno;
		MUTEX_UNLOCK_PROCMASK(&tiptr->ti_lock, procmask);
		trace2(TR_t_bind, 1, fd);
		errno = sv_errno;
		return (-1);
	}

	bind_reqp = (struct T_bind_req *)ctlbuf.buf;
	size = (int)sizeof (struct T_bind_req);

	use_xpg41tpi = (_T_IS_XTI(api_semantics)) &&
		((tiptr->ti_prov_flag & XPG4_1) != 0);
	if (use_xpg41tpi)
		/* XTI call and provider knows the XTI inspired TPI */
		bind_reqp->PRIM_type = T_BIND_REQ;
	else
		/* TLI caller old TPI provider */
		bind_reqp->PRIM_type = O_T_BIND_REQ;

	bind_reqp->ADDR_length = (req == NULL? 0: req->addr.len);
	bind_reqp->ADDR_offset = 0;
	bind_reqp->CONIND_number = (req == NULL? 0: req->qlen);


	if (bind_reqp->ADDR_length) {
		if (_t_aligned_copy(&ctlbuf, (int)bind_reqp->ADDR_length, size,
		    req->addr.buf, &bind_reqp->ADDR_offset) < 0) {
			/*
			 * Aligned copy will overflow buffer allocated based
			 * on transport maximum address length.
			 * return error.
			 */
			t_errno = TBADADDR;
			goto err_out;
		}
		size = bind_reqp->ADDR_offset + bind_reqp->ADDR_length;
	}

	if (_t_do_ioctl(fd, ctlbuf.buf, size, TI_BIND, &retlen) < 0) {
		goto err_out;
	}

	if (retlen < (int)sizeof (struct T_bind_ack)) {
		t_errno = TSYSERR;
		errno = EIO;
		goto err_out;
	}

	bind_ackp = (struct T_bind_ack *)ctlbuf.buf;

	if ((req != NULL) && req->addr.len != 0 &&
	    (use_xpg41tpi == 0) && (_T_IS_XTI(api_semantics))) {
		/*
		 * Best effort to do XTI on old TPI.
		 *
		 * Match address requested or unbind and fail with
		 * TADDRBUSY.
		 *
		 * XXX - Hack alert ! Should we do this at all ?
		 * Not "supported" as may not work if encoding of
		 * address is different in the returned address. This
		 * will also have trouble with TCP/UDP wildcard port
		 * requests
		 */
		if ((req->addr.len != bind_ackp->ADDR_length) ||
		    (memcmp(req->addr.buf, ctlbuf.buf +
		    bind_ackp->ADDR_offset, req->addr.len) != 0)) {
			(void) _tx_unbind_locked(fd, tiptr, &ctlbuf);
			t_errno = TADDRBUSY;
			goto err_out;
		}
	}

	tiptr->ti_ocnt = 0;
	tiptr->ti_flags &= ~TX_TQFULL_NOTIFIED;

	_T_TX_NEXTSTATE(T_BIND, tiptr, "t_bind: invalid state event T_BIND");

	if (ret != NULL) {
		if (_T_IS_TLI(api_semantics) || ret->addr.maxlen > 0) {
			if (TLEN_GT_NLEN(bind_reqp->ADDR_length,
			    ret->addr.maxlen)) {
				t_errno = TBUFOVFLW;
				goto err_out;
			}
			(void) memcpy(ret->addr.buf,
			    ctlbuf.buf + bind_ackp->ADDR_offset,
			    (size_t)bind_ackp->ADDR_length);
			ret->addr.len = bind_ackp->ADDR_length;
		}
		ret->qlen = bind_ackp->CONIND_number;
	}

	tiptr->ti_qlen = (uint) bind_ackp->CONIND_number;

	if (didalloc)
		free(ctlbuf.buf);
	else
		tiptr->ti_ctlbuf = ctlbuf.buf;
	MUTEX_UNLOCK_PROCMASK(&tiptr->ti_lock, procmask);
	trace2(TR_t_bind, 1, fd);
	return (0);
	/* NOTREACHED */
err_out:
	sv_errno = errno;
	if (didalloc)
		free(ctlbuf.buf);
	else
		tiptr->ti_ctlbuf = ctlbuf.buf;
	MUTEX_UNLOCK_PROCMASK(&tiptr->ti_lock, procmask);
	trace2(TR_t_bind, 1, fd);
	errno = sv_errno;
	return (-1);
}
