/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986, 1987, 1988, 1989, 1994  Sun Microsystems, Inc
 * 	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *			All rights reserved.
 *
 */

#ident	"@(#)access.c	1.3	94/10/13 SMI"	/* SVr4 1.103	*/

#include <sys/param.h>
#include <sys/isa_defs.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/cred.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/pathname.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>

/*
 * Determine accessibility of file.
 */

#define	E_OK	010	/* use effective ids */
#define	R_OK	004
#define	W_OK	002
#define	X_OK	001

int
access(char *fname, int fmode)
{
	vnode_t *vp;
	register cred_t *tmpcr;
	register int error;
	register int mode;
	register int eok;
	register cred_t *cr;

	if (fmode & ~(E_OK|R_OK|W_OK|X_OK))
		return (set_errno(EINVAL));

	mode = ((fmode & (R_OK|W_OK|X_OK)) << 6);
	eok = (fmode & E_OK);

	if (eok)
		tmpcr = CRED();
	else {
		cr = CRED();
		tmpcr = crdup(cr);
		tmpcr->cr_uid = cr->cr_ruid;
		tmpcr->cr_gid = cr->cr_rgid;
		tmpcr->cr_ruid = cr->cr_uid;
		tmpcr->cr_rgid = cr->cr_gid;
	}

	if (error = lookupname(fname, UIO_USERSPACE, FOLLOW, NULLVPP, &vp)) {
		if (!eok)
			crfree(tmpcr);
		return (set_errno(error));
	}

	if (mode) {
		if (error = VOP_ACCESS(vp, mode, 0, tmpcr))
			(void) set_errno(error);
	}

	if (!eok)
		crfree(tmpcr);
	VN_RELE(vp);
	return (error);
}
