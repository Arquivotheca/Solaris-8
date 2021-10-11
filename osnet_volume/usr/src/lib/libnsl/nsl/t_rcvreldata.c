/*	Copyright (c) 1993-96 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/


#pragma ident	"@(#)t_rcvreldata.c	1.2	98/04/19 SMI"

/*
 * t_rcvrel.c and t_rcvreldata.c are very similar and contain common code.
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
#include <signal.h>
#include <syslog.h>
#include <assert.h>
#include "timt.h"
#include "tx.h"

/* ARGSUSED */
int
_tx_rcvreldata(int fd, struct t_discon *discon, int api_semantics)
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


	trace2(TR_t_rcvreldata, 0, fd);
	assert(api_semantics == TX_XTI_XNS5_API);
	if ((tiptr = _t_checkfd(fd, 0, api_semantics)) == 0) {
		sv_errno = errno;
		trace2(TR_t_rcvreldata, 1, fd);
		errno = sv_errno;
		return (-1);
	}
	MUTEX_LOCK_THRMASK(&tiptr->ti_lock, mask);

	if (tiptr->ti_servtype != T_COTS_ORD) {
		t_errno = TNOTSUPPORT;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_rcvreldata, 1, fd);
		return (-1);
	}

	if (! (tiptr->ti_state == T_DATAXFER ||
	    tiptr->ti_state == T_OUTREL)) {
		t_errno = TOUTSTATE;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_rcvreldata, 1, fd);
		return (-1);
	}

	if ((retval = _t_look_locked(fd, tiptr, 0, api_semantics)) < 0) {
		sv_errno = errno;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_rcvreldata, 1, fd);
		errno = sv_errno;
		return (-1);
	}

	if (retval == T_DISCONNECT) {
		/*
		 * This ensures preference to T_DISCON_IND which is
		 * the design model for TPI
		 */
		t_errno = TLOOK;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_rcvreldata, 1, fd);
		return (-1);
	}

	/*
	 * Someday there could be transport providers that support T_ORDRELDATA
	 * Until then this function behaves the same as t_rcvrel()
	 * Note: Currently only mOSI ("minimal OSI") provider is specified
	 * to use T_ORDRELDATA so probability of needing it is minimal.
	 */

	if ((tiptr->ti_lookcnt > 0) &&
	    (*((t_scalar_t *)tiptr->ti_lookbufs.tl_lookcbuf) == T_ORDREL_IND)) {
		/*
		 * Current look buffer event is T_ORDREL_IND.
		 * Remove it from look buffer event list.
		 */
		_t_free_looklist_head(tiptr);
		_T_TX_NEXTSTATE(T_RCVREL, tiptr,
			"t_rcvreldata: invalid state event T_RCVREL");
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_rcvreldata, 1, fd);
		return (0);
	} else {
		if (retval != T_ORDREL) {
			t_errno = TNOREL;
			MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
			trace2(TR_t_rcvreldata, 1, fd);
			return (-1);
		}
	}

	/*
	 * get ordrel off read queue.
	 * use ctl and rcv buffers
	 *
	 * Acquire ctlbuf for use in sending/receiving control part
	 * of the message.
	 */
	if (_t_acquire_ctlbuf(tiptr, &ctlbuf, &didalloc) < 0) {
		sv_errno = errno;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_rcvreldata, 1, fd);
		errno = sv_errno;
		return (-1);
	}

	/*
	 * Acquire databuf for use in sending/receiving data part
	 */
	if (_t_acquire_databuf(tiptr, &databuf, &didralloc) < 0) {
		sv_errno = errno;
		if (didalloc)
			free(ctlbuf.buf);
		else
			tiptr->ti_ctlbuf = ctlbuf.buf;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_rcvreldata, 1, fd);
		errno = sv_errno;
		return (-1);
	}

	/*
	 * Since we have verified above that an orderly release event
	 * is pending on this endpoint, we assume that this getmsg()
	 * cannot block forever.
	 */
	do {
		retval = getmsg(fd, &ctlbuf, &databuf, &flg);
	} while (retval < 0 && errno == EINTR);

	if (retval < 0) {
		t_errno = TSYSERR;
		goto err_out;
	}

	/*
	 * did I get entire message?
	 */
	if (retval > 0) {
		t_errno = TSYSERR;
		errno = EIO;
		goto err_out;
	}
	pptr = (union T_primitives *)ctlbuf.buf;

	if (ctlbuf.len < (int)sizeof (struct T_ordrel_ind)) {
		t_errno = TSYSERR;
		errno = EPROTO;
		goto err_out;
	}
	if (pptr->type != T_ORDREL_IND) {
		if (pptr->type == T_DISCON_IND) {
			/*
			 * T_DISCON_IND gets priority following
			 * TPI design philosphy.
			 *
			 * Add it to the events in the "look buffer"
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
		} else {
			t_errno = TSYSERR;
			errno = EPROTO;
			goto err_out;
		}
	}

	_T_TX_NEXTSTATE(T_RCVREL, tiptr,
		"t_rcvreldata: invalid state event T_RCVREL");

	if (didalloc)
		free(ctlbuf.buf);
	else
		tiptr->ti_ctlbuf = ctlbuf.buf;
	if (didralloc)
		free(databuf.buf);
	else
		tiptr->ti_rcvbuf = databuf.buf;
	MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
	trace2(TR_t_rcvreldata, 1, fd);
	return (0);

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
	trace2(TR_t_rcvreldata, 1, fd);
	errno = sv_errno;
	return (-1);
}
