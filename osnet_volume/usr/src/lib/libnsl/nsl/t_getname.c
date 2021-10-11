/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1993-96 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/


#pragma ident	"@(#)t_getname.c	1.23	98/04/19 SMI"
		/* SVr4.0 1.1.1.1 */

#include <errno.h>
#include <rpc/trace.h>
#include <unistd.h>
#include <string.h>
#include <stropts.h>
#include <sys/stream.h>
#include <xti.h>
#define	_SUN_TPI_VERSION 2
#include <sys/tihdr.h>
#include <sys/timod.h>
#include <assert.h>
#include <signal.h>
#include "timt.h"
#include "tx.h"

static int __tx_tlitpi_getprotaddr_locked(struct _ti_user *tiptr,
	struct t_bind *boundaddr, struct t_bind *peer);


static int __tx_getname_locked(int fd, struct netbuf *name, int type);

int
_tx_getname(int fd, struct netbuf *name, int type, int api_semantics)
{
	sigset_t mask;
	struct _ti_user *tiptr;
	int retval, sv_errno;

	trace3(TR_t_getname, 0, fd, type);
	assert(_T_IS_TLI(api_semantics)); /* TLI only interface */
	if (!name || ((type != LOCALNAME) && (type != REMOTENAME))) {
		trace3(TR_t_getname, 1, fd, type);
		errno = EINVAL;
		return (-1);
	}

	if ((tiptr = _t_checkfd(fd, 0, api_semantics)) == 0) {
		sv_errno = errno;
		trace3(TR_t_getname, 1, fd, type);
		errno = sv_errno;
		return (-1);
	}
	MUTEX_LOCK_THRMASK(&tiptr->ti_lock, mask);

	retval = __tx_getname_locked(fd, name, type);

	if (retval < 0) {
		sv_errno = errno;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace3(TR_t_getname, 1, fd, type);
		errno = sv_errno;
		return (-1);
	}

	MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);

	trace3(TR_t_getname, 1, fd, type);
	return (0);
}


static int
__tx_getname_locked(int fd, struct netbuf *name, int type)
{
	int retval;

	do {
		retval = _ioctl(fd,
		    (type == LOCALNAME) ? TI_GETMYNAME : TI_GETPEERNAME, name);
	} while (retval < 0 && errno == EINTR);

	if (retval < 0) {
		t_errno = TSYSERR;
		return (-1);
	}
	return (0);
}



