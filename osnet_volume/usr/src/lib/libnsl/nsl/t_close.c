/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1993-96 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/


#pragma ident	"@(#)t_close.c	1.21	97/08/12 SMI"	/* SVr4.0 1.5 */

#include <rpc/trace.h>
#include <errno.h>
#include <unistd.h>
#include <xti.h>
#include <signal.h>
#include <stropts.h>
#include "timt.h"
#include "tx.h"

int
_tx_close(int fd, int api_semantics)
{
	sigset_t mask;
	int sv_errno;

	trace2(TR_t_close, 0, fd);

	if (_t_checkfd(fd, 0, api_semantics) == NULL) {
		sv_errno = errno;
		trace2(TR_t_close, 1, fd);
		errno = sv_errno;
		return (-1);
	}

	MUTEX_LOCK_PROCMASK(&_ti_userlock, mask);
	if (_t_delete_tilink(fd) < 0) {
		sv_errno = errno;
		MUTEX_UNLOCK_PROCMASK(&_ti_userlock, mask);
		trace2(TR_t_close, 1, fd);
		errno = sv_errno;
		return (-1);
	}
	/*
	 * Note: close() needs to be inside the lock. If done
	 * outside, another process may inherit the desriptor
	 * and recreate library level instance structures
	 */
	(void) close(fd);

	MUTEX_UNLOCK_PROCMASK(&_ti_userlock, mask);
	trace2(TR_t_close, 1, fd);
	return (0);
}
