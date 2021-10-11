/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1993-96 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/

#pragma ident	"@(#)t_sync.c	1.25	98/04/19 SMI"	/* SVr4.0 1.4.4.1 */

#include <errno.h>
#include <rpc/trace.h>
#include <xti.h>
#include <stropts.h>
#include <sys/timod.h>
#include "timt.h"
#include "tx.h"

int
_tx_sync(int fd, int api_semantics)
{
	struct _ti_user *tiptr;
	int sv_errno;
	int force_sync = 0;

	trace2(TR_t_sync, 0, fd);
	/*
	 * In case of fork/exec'd servers, _t_checkfd() has all
	 * the code to synchronize the tli data structures.
	 *
	 * We do a "forced sync" for XTI and not TLI. Detailed comments
	 * in _utililty.c having to do with rpcgen generated code and
	 * associated risk.
	 *
	 */
	if (_T_IS_XTI(api_semantics))
		force_sync = 1;

	if ((tiptr = _t_checkfd(fd, force_sync, api_semantics)) == NULL) {
		sv_errno = errno;
		trace2(TR_t_sync, 1, fd);
		errno = sv_errno;
		return (-1);
	}
	trace2(TR_t_sync, 1, fd);
	return (tiptr->ti_state);
}
