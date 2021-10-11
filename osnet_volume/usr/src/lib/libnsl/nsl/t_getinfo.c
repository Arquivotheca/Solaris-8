/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1993-96 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/


#pragma ident	"@(#)t_getinfo.c	1.23	98/04/19 SMI"	/* SVr4.0 1.5 */

#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <rpc/trace.h>
#include <sys/stream.h>
#define	_SUN_TPI_VERSION 2
#include <sys/tihdr.h>
#include <sys/timod.h>
#include <xti.h>
#include <signal.h>
#include <stropts.h>
#include "timt.h"
#include "tx.h"

int
_tx_getinfo(int fd, struct t_info *info, int api_semantics)
{
	struct T_info_req *inforeqp;
	struct T_info_ack *infoackp;
	int retlen;
	sigset_t mask;
	struct _ti_user *tiptr;
	int retval, sv_errno, didalloc;
	struct strbuf ctlbuf;

	trace2(TR_t_getinfo, 0, fd);
	if ((tiptr = _t_checkfd(fd, 0, api_semantics)) == 0) {
		sv_errno = errno;
		trace2(TR_t_getinfo, 1, fd);
		errno = sv_errno;
		return (-1);
	}
	MUTEX_LOCK_THRMASK(&tiptr->ti_lock, mask);

	/*
	 * Acquire buffer for use in sending/receiving the message.
	 * Note: assumes (correctly) that ti_ctlsize is large enough
	 * to hold sizeof (struct T_info_req/ack)
	 */
	if (_t_acquire_ctlbuf(tiptr, &ctlbuf, &didalloc) < 0) {
		sv_errno = errno;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_getinfo, 1, fd);
		errno = sv_errno;
		return (-1);
	}

	inforeqp =  (struct T_info_req *)ctlbuf.buf;
	inforeqp->PRIM_type = T_INFO_REQ;

	do {
		retval = _t_do_ioctl(fd, ctlbuf.buf,
			(int)sizeof (struct T_info_req), TI_GETINFO, &retlen);
	} while (retval < 0 && errno == EINTR);

	if (retval < 0)
		goto err_out;

	if (retlen != (int)sizeof (struct T_info_ack)) {
		t_errno = TSYSERR;
		errno = EIO;
		goto err_out;
	}

	infoackp = (struct T_info_ack *)ctlbuf.buf;

	info->addr = infoackp->ADDR_size;
	info->options = infoackp->OPT_size;
	info->tsdu = infoackp->TSDU_size;
	info->etsdu = infoackp->ETSDU_size;
	info->connect = infoackp->CDATA_size;
	info->discon = infoackp->DDATA_size;
	info->servtype = infoackp->SERV_type;

	if (_T_IS_XTI(api_semantics)) {
		/* XTI ONLY - TLI t_info struct does not have "flags" */
		info->flags = 0;
		if (infoackp->PROVIDER_flag & (SENDZERO|OLD_SENDZERO))
			info->flags |= T_SENDZERO;
	}
	if (didalloc)
		free(ctlbuf.buf);
	else
		tiptr->ti_ctlbuf = ctlbuf.buf;
	MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
	trace2(TR_t_getinfo, 1, fd);
	return (0);

err_out:
	sv_errno = errno;
	if (didalloc)
		free(ctlbuf.buf);
	else
		tiptr->ti_ctlbuf = ctlbuf.buf;
	MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
	trace2(TR_t_getinfo, 1, fd);
	errno = sv_errno;
	return (-1);
}
