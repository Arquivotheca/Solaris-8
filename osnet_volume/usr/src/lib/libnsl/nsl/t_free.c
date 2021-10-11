/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1993-96 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/


#pragma ident	"@(#)t_free.c	1.17	98/04/19 SMI"	/* SVr4.0 1.2 */

#include <xti.h>
#include <errno.h>
#include <stropts.h>
#include <stdlib.h>
#include <sys/types.h>
#include <rpc/trace.h>
#include "timt.h"
#include "tx.h"

int
_tx_free(char *ptr, int struct_type, int api_semantics)
{
	union structptrs {
		struct t_bind *bind;
		struct t_call *call;
		struct t_discon *dis;
		struct t_optmgmt *opt;
		struct t_unitdata *udata;
		struct t_uderr *uderr;
	} p;

	/*
	 * Free all the buffers associated with the appropriate
	 * fields of each structure.
	 */

	trace2(TR_t_free, 0, struct_type);
	switch (struct_type) {

	case T_BIND:
		p.bind = (struct t_bind *)ptr;
		if (p.bind->addr.buf != NULL)
			free(p.bind->addr.buf);
		break;

	case T_CALL:
		p.call = (struct t_call *)ptr;
		if (p.call->addr.buf != NULL)
			free(p.call->addr.buf);
		if (p.call->opt.buf != NULL)
			free(p.call->opt.buf);
		if (p.call->udata.buf != NULL)
			free(p.call->udata.buf);
		break;

	case T_OPTMGMT:
		p.opt = (struct t_optmgmt *)ptr;
		if (p.opt->opt.buf != NULL)
			free(p.opt->opt.buf);
		break;

	case T_DIS:
		p.dis = (struct t_discon *)ptr;
		if (p.dis->udata.buf != NULL)
			free(p.dis->udata.buf);
		break;

	case T_UNITDATA:
		p.udata = (struct t_unitdata *)ptr;
		if (p.udata->addr.buf != NULL)
			free(p.udata->addr.buf);
		if (p.udata->opt.buf != NULL)
			free(p.udata->opt.buf);
		if (p.udata->udata.buf != NULL)
			free(p.udata->udata.buf);
		break;

	case T_UDERROR:
		p.uderr = (struct t_uderr *)ptr;
		if (p.uderr->addr.buf != NULL)
			free(p.uderr->addr.buf);
		if (p.uderr->opt.buf != NULL)
			free(p.uderr->opt.buf);
		break;

	case T_INFO:
		break;

	default:
		if (_T_IS_XTI(api_semantics)) {
			t_errno = TNOSTRUCTYPE;
			trace2(TR_t_free, 1, struct_type);
		} else {	/* TX_TLI_API */
			t_errno = TSYSERR;
			trace2(TR_t_free, 1, struct_type);
			errno = EINVAL;
		}
		return (-1);
	}

	free(ptr);
	trace2(TR_t_free, 1, struct_type);
	return (0);
}
