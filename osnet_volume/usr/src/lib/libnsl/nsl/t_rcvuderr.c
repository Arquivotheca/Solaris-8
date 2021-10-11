/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1993-96 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/


#pragma ident	"@(#)t_rcvuderr.c	1.24	98/04/19 SMI"	/* SVr4.0 1.5 */

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
#include <syslog.h>
#include <assert.h>
#include "timt.h"
#include "tx.h"

int
_tx_rcvuderr(int fd, struct t_uderr *uderr, int api_semantics)
{
	struct strbuf ctlbuf, databuf;
	int flg;
	int retval;
	union T_primitives *pptr;
	struct _ti_user *tiptr;
	sigset_t mask;
	int sv_errno;
	int didalloc;
	int use_lookbufs = 0;


	trace2(TR_t_rcvuderr, 0, fd);
	if ((tiptr = _t_checkfd(fd, 0, api_semantics)) == NULL) {
		sv_errno = errno;
		trace2(TR_t_rcvuderr, 1, fd);
		errno = sv_errno;
		return (-1);
	}
	MUTEX_LOCK_THRMASK(&tiptr->ti_lock, mask);

	if (tiptr->ti_servtype != T_CLTS) {
		t_errno = TNOTSUPPORT;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_rcvuderr, 1, fd);
		return (-1);
	}
	/*
	 * is there a unitdata error indication in look buffer
	 */
	if (tiptr->ti_lookcnt > 0) {
		ctlbuf.len = tiptr->ti_lookbufs.tl_lookclen;
		ctlbuf.buf = tiptr->ti_lookbufs.tl_lookcbuf;
		/* Note: cltbuf.maxlen not used in this case */

		assert(((union T_primitives *)ctlbuf.buf)->type
			== T_UDERROR_IND);

		databuf.maxlen = 0;
		databuf.len = 0;
		databuf.buf = NULL;

		use_lookbufs = 1;

	} else {
		if ((retval = _t_look_locked(fd, tiptr, 0,
		    api_semantics)) < 0) {
			sv_errno = errno;
			MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
			trace2(TR_t_rcvuderr, 1, fd);
			errno = sv_errno;
			return (-1);
		}
		if (retval != T_UDERR) {
			t_errno = TNOUDERR;
			MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
			trace2(TR_t_rcvuderr, 1, fd);
			return (-1);
		}

		/*
		 * Acquire ctlbuf for use in sending/receiving control part
		 * of the message.
		 */
		if (_t_acquire_ctlbuf(tiptr, &ctlbuf, &didalloc) < 0) {
			sv_errno = errno;
			MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
			trace2(TR_t_rcvuderr, 1, fd);
			errno = sv_errno;
			return (-1);
		}

		databuf.maxlen = 0;
		databuf.len = 0;
		databuf.buf = NULL;

		flg = 0;

		/*
		 * Since we already verified that a unitdata error
		 * indication is pending, we assume that this getmsg()
		 * will not block indefinitely.
		 */
		if ((retval = getmsg(fd, &ctlbuf, &databuf, &flg)) < 0) {

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

	}

	pptr = (union T_primitives *)ctlbuf.buf;

	if ((ctlbuf.len < (int)sizeof (struct T_uderror_ind)) ||
	    (pptr->type != T_UDERROR_IND)) {
		t_errno = TSYSERR;
		errno = EPROTO;
		goto err_out;
	}

	if (uderr) {
		if (_T_IS_TLI(api_semantics) || uderr->addr.maxlen > 0) {
			if (TLEN_GT_NLEN(pptr->uderror_ind.DEST_length,
			    uderr->addr.maxlen)) {
				t_errno = TBUFOVFLW;
				goto err_out;
			}
			(void) memcpy(uderr->addr.buf, ctlbuf.buf +
			    pptr->uderror_ind.DEST_offset,
			    (size_t)pptr->uderror_ind.DEST_length);
			uderr->addr.len =
			    (unsigned int)pptr->uderror_ind.DEST_length;
		}
		if (_T_IS_TLI(api_semantics) || uderr->addr.maxlen > 0) {
			if (TLEN_GT_NLEN(pptr->uderror_ind.OPT_length,
			    uderr->opt.maxlen)) {
				t_errno = TBUFOVFLW;
				goto err_out;
			}
			(void) memcpy(uderr->opt.buf, ctlbuf.buf +
			    pptr->uderror_ind.OPT_offset,
			    (size_t)pptr->uderror_ind.OPT_length);
			uderr->opt.len =
			    (unsigned int)pptr->uderror_ind.OPT_length;
		}
		uderr->error = pptr->uderror_ind.ERROR_type;
	}

	_T_TX_NEXTSTATE(T_RCVUDERR, tiptr,
			"t_rcvuderr: invalid state event T_RCVUDERR");
	if (use_lookbufs)
		_t_free_looklist_head(tiptr);
	else {
		if (didalloc)
			free(ctlbuf.buf);
		else
			tiptr->ti_ctlbuf = ctlbuf.buf;
	}
	MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
	trace2(TR_t_rcvuderr, 1, fd);
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
	}
	MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
	trace2(TR_t_rcvuderr, 1, fd);
	errno = sv_errno;
	return (-1);
}
