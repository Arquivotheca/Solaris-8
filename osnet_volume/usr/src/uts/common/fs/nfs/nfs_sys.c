/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)nfs_sys.c	1.35	99/08/13 SMI"	/* SVr4.0 1.5   */

/*
 *  		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's Unix(r) System V.
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * Copyright (c) 1986-1989,1993-1995,1997,1999 by Sun Microsystems, Inc.
 * Copyright (c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 * All rights reserved.
 */

#include <sys/types.h>
#include <rpc/types.h>
#include <sys/systm.h>
#include <sys/vfs.h>
#include <sys/errno.h>
#include <sys/cred.h>
#include <sys/siginfo.h>
#include <sys/proc.h>		/* for exit() declaration */
#include <nfs/nfs.h>
#include <nfs/nfssys.h>
#include <sys/thread.h>
#include <rpc/auth.h>
#include <rpc/rpcsys.h>
#include <rpc/svc.h>

int
nfssys(enum nfssys_op opcode, void *arg)
{
	int error;

	switch (opcode) {
	case SVCPOOL_CREATE: { /* setup an RPC server thread pool */
		struct svcpool_args p;

		if (copyin(arg, &p, sizeof (p)))
			return (set_errno(EFAULT));

		error = svc_pool_create(&p);
		break;
	}

	case NFS_SVC: { /* NFS server daemon */
		STRUCT_DECL(nfs_svc_args, nsa);

		STRUCT_INIT(nsa, get_udatamodel());

		if (copyin(arg, STRUCT_BUF(nsa), STRUCT_SIZE(nsa)))
			return (set_errno(EFAULT));

		error = nfs_svc(STRUCT_BUF(nsa), get_udatamodel());
		break;
	}

	case EXPORTFS: { /* export a file system */
		STRUCT_DECL(exportfs_args, ea);

		STRUCT_INIT(ea, get_udatamodel());
		if (copyin(arg, STRUCT_BUF(ea), STRUCT_SIZE(ea)))
			return (set_errno(EFAULT));

		error = exportfs(STRUCT_BUF(ea), get_udatamodel(), CRED());
		break;
	}

	case NFS_GETFH: { /* get a file handle */
		STRUCT_DECL(nfs_getfh_args, nga);

		STRUCT_INIT(nga, get_udatamodel());
		if (copyin(arg, STRUCT_BUF(nga), STRUCT_SIZE(nga)))
			return (set_errno(EFAULT));

		error = nfs_getfh(STRUCT_BUF(nga), get_udatamodel(), CRED());
		break;
	}

	case NFS_REVAUTH: { /* revoke the cached credentials for the uid */
		STRUCT_DECL(nfs_revauth_args, nra);

		STRUCT_INIT(nra, get_udatamodel());
		if (copyin(arg, STRUCT_BUF(nra), STRUCT_SIZE(nra)))
			return (set_errno(EFAULT));

		error = sec_clnt_revoke(STRUCT_FGET(nra, authtype),
		    STRUCT_FGET(nra, uid), CRED(), NULL, get_udatamodel());
		break;
	}

	case LM_SVC: { /* LM server daemon */
		struct lm_svc_args lsa;

		if (get_udatamodel() != DATAMODEL_NATIVE) {
			STRUCT_DECL(lm_svc_args, ulsa);

			STRUCT_INIT(ulsa, get_udatamodel());
			if (copyin(arg, STRUCT_BUF(ulsa), STRUCT_SIZE(ulsa)))
				return (set_errno(EFAULT));

			lsa.version = STRUCT_FGET(ulsa, version);
			lsa.fd = STRUCT_FGET(ulsa, fd);
			lsa.n_fmly = STRUCT_FGET(ulsa, n_fmly);
			lsa.n_proto = STRUCT_FGET(ulsa, n_proto);
			lsa.n_rdev = expldev(STRUCT_FGET(ulsa, n_rdev));
			lsa.debug = STRUCT_FGET(ulsa, debug);
			lsa.timout = STRUCT_FGET(ulsa, timout);
			lsa.grace = STRUCT_FGET(ulsa, grace);
			lsa.retransmittimeout = STRUCT_FGET(ulsa,
			    retransmittimeout);
		} else {
			if (copyin(arg, &lsa, sizeof (lsa)))
				return (set_errno(EFAULT));
		}

		error = lm_svc(&lsa);
		break;
	}

	case KILL_LOCKMGR: {
		error = lm_shutdown();
		break;
	}

	case LOG_FLUSH:	{	/* Flush log buffer and possibly rename */
		STRUCT_DECL(nfsl_flush_args, nfa);

		STRUCT_INIT(nfa, get_udatamodel());
		if (copyin(arg, STRUCT_BUF(nfa), STRUCT_SIZE(nfa)))
			return (set_errno(EFAULT));

		error = nfsl_flush(STRUCT_BUF(nfa), get_udatamodel(), CRED());
		break;
	}

	default:
		error = EINVAL;
		break;
	}

	return ((error != 0) ? set_errno(error) : 0);
}
