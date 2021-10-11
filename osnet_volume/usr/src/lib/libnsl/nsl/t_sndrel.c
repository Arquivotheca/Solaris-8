/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1993-96 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/


#pragma ident	"@(#)t_sndrel.c	1.22	98/04/19 SMI"	/* SVr4.0 1.4 */

/*
 * t_sndrel.c and t_sndreldata.c are very similar and contain common code.
 * Any changes to either of them should be reviewed to see whether they
 * are applicable to the other file.
 */
#include <rpc/trace.h>
#include <errno.h>
#include <stropts.h>
#include <sys/stream.h>
#define	_SUN_TPI_VERSION 2
#include <sys/tihdr.h>
#include <sys/timod.h>
#include <xti.h>
#include "timt.h"
#include "tx.h"

int
_tx_sndrel(int fd, int api_semantics)
{
	struct T_ordrel_req orreq;
	struct strbuf ctlbuf;
	struct _ti_user *tiptr;
	sigset_t mask;
	int sv_errno;

	trace2(TR_t_sndrel, 0, fd);
	if ((tiptr = _t_checkfd(fd, 0, api_semantics)) == NULL) {
		sv_errno = errno;
		trace2(TR_t_sndrel, 1, fd);
		errno = sv_errno;
		return (-1);
	}
	MUTEX_LOCK_THRMASK(&tiptr->ti_lock, mask);

	if (tiptr->ti_servtype != T_COTS_ORD) {
		t_errno = TNOTSUPPORT;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_sndrel, 1, fd);
		return (-1);
	}

	if (_T_IS_XTI(api_semantics)) {
		/*
		 * User level state verification only done for XTI
		 * because doing for TLI may break existing applications
		 */
		if (! (tiptr->ti_state == T_DATAXFER ||
		    tiptr->ti_state == T_INREL)) {
			t_errno = TOUTSTATE;
			MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
			trace2(TR_t_sndrel, 1, fd);
			return (-1);
		}

		if (_t_look_locked(fd, tiptr, 0,
		    api_semantics) == T_DISCONNECT) {
			t_errno = TLOOK;
			MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
			trace2(TR_t_sndrel, 1, fd);
			return (-1);
		}

	}

	orreq.PRIM_type = T_ORDREL_REQ;
	ctlbuf.maxlen = (int)sizeof (struct T_ordrel_req);
	ctlbuf.len = (int)sizeof (struct T_ordrel_req);
	ctlbuf.buf = (caddr_t)&orreq;

	/*
	 * Calls to send data (write or putmsg) can potentially
	 * block, for MT case, we drop the lock and enable signals here
	 * and acquire it back
	 */
	MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
	if (putmsg(fd, &ctlbuf, NULL, 0) < 0) {
		sv_errno = errno;

		if (errno == EAGAIN)
			t_errno = TFLOW;
		else
			t_errno = TSYSERR;
		trace2(TR_t_sndrel, 1, fd);
		errno = sv_errno;
		return (-1);
	}
	MUTEX_LOCK_THRMASK(&tiptr->ti_lock, mask);
	_T_TX_NEXTSTATE(T_SNDREL, tiptr,
				"t_sndrel: invalid state on event T_SNDREL");
	MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
	trace2(TR_t_sndrel, 1, fd);
	return (0);
}
