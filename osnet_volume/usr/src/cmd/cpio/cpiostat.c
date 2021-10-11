/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident	"@(#)cpiostat.c	1.8	95/11/06 SMI"	/* SVr4.0 1.1	*/

#define	_STYPES
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mkdev.h>

static struct cpioinfo {
	o_dev_t st_dev;
	o_ino_t st_ino;
	o_mode_t	st_mode;
	o_nlink_t	st_nlink;
	uid_t st_uid;		/* actual uid */
	gid_t st_gid;		/* actual gid */
	o_dev_t st_rdev;
	off_t	st_size;
	time_t	st_modtime;
} tmpinfo;


static o_dev_t
convert(dev_t dev)
{
	major_t maj, min;

	maj = major(dev);	/* get major number */
	min = minor(dev);	/* get minor number */

	/* make old device number */
	return ((maj << 8) | min);
}

struct cpioinfo *
svr32stat(char *fname)
{
	struct stat stbuf;

	if (stat(fname, &stbuf) < 0) {
		stbuf.st_ino = 0;
	}
	tmpinfo.st_dev = convert(stbuf.st_dev);
	tmpinfo.st_ino = stbuf.st_ino;
	tmpinfo.st_mode = stbuf.st_mode;
	tmpinfo.st_nlink = stbuf.st_nlink;
	tmpinfo.st_uid = stbuf.st_uid;
	tmpinfo.st_gid = stbuf.st_gid;
	tmpinfo.st_rdev = convert(stbuf.st_rdev);
	tmpinfo.st_size = stbuf.st_size;
	tmpinfo.st_modtime = stbuf.st_mtime;
	return (&tmpinfo);
}

struct cpioinfo *
svr32lstat(char *fname)
{
	struct stat stbuf;

	if (!lstat(fname, &stbuf)) {
		tmpinfo.st_dev = convert(stbuf.st_dev);
		tmpinfo.st_ino = stbuf.st_ino;
		tmpinfo.st_mode = stbuf.st_mode;
		tmpinfo.st_nlink = stbuf.st_nlink;
		tmpinfo.st_uid = stbuf.st_uid;
		tmpinfo.st_gid = stbuf.st_gid;
		tmpinfo.st_rdev = convert(stbuf.st_rdev);
		tmpinfo.st_size = stbuf.st_size;
		tmpinfo.st_modtime = stbuf.st_mtime;
		return (&tmpinfo);
	}

	return ((struct cpioinfo *)0);
}
