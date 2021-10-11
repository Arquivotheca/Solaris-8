/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)nfssys.c	1.11	98/07/06 SMI"	/* SVr4.0 1.1	*/

/*
 *  		PROPRIETARY NOTICE (Combined)
 *
 *  This source code is unpublished proprietary information
 *  constituting, or derived under license from AT&T's Unix(r) System V.
 *
 *
 *
 *  		Copyright Notice
 *
 *  Notice of copyright on this source code product does not indicate
 *  publication.
 *
 *  	(c) 1986,1987,1988,1989,1996  Sun Microsystems, Inc.
 *  	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 */

/*
 * Copyright (c) 1986-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*LINTLIBRARY*/
#pragma weak exportfs = _exportfs
#pragma weak nfs_getfh = _nfs_getfh
#pragma weak nfssvc = _nfssvc

#include "synonyms.h"
#include <sys/types.h>
#include <sys/types32.h>
#include <rpc/types.h>
#include <sys/vfs.h>
#include <nfs/nfs.h>
#include <nfs/export.h>
#include <nfs/nfssys.h>
#include "libc.h"

int
exportfs(char *dir, struct exportdata *ep)
{
	struct exportfs_args ea;

	ea.dname = dir;
	ea.uex = ep;
	return (_nfssys(EXPORTFS, &ea));
}

int
nfs_getfh(char *path, fhandle_t *fhp)
{
	struct nfs_getfh_args nga;

	nga.fname = path;
	nga.fhp = fhp;
	return (_nfssys(NFS_GETFH, &nga));
}

int
nfssvc(int fd)
{
	struct nfs_svc_args nsa;

	nsa.fd = fd;
	return (_nfssys(NFS_SVC, &nsa));
}
