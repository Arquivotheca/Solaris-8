/*	Copyright (c) 1998 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/

#pragma ident	"@(#)t_sndvudata.c	1.2	98/04/19 SMI"

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
#include <assert.h>
#include "timt.h"
#include "tx.h"

int
_tx_sndvudata(int fd, const struct t_unitdata *unitdata, struct t_iovec *tiov,
    unsigned int tiovcount, int api_semantics)
{
	struct T_unitdata_req *udreq;
	struct strbuf ctlbuf;
	struct strbuf databuf;
	int size;
	struct _ti_user *tiptr;
	sigset_t mask;
	int sv_errno;
	int didalloc;
	char *dataptr;
	unsigned int nbytes;

	trace2(TR_t_sndvudata, 0, fd);
	assert(api_semantics == TX_XTI_XNS5_API);
	if ((tiptr = _t_checkfd(fd, 0, api_semantics)) == NULL) {
		sv_errno = errno;
		trace2(TR_t_sndvudata, 1, fd);
		errno = sv_errno;
		return (-1);
	}
	MUTEX_LOCK_THRMASK(&tiptr->ti_lock, mask);

	if (tiptr->ti_servtype != T_CLTS) {
		t_errno = TNOTSUPPORT;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_sndvudata, 1, fd);
		return (-1);
	}

	if (tiovcount == 0 || tiovcount > T_IOV_MAX) {
		t_errno = TBADDATA;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_sndvudata, 1, fd);
		return (-1);
	}

	if (tiptr->ti_state != T_IDLE) {
		t_errno = TOUTSTATE;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_sndvudata, 1, fd);
		return (-1);
	}

	nbytes = _t_bytecount_upto_intmax(tiov, tiovcount);

	if ((nbytes == 0) &&
	    !(tiptr->ti_prov_flag & (SENDZERO|OLD_SENDZERO))) {
		t_errno = TBADDATA;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_sndvudata, 1, fd);
		return (-1);
	}

	if ((tiptr->ti_maxpsz > 0) && (nbytes > (uint32_t)tiptr->ti_maxpsz)) {
		t_errno = TBADDATA;
		sv_errno = errno;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_sndvudata, 1, fd);
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
		trace2(TR_t_sndvudata, 1, fd);
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

	dataptr = NULL;
	if (nbytes != 0) {
		if ((dataptr = malloc((size_t)nbytes)) == NULL) {
			t_errno = TSYSERR;
			goto err_out;
		}
		_t_gather(dataptr, tiov, tiovcount);
	}
	databuf.buf = dataptr;
	databuf.len = nbytes;
	databuf.maxlen = nbytes;
	/*
	 * Calls to send data (write or putmsg) can potentially
	 * block, for MT case, we drop the lock and enable signals here
	 * and acquire it back
	 */
	MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
	if (putmsg(fd, &ctlbuf, &databuf, 0) < 0) {
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
			"t_sndvudata: invalid state event T_SNDUDATA");
	if (didalloc)
		free(ctlbuf.buf);
	else
		tiptr->ti_ctlbuf = ctlbuf.buf;
	if (dataptr != NULL)
		free(dataptr);
	MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
	trace2(TR_t_sndvudata, 0, fd);
	return (0);
err_out:
	sv_errno = errno;
	if (didalloc)
		free(ctlbuf.buf);
	else
		tiptr->ti_ctlbuf = ctlbuf.buf;
	if (dataptr != NULL)
		free(dataptr);
	MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
	trace2(TR_t_sndvudata, 1, fd);
	errno = sv_errno;
	return (-1);
}
