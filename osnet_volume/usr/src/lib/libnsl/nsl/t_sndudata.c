/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1993-96 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/


#pragma ident	"@(#)t_sndudata.c	1.23	98/04/19 SMI"	/* SVr4.0 1.5 */

/*
 * t_sndudata.c and t_sndvudata.c are very similar and contain common code.
 * Any changes to either of them should be reviewed to see whether they
 * are applicable to the other file.
 */
#include <rpc/trace.h>
#include <stdlib.h>
#include <errno.h>
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
_tx_sndudata(int fd, const struct t_unitdata *unitdata, int api_semantics)
{
	struct T_unitdata_req *udreq;
	struct strbuf ctlbuf;
	int size;
	struct _ti_user *tiptr;
	sigset_t mask;
	int sv_errno;
	int didalloc;

	trace2(TR_t_sndudata, 0, fd);
	if ((tiptr = _t_checkfd(fd, 0, api_semantics)) == NULL) {
		sv_errno = errno;
		trace2(TR_t_sndudata, 1, fd);
		errno = sv_errno;
		return (-1);
	}
	MUTEX_LOCK_THRMASK(&tiptr->ti_lock, mask);

	if (tiptr->ti_servtype != T_CLTS) {
		t_errno = TNOTSUPPORT;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_sndudata, 1, fd);
		return (-1);
	}

	if (_T_IS_XTI(api_semantics)) {
		/*
		 * User level state verification only done for XTI
		 * because doing for TLI may break existing applications
		 */
		if (tiptr->ti_state != T_IDLE) {
			t_errno = TOUTSTATE;
			MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
			trace2(TR_t_sndudata, 1, fd);
			return (-1);
		}
	}

	if (((int)unitdata->udata.len == 0) &&
	    !(tiptr->ti_prov_flag & (SENDZERO|OLD_SENDZERO))) {
		t_errno = TBADDATA;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_sndudata, 1, fd);
		return (-1);
	}

	if ((tiptr->ti_maxpsz > 0) &&
	    (unitdata->udata.len > (uint32_t)tiptr->ti_maxpsz)) {
		if (_T_IS_TLI(api_semantics)) {
			t_errno = TSYSERR;
			errno = EPROTO;
		} else
			t_errno = TBADDATA;
		sv_errno = errno;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_sndudata, 1, fd);
		errno = sv_errno;
		return (-1);
	}

	/*
	 * Acquire ctlbuf for use in sending/receiving control part
	 * of the message.
	 */
	if (_t_acquire_ctlbuf(tiptr, &ctlbuf, &didalloc) < 0) {
		sv_errno = errno;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_sndudata, 1, fd);
		errno = sv_errno;
		return (-1);
	}

	udreq = (struct T_unitdata_req *)ctlbuf.buf;

	udreq->PRIM_type = T_UNITDATA_REQ;
	udreq->DEST_length = unitdata->addr.len;
	udreq->DEST_offset = 0;
	udreq->OPT_length = unitdata->opt.len;
	udreq->OPT_offset = 0;
	size = (int)sizeof (struct T_unitdata_req);

	if (unitdata->addr.len) {
		if (_t_aligned_copy(&ctlbuf, unitdata->addr.len, size,
		    unitdata->addr.buf, &udreq->DEST_offset) < 0) {
			/*
			 * Aligned copy based will overflow buffer
			 * allocated based on maximum transport address
			 * size information
			 */
			t_errno = TSYSERR;
			errno = EPROTO;
			goto err_out;
		}
		size = udreq->DEST_offset + udreq->DEST_length;
	}
	if (unitdata->opt.len) {
		if (_t_aligned_copy(&ctlbuf, unitdata->opt.len, size,
		    unitdata->opt.buf, &udreq->OPT_offset) < 0) {
			/*
			 * Aligned copy based will overflow buffer
			 * allocated based on maximum transport option
			 * size information
			 */
			t_errno = TSYSERR;
			errno = EPROTO;
			goto err_out;
		}
		size = udreq->OPT_offset + udreq->OPT_length;
	}

	if (size > (int)ctlbuf.maxlen) {
		t_errno = TSYSERR;
		errno = EIO;
		goto err_out;
	}

	ctlbuf.len = size;

	/*
	 * Calls to send data (write or putmsg) can potentially
	 * block, for MT case, we drop the lock and enable signals here
	 * and acquire it back.
	 * At this point, we are sure SENDZERO is supported and the
	 * putmsg below may send a zero length message,
	 * (i.e with valid control part, but zero data part)
	 */
	MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
	if (putmsg(fd, &ctlbuf, (struct strbuf *)&unitdata->udata, 0) < 0) {
		if (errno == EAGAIN)
			t_errno = TFLOW;
		else
			t_errno = TSYSERR;
		sv_errno = errno;
		MUTEX_LOCK_THRMASK(&tiptr->ti_lock, mask);
		errno = sv_errno;
		goto err_out;
	}
	MUTEX_LOCK_THRMASK(&tiptr->ti_lock, mask);

	_T_TX_NEXTSTATE(T_SNDUDATA, tiptr,
			"t_sndudata: invalid state event T_SNDUDATA");
	if (didalloc)
		free(ctlbuf.buf);
	else
		tiptr->ti_ctlbuf = ctlbuf.buf;
	MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
	trace2(TR_t_sndudata, 1, fd);
	return (0);
err_out:
	sv_errno = errno;
	if (didalloc)
		free(ctlbuf.buf);
	else
		tiptr->ti_ctlbuf = ctlbuf.buf;
	MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
	trace2(TR_t_sndudata, 1, fd);
	errno = sv_errno;
	return (-1);
}