int
_tx_getprotaddr(
	int fd,
	struct t_bind *boundaddr,
	struct t_bind *peeraddr,
	int api_semantics)
{
	sigset_t mask;
	struct _ti_user *tiptr;
	int retval, sv_errno;
	struct T_addr_req *addreqp;
	struct T_addr_ack *addrackp;
	int didalloc;
	struct strbuf ctlbuf;
	int retlen;

	trace2(TR_t_getprotaddr, 0, fd);

	if ((tiptr = _t_checkfd(fd, 0, api_semantics)) == 0) {
		sv_errno = errno;
		trace2(TR_t_getprotaddr, 1, fd);
		errno = sv_errno;
		return (-1);
	}

	MUTEX_LOCK_THRMASK(&tiptr->ti_lock, mask);

	if ((tiptr->ti_prov_flag & XPG4_1) == 0) {
		/*
		 * Provider does not support XTI inspired TPI so we
		 * try to do operation assuming TLI inspired TPI
		 */
		retval = __tx_tlitpi_getprotaddr_locked(tiptr, boundaddr,
					peeraddr);
		sv_errno = errno;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		errno = sv_errno;
		return (retval);
	}

	/*
	 * Acquire buffer for use in sending/receiving the message.
	 * Note: assumes (correctly) that ti_ctlsize is large enough
	 * to hold sizeof (struct T_addr_req/ack)
	 */
	if (_t_acquire_ctlbuf(tiptr, &ctlbuf, &didalloc) < 0) {
		sv_errno = errno;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_bind, 1, fd);
		errno = sv_errno;
		return (-1);
	}

	addreqp = (struct T_addr_req *)ctlbuf.buf;
	addreqp->PRIM_type = T_ADDR_REQ;

	do {
		retval = _t_do_ioctl(fd, ctlbuf.buf,
			(int)sizeof (struct T_addr_req), TI_GETADDRS, &retlen);
	} while (retval < 0 && errno == EINTR);

	if (retval < 0) {
		sv_errno = errno;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_getprotaddr, 1, fd);
		errno = sv_errno;
		return (-1);
	}
	MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);

	if (retlen < (int)sizeof (struct T_addr_ack)) {
		t_errno = TSYSERR;
		trace2(TR_t_getprotaddr, 1, fd);
		errno = EIO;
		return (-1);
	}

	addrackp = (struct T_addr_ack *)ctlbuf.buf;

	/*
	 * We assume null parameters are OK and not errors
	 */
	if (boundaddr != NULL && boundaddr->addr.maxlen > 0) {
		if (TLEN_GT_NLEN(addrackp->LOCADDR_length,
		    boundaddr->addr.maxlen)) {
			t_errno = TBUFOVFLW;
			trace2(TR_t_getprotaddr, 1, fd);
			return (-1);
		}
		boundaddr->addr.len = addrackp->LOCADDR_length;
		memcpy(boundaddr->addr.buf,
		    ctlbuf.buf + addrackp->LOCADDR_offset,
		    (size_t)addrackp->LOCADDR_length);
	}

	/*
	 * Note: In states where there is no remote end of the connection
	 * the T_ADDR_REQ primitive does not return a remote address. However,
	 * in protcols such as TCP, the transport connection is established
	 * before the TLI/XTI level association is established. Therefore,
	 * in state T_OUTCON, the transport may return a remote address where
	 * TLI/XTI level thinks there is no remote end and therefore
	 * no remote address should be returned. We therefore do not look at
	 * address returned by transport provider in T_OUTCON state.
	 * Tested by XTI test suite.
	 */
	if (tiptr->ti_state != T_OUTCON &&
	    peeraddr != NULL && peeraddr->addr.maxlen > 0) {
		if (TLEN_GT_NLEN(addrackp->REMADDR_length,
		    peeraddr->addr.maxlen)) {
			t_errno = TBUFOVFLW;
			trace2(TR_t_getprotaddr, 1, fd);
			return (-1);
		}
		peeraddr->addr.len = addrackp->REMADDR_length;
		memcpy(peeraddr->addr.buf,
		    ctlbuf.buf + addrackp->REMADDR_offset,
		    (size_t)addrackp->REMADDR_length);
	}

	trace2(TR_t_getprotaddr, 1, fd);
	return (0);
}

static int
__tx_tlitpi_getprotaddr_locked(
	struct _ti_user *tiptr,
	struct t_bind *boundaddr,
	struct t_bind *peeraddr)
{
	if (boundaddr) {
		boundaddr->addr.len = 0;
		if (tiptr->ti_state >= TS_IDLE) {
			/*
			 * assume bound endpoint .
			 * Note: TI_GETMYNAME can return
			 * a finite length all zeroes address for unbound
			 * endpoint so we avoid relying on it for bound
			 * endpoints for XTI t_getprotaddr() semantics.
			 */
			if (__tx_getname_locked(tiptr->ti_fd, &boundaddr->addr,
			    LOCALNAME) < 0)
				return (-1);

		}
	}
	if (peeraddr) {

		peeraddr->addr.len = 0;

		if (tiptr->ti_state >= TS_DATA_XFER) {
			/*
			 * assume connected endpoint.
			 * The TI_GETPEERNAME call can fail with error
			 * if endpoint is not connected so we don't call it
			 * for XTI t_getprotaddr() semantics
			 */
			if (__tx_getname_locked(tiptr->ti_fd, &peeraddr->addr,
			    REMOTENAME) < 0)
				return (-1);
		}
	}
	return (0);
}
