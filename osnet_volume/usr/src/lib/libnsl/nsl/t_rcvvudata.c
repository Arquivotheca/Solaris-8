/*	Copyright (c) 1998 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/


#pragma ident	"@(#)t_rcvvudata.c	1.2	98/04/19 SMI"

/*
 * t_rcvudata.c and t_rcvvudata.c are very similar and contain common code.
 * Any changes to either of them should be reviewed to see whether they
 * are applicable to the other file.
 */
#include <rpc/trace.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stropts.h>
#include <sys/stream.h>
#define	_SUN_TPI_VERSION 2
#include <sys/tihdr.h>
#include <sys/timod.h>
#include <xti.h>
#include <syslog.h>
#include <assert.h>
#include "timt.h"
#include "tx.h"


int
_tx_rcvvudata(
	int fd,
	struct t_unitdata *unitdata,
	struct t_iovec *tiov,
	unsigned int tiovcount,
	int *flags,
	int api_semantics
)
{
	struct strbuf ctlbuf;
	struct strbuf databuf;
	char *dataptr;
	int retval;
	union T_primitives *pptr;
	struct _ti_user *tiptr;
	sigset_t mask;
	int sv_errno;
	int didalloc;
	int flg = 0;
	unsigned int nbytes;

	trace2(TR_t_rcvvudata, 0, fd);
	assert(api_semantics == TX_XTI_XNS5_API);

	if (tiovcount == 0 || tiovcount > T_IOV_MAX) {
		t_errno = TBADDATA;
		return (-1);
	}

	if ((tiptr = _t_checkfd(fd, 0, api_semantics)) == NULL) {
		sv_errno = errno;
		trace2(TR_t_rcvvudata, 1, fd);
		errno = sv_errno;
		return (-1);
	}
	MUTEX_LOCK_THRMASK(&tiptr->ti_lock, mask);

	if (tiptr->ti_servtype != T_CLTS) {
		t_errno = TNOTSUPPORT;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_rcvvudata, 1, fd);
		return (-1);
	}

	if (tiptr->ti_state != T_IDLE) {
		t_errno = TOUTSTATE;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_rcvvudata, 1, fd);
		return (-1);
	}

	/*
	 * check if there is something in look buffer
	 */
	if (tiptr->ti_lookcnt > 0) {
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_rcvvudata, 1, fd);
		t_errno = TLOOK;
		return (-1);
	}

	/*
	 * Acquire ctlbuf for use in sending/receiving control part
	 * of the message.
	 */
	if (_t_acquire_ctlbuf(tiptr, &ctlbuf, &didalloc) < 0) {
		sv_errno = errno;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_rcvvudata, 1, fd);
		errno = sv_errno;
		return (-1);
	}

	*flags = 0;

	nbytes = _t_bytecount_upto_intmax(tiov, tiovcount);
	dataptr = NULL;
	if (nbytes != 0 && ((dataptr = malloc(nbytes)) == NULL)) {
		t_errno = TSYSERR;
		goto err_out;
	}

	databuf.maxlen = nbytes;
	databuf.len = 0;
	databuf.buf = dataptr;

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

	/*
	 * is there control piece with data?
	 */
	if (ctlbuf.len > 0) {
		if (ctlbuf.len < (int)sizeof (t_scalar_t)) {
			t_errno = TSYSERR;
			errno = EPROTO;
			goto err_out;
		}

		pptr = (union T_primitives *)ctlbuf.buf;

		switch (pptr->type) {

		case T_UNITDATA_IND:
			if ((ctlbuf.len <
			    (int)sizeof (struct T_unitdata_ind)) ||
			    (pptr->unitdata_ind.OPT_length &&
			    (ctlbuf.len < (int)(pptr->unitdata_ind.OPT_length
			    + pptr->unitdata_ind.OPT_offset)))) {
				t_errno = TSYSERR;
				errno = EPROTO;
				goto err_out;
			}

			if (unitdata->addr.maxlen > 0) {
				if (TLEN_GT_NLEN(pptr->unitdata_ind.SRC_length,
				    unitdata->addr.maxlen)) {
					t_errno = TBUFOVFLW;
					goto err_out;
				}
				(void) memcpy(unitdata->addr.buf,
				    ctlbuf.buf + pptr->unitdata_ind.SRC_offset,
				    (size_t)pptr->unitdata_ind.SRC_length);
				unitdata->addr.len =
				    pptr->unitdata_ind.SRC_length;
			}
			if (unitdata->opt.maxlen > 0) {
				if (TLEN_GT_NLEN(pptr->unitdata_ind.OPT_length,
				    unitdata->opt.maxlen)) {
					t_errno = TBUFOVFLW;
					goto err_out;
				}
				(void) memcpy(unitdata->opt.buf, ctlbuf.buf +
				    pptr->unitdata_ind.OPT_offset,
				    (size_t)pptr->unitdata_ind.OPT_length);
				unitdata->opt.len =
					pptr->unitdata_ind.OPT_length;
			}
			if (retval & MOREDATA)
				*flags |= T_MORE;
			/*
			 * No state changes happens on T_RCVUDATA
			 * event (NOOP). We do it only to log errors.
			 */
			_T_TX_NEXTSTATE(T_RCVUDATA, tiptr,
			    "t_rcvvudata: invalid state event T_RCVUDATA");

			if (didalloc)
				free(ctlbuf.buf);
			else
				tiptr->ti_ctlbuf = ctlbuf.buf;
			_t_scatter(&databuf, tiov, tiovcount);
			if (dataptr != NULL)
				free(dataptr);
			MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
			trace2(TR_t_rcvvudata, 1, fd);
			return (databuf.len);

		case T_UDERROR_IND:
			if (_t_register_lookevent(tiptr, 0, 0, ctlbuf.buf,
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
		goto err_out;

	} else {		/* else part of "if (ctlbuf.len > 0)" */
		unitdata->addr.len = 0;
		unitdata->opt.len = 0;
		/*
		 * only data in message no control piece
		 */
		if (retval & MOREDATA)
			*flags = T_MORE;
		/*
		 * No state transition occurs on
		 * event T_RCVUDATA. We do it only to
		 * log errors.
		 */
		_T_TX_NEXTSTATE(T_RCVUDATA, tiptr,
		    "t_rcvvudata: invalid state event T_RCVUDATA");
		if (didalloc)
			free(ctlbuf.buf);
		else
			tiptr->ti_ctlbuf = ctlbuf.buf;
		_t_scatter(&databuf, tiov, tiovcount);
		if (dataptr != NULL)
			free(dataptr);
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_rcvvudata, 0, fd);
		return (databuf.len);
	}
	/* NOTREACHED */
err_out:
	sv_errno = errno;
	if (didalloc)
		free(ctlbuf.buf);
	else
		tiptr->ti_ctlbuf = ctlbuf.buf;
	if (dataptr != NULL)
		free(dataptr);
	MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
	trace2(TR_t_rcvvudata, 1, fd);
	errno = sv_errno;
	return (-1);
}
