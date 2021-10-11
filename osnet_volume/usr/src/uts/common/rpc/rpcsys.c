/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)rpcsys.c	1.5	98/06/21 SMI" /* SVr4.0 1.5 */

/*
 *  		PROPRIETARY NOTICE (Combined)
 *
 *  This source code is unpublished proprietary information
 *  constituting, or derived under license from AT&T's Unix(r) System V.
 *
 *
 *		Copyright Notice
 *
 *  Notice of copyright on this source code product does not indicate
 *  publication.
 *
 *  	Copyright (c) 1986-1989,1993,1994,1995 by Sun Microsystems, Inc.
 *  	Copyright (c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	All rights reserved.
 */

#include <sys/types.h>
#include <rpc/types.h>
#include <sys/systm.h>
#include <sys/vfs.h>
#include <sys/errno.h>
#include <sys/cred.h>
#include <rpc/rpcsys.h>
#include <sys/model.h>


/*ARGSUSED*/
int
rpcsys(enum rpcsys_op opcode, void *arg)
{
	switch (opcode) {
	case KRPC_REVAUTH:
		/* revoke the cached credentials for the given uid */
		{
		STRUCT_DECL(krpc_revauth, nra);
		int result;

		STRUCT_INIT(nra, get_udatamodel());
		if (copyin(arg, STRUCT_BUF(nra), STRUCT_SIZE(nra)))
			return (set_errno(EFAULT));

		result = sec_clnt_revoke(STRUCT_FGET(nra, rpcsec_flavor_1),
				STRUCT_FGET(nra, uid_1), CRED(),
				STRUCT_FGETP(nra, flavor_data_1),
				get_udatamodel());
		return ((result != 0) ? set_errno(result) : 0);
		}

	default:
		return (set_errno(EINVAL));
	}
}
