/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1993-96 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/


#pragma ident	"@(#)t_open.c	1.27	98/04/19 SMI"	/* SVr4.0 1.5.3.3 */

#include <fcntl.h>
#include <rpc/trace.h>
#include <unistd.h>
#include <errno.h>
#include <stropts.h>
#define	_SUN_TPI_VERSION 2
#include <sys/timod.h>
#include <xti.h>
#include <signal.h>
#include <syslog.h>
#include "timt.h"
#include "tx.h"

/*
 * If _tx_open is called for transport that doesn't understand T_CAPABILITY_REQ
 * TPI message, call to _t_create may fail the first time it is called with
 * given transport (in the rare case when transport shuts down the stream with
 * M_ERROR in reply to unknown T_CAPABILITY_REQ). In this case we may reopen the
 * stream again since timod will emulate T_CAPABILITY_REQ behaviour.
 *
 * _t_create sends T_CAPABILITY_REQ through TI_CAPABILITY ioctl.
 */

int
_tx_open(const char *path, int flags, struct t_info *info, int api_semantics)
{
	int retval, fd, sv_errno;
	int sv_terrno;
	int sv_errno_global;
	struct _ti_user *tiptr;
	sigset_t mask;
	int t_create_first_attempt = 1;
	int ticap_ioctl_failed = 0;

	trace2(TR_t_open, 0, flags);
	if (!(flags & O_RDWR)) {
		t_errno = TBADFLAG;
		trace2(TR_t_open, 1, flags);
		return (-1);
	}

	sv_errno_global = errno;
	sv_terrno = t_errno;

retry:
	if ((fd = open(path, flags)) < 0) {
		sv_errno = errno;

		trace2(TR_t_open, 1, flags);
		errno = sv_errno;
		t_errno = TSYSERR;
		if (_T_IS_XTI(api_semantics) && errno == ENOENT)
			/* XTI only */
			t_errno = TBADNAME;
		return (-1);
	}
	/*
	 * is module already pushed
	 */
	do {
		retval = _ioctl(fd, I_FIND, "timod");
	} while (retval < 0 && errno == EINTR);

	if (retval < 0) {
		sv_errno = errno;

		t_errno = TSYSERR;
		(void) close(fd);
		trace2(TR_t_open, 1, flags);
		errno = sv_errno;
		return (-1);
	}

	if (retval == 0) {
		/*
		 * "timod" not already on stream, then push it
		 */
		do {
			/*
			 * Assumes (correctly) that I_PUSH  is
			 * atomic w.r.t signals (EINTR error)
			 */
			retval = _ioctl(fd, I_PUSH, "timod");
		} while (retval < 0 && errno == EINTR);

		if (retval < 0) {
			int sv_errno = errno;

			t_errno = TSYSERR;
			(void) close(fd);
			trace2(TR_t_open, 1, flags);
			errno = sv_errno;
			return (-1);
		}
	}

	MUTEX_LOCK_PROCMASK(&_ti_userlock, mask);
	/*
	 * Call to _t_create may fail either because transport doesn't
	 * understand T_CAPABILITY_REQ or for some other reason. It is nearly
	 * impossible to distinguish between these cases so it is implicitly
	 * assumed that it is always save to close and reopen the same stream
	 * and that open/close doesn't have side effects. _t_create may fail
	 * only once if its' failure is caused by unimplemented
	 * T_CAPABILITY_REQ.
	 */
	tiptr = _t_create(fd, info, api_semantics, &ticap_ioctl_failed);
	if (tiptr == NULL) {
		/*
		 * If _t_create failed due to fail of ti_capability_req we may
		 * try to reopen the stream in the hope that timod will emulate
		 * TI_CAPABILITY and it will succeed when called again.
		 */
		if (t_create_first_attempt == 1 && ticap_ioctl_failed == 1) {
			t_create_first_attempt = 0;
			(void) close(fd);
			errno = sv_errno_global;
			t_errno = sv_terrno;
			MUTEX_UNLOCK_PROCMASK(&_ti_userlock, mask);
			goto retry;
		} else {
			int sv_errno = errno;
			(void) close(fd);
			MUTEX_UNLOCK_PROCMASK(&_ti_userlock, mask);
			errno = sv_errno;
			return (-1);
		}
	}

	/*
	 * _t_create synchronizes state witk kernel timod and
	 * already sets it to T_UNBND - what it needs to be
	 * be on T_OPEN event. No _T_TX_NEXTSTATE needed here.
	 */
	MUTEX_UNLOCK_PROCMASK(&_ti_userlock, mask);

	do {
		retval = _ioctl(fd, I_FLUSH, FLUSHRW);
	} while (retval < 0 && errno == EINTR);

	/*
	 * We ignore other error cases (retval < 0) - assumption is
	 * that I_FLUSH failures is temporary (e.g. ENOSR) or
	 * otherwise benign failure on a this newly opened file
	 * descriptor and not a critical failure.
	 */

	trace2(TR_t_open, 1, flags);
	return (fd);
}
