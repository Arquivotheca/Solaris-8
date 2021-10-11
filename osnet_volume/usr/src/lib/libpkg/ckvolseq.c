/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/
#ident	"@(#)ckvolseq.c	1.9	93/03/10 SMI"	/* SVr4.0  1.6.1.1	*/

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <pkgstrct.h>
#include <pkglib.h>
#include "pkglocale.h"

extern char	errbuf[]; 		/* WHERE? */

#define	PKGMAP	"pkgmap"
#define	PKGINFO	"pkginfo"

#define	MSG_SEQ		"Volume is out of sequence."
#define	MSG_CORRUPT	"Volume is corrupt or is not part of the appropriate " \
			"package."
#define	ERR_NOPKGMAP	"ERROR: unable to process <%s>"
#define	ERR_BADPKGINFO	"ERROR: unable to process <%s>"

int
ckvolseq(char *dir, int part, int nparts)
{
	static struct cinfo cinfo;
	char	ftype, path[PATH_MAX];

	if (part > 0) {
		ftype = 'f';
		if (part == 1) {
			/*
			 * save stats about content information of pkginfo
			 * file in order to verify multi-volume packages
			 */
			cinfo.cksum = cinfo.size = cinfo.modtime = (-1L);
			(void) sprintf(path, "%s/pkginfo", dir);
			if (cverify(0, &ftype, path, &cinfo)) {
				logerr(pkg_gt(ERR_BADPKGINFO), path);
				logerr(errbuf);
				return (1);
			}
			(void) sprintf(path, "%s/pkgmap", dir);
			if (access(path, 0)) {
				logerr(pkg_gt(ERR_NOPKGMAP), path);
				return (2);
			}
		} else {
			/* temp fix due to summit problem */
			cinfo.modtime = (-1);

			/* pkginfo file doesn't match first floppy */
			(void) sprintf(path, "%s/pkginfo", dir);
			if (cverify(0, &ftype, path, &cinfo)) {
				logerr(pkg_gt(MSG_CORRUPT));
				logerr(errbuf);
				return (1);
			}
		}
	} else
		part = (-part);

	/*
	 * each volume in a multi-volume package must
	 * contain either the root.n or reloc.n directories
	 */
	if (nparts != 1) {
		/* look for multi-volume specification */
		(void) sprintf(path, "%s/root.%d", dir, part);
		if (access(path, 0) == 0)
			return (0);
		(void) sprintf(path, "%s/reloc.%d", dir, part);
		if (access(path, 0) == 0)
			return (0);
		if (part == 1) {
			(void) sprintf(path, "%s/install", dir, part);
			if (access(path, 0) == 0)
				return (0);
		}
		if (nparts) {
			logerr(pkg_gt(MSG_SEQ));
			return (2);
		}
	}
	return (0);
}
