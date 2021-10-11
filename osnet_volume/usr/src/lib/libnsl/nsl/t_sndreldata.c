/*	Copyright (c) 1993-96 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/


#pragma ident	"@(#)t_sndreldata.c	1.2	98/04/19 SMI"

/*
 * t_sndrel.c and t_sndreldata.c are very similar and contain common code.
 * Any changes to either of them should be reviewed to see whether they
 * are applicable to the other file.
 */
#include <rpc/trace.h>
#include <rpc/trace.h>
#include <errno.h>
#include <stropts.h>
#include <sys/stream.h>
#define	_SUN_TPI_VERSION 2
#include <sys/tihdr.h>
#include <sys/timod.h>
#include <xti.h>
#include <assert.h>
#include "timt.h"
#include "tx.h"

int
_tx_sndreldata(int fd, struct t_discon *discon, int api_semantics)
{
	struct T_ordrel_req orreq;
	struct strbuf ctlbuf;
	struct _ti_user *tiptr;
	sigset_t mask;
	int sv_errno;

	trace2(TR_t_sndreldata, 0, fd);
	assert(api_semantics == TX_XTI_XNS5_API);
	if ((tiptr = _t_checkfd(fd, 0, api_semantics)) == NULL) {
		sv_errno = errno;
		trace2(TR_t_sndreldata, 1, fd);
		errno = sv_errno;
		return (-1);
	}
	MUTEX_LOCK_THRMASK(&tiptr->ti_lock, mask);

	if (tiptr->ti_servtype != T_COTS_ORD) {
		t_errno = TNOTSUPPORT;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_sndreldata, 1, fd);
		return (-1);
	}

	if (! (tiptr->ti_state == T_DATAXFER ||
	    tiptr->ti_state == T_INREL)) {
		t_errno = TOUTSTATE;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_sndreldata, 1, fd);
		return (-1);
	}

	if (_t_look_locked(fd, tiptr, 0,
	    api_semantics) == T_DISCONNECT) {
		t_errno = TLOOK;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_sndreldata, 1, fd);
		return (-1);
	}

	/*
	 * Someday there could be transport providers that support T_ORDRELDATA
	 * Until that happens, this function returns TBADDATA if user data
	 * was specified. If no user data is specified, this function
	 * behaves as t_sndrel()
	 * Note: Currently only mOSI ("minimal OSI") provider is specified
	 * to use T_ORDRELDATA so probability of needing it is minimal.
	 */

	if (discon && discon->udata.len) {
		t_errno = TBADDATA;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		return (-1);
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
		trace2(TR_t_sndreldata, 1, fd);
		errno = sv_errno;
		return (-1);
	}
	MUTEX_LOCK_THRMASK(&tiptr->ti_lock, mask);
	_T_TX_NEXTSTATE(T_SNDREL, tiptr,
			"t_sndreldata: invalid state on event T_SNDREL");
	MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
	trace2(TR_t_sndreldata, 1, fd);
	return (0);
}
