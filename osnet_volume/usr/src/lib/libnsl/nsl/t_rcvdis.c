/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1993-96 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/


#pragma ident	"@(#)t_rcvdis.c	1.25	98/04/19 SMI"	/* SVr4.0 1.10 */

#include <rpc/trace.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
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
_tx_rcvdis(int fd, struct t_discon *discon, int api_semantics)
{
	struct strbuf ctlbuf;
	struct strbuf databuf;
	int retval;
	union T_primitives *pptr;
	struct _ti_user *tiptr;
	sigset_t mask;
	int sv_errno;
	int flg = 0;
	int didalloc, didralloc;
	int use_lookbufs = 0;

	trace2(TR_t_rcvdis, 0, fd);
	if ((tiptr = _t_checkfd(fd, 0, api_semantics)) == NULL) {
		sv_errno = errno;
		trace2(TR_t_rcvdis, 1, fd);
		errno = sv_errno;
		return (-1);
	}

	/*
	 * Acquire per thread lock.
	 * Note: Lock is held across most of this routine
	 * including the blocking getmsg() call. This is fine
	 * because it is first verfied that an event is pending
	 */
	MUTEX_LOCK_THRMASK(&tiptr->ti_lock, mask);

	if (tiptr->ti_servtype == T_CLTS) {
		t_errno = TNOTSUPPORT;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_rcvdis, 1, fd);
		return (-1);
	}

	if (_T_IS_XTI(api_semantics)) {
		/*
		 * User level state verification only done for XTI
		 * because doing for TLI may break existing applications
		 */
		if (! (tiptr->ti_state == T_DATAXFER ||
		    tiptr->ti_state == T_OUTCON ||
		    tiptr->ti_state == T_OUTREL ||
		    tiptr->ti_state == T_INREL ||
		    (tiptr->ti_state == T_INCON && tiptr->ti_ocnt > 0))) {
			t_errno = TOUTSTATE;
			MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
			trace2(TR_t_rcvdis, 1, fd);
			return (-1);
		}
	}
	/*
	 * Handle likely scenario as special case:
	 * Is there a discon in look buffer as the first
	 * event in the lookbuffer, is so just get it.
	 */
	if ((tiptr->ti_lookcnt > 0) &&
	    (*((t_scalar_t *)tiptr->ti_lookbufs.tl_lookcbuf) == T_DISCON_IND)) {
		/*
		 * The T_DISCON_IND is already in the look buffer
		 */
		ctlbuf.len = tiptr->ti_lookbufs.tl_lookclen;
		ctlbuf.buf = tiptr->ti_lookbufs.tl_lookcbuf;
		/* Note: ctlbuf.maxlen not used in this case */

		databuf.len = tiptr->ti_lookbufs.tl_lookdlen;
		databuf.buf = tiptr->ti_lookbufs.tl_lookdbuf;
		/* Note databuf.maxlen not used in this case */

		use_lookbufs = 1;

	} else {

		if ((retval = _t_look_locked(fd, tiptr, 0,
		    api_semantics)) < 0) {
			sv_errno = errno;
			MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
			trace2(TR_t_rcvdis, 1, fd);
			errno = sv_errno;
			return (-1);
		}

		if (retval != T_DISCONNECT) {
			t_errno = TNODIS;
			MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
			trace2(TR_t_rcvdis, 1, fd);
			return (-1);
		}

		/*
		 * get disconnect off read queue.
		 * use ctl and rcv buffers
		 *
		 * Acquire ctlbuf for use in sending/receiving control part
		 * of the message.
		 */
		if (_t_acquire_ctlbuf(tiptr, &ctlbuf, &didalloc) < 0) {
			sv_errno = errno;
			MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
			trace2(TR_t_rcvdis, 1, fd);
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
			trace2(TR_t_rcvdis, 1, fd);
			errno = sv_errno;
			return (-1);
		}

		/*
		 * Since we already verified that a disconnect event
		 * is present, we assume that this getmsg() cannot
		 * block indefinitely
		 */
		do {
			retval = getmsg(fd, &ctlbuf, &databuf, &flg);
		} while (retval < 0 && errno == EINTR);

		if (retval  < 0) {
			t_errno = TSYSERR;
			goto err_out;
		}
		if (databuf.len == -1) databuf.len = 0;

		/*
		 * did I get entire message?
		 */
		if (retval > 0) {
			t_errno = TSYSERR;
			errno = EIO;
			goto err_out;
		}
	}


	pptr = (union T_primitives *)ctlbuf.buf;

	if ((ctlbuf.len < (int)sizeof (struct T_discon_ind)) ||
	    (pptr->type != T_DISCON_IND)) {
		t_errno = TSYSERR;
		errno = EPROTO;
		goto err_out;
	}

	/*
	 * clear more and expedited flags
	 */
	tiptr->ti_flags &= ~(MORE | EXPEDITED);

	if (tiptr->ti_ocnt <= 0) {
		_T_TX_NEXTSTATE(T_RCVDIS1, tiptr,
				"t_rcvdis: invalid state event T_RCVDIS1");
	} else {
		if (tiptr->ti_ocnt == 1) {
			_T_TX_NEXTSTATE(T_RCVDIS2, tiptr,
				"t_rcvdis: invalid state event T_RCVDIS2");
		} else {
			_T_TX_NEXTSTATE(T_RCVDIS3, tiptr,
				"t_rcvdis: invalid state event T_RCVDIS3");
		}
		tiptr->ti_ocnt--;
		tiptr->ti_flags &= ~TX_TQFULL_NOTIFIED;
	}

	if (discon != NULL) {
		if (_T_IS_TLI(api_semantics) || discon->udata.maxlen > 0) {
			if (databuf.len > (int)discon->udata.maxlen) {
				t_errno = TBUFOVFLW;
				goto err_out;
			}
			(void) memcpy(discon->udata.buf, databuf.buf,
			    (size_t)databuf.len);
			discon->udata.len = databuf.len;
		}
		discon->reason = pptr->discon_ind.DISCON_reason;
		discon->sequence = pptr->discon_ind.SEQ_number;
	}
	if (use_lookbufs)
		_t_free_looklist_head(tiptr);
	else {
		if (didalloc)
			free(ctlbuf.buf);
		else
			tiptr->ti_ctlbuf = ctlbuf.buf;
		if (didralloc)
			free(databuf.buf);
		else
			tiptr->ti_rcvbuf = databuf.buf;
	}
	MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
	trace2(TR_t_rcvdis, 1, fd);
	return (0);

err_out:
	sv_errno = errno;

	if (use_lookbufs)
		_t_free_looklist_head(tiptr);
	else {
		if (didalloc)
			free(ctlbuf.buf);
		else
			tiptr->ti_ctlbuf = ctlbuf.buf;
		if (didralloc)
			free(databuf.buf);
		else
			tiptr->ti_rcvbuf = databuf.buf;
	}
	MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
	trace2(TR_t_rcvdis, 1, fd);
	errno = sv_errno;
	return (-1);
}
