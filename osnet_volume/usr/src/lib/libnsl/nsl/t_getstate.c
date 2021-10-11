/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1993-96 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/


#pragma ident	"@(#)t_getstate.c	1.18	97/08/12 SMI"
		/* SVr4.0 1.4.2.1 */

#include <errno.h>
#include <rpc/trace.h>
#include <xti.h>
#include <stropts.h>
#include <sys/timod.h>
#include "timt.h"
#include "tx.h"


int
_tx_getstate(int fd, int api_semantics)
{
	struct _ti_user *tiptr;
	int sv_errno;

	trace2(TR_t_getstate, 0, fd);
	if ((tiptr = _t_checkfd(fd, 0, api_semantics)) == NULL) {
		sv_errno = errno;
		trace2(TR_t_getstate, 1, fd);
		errno = sv_errno;
		return (-1);
	}

	switch (tiptr->ti_state) {

	case T_UNBND:
	case T_IDLE:
	case T_INCON:
	case T_OUTCON:
	case T_DATAXFER:
	case T_INREL:
	case T_OUTREL:
		trace2(TR_t_getstate, 1, fd);
		return (tiptr->ti_state);
	default:
		t_errno = TSTATECHNG;
		trace2(TR_t_getstate, 1, fd);
		return (-1);
	}
}
