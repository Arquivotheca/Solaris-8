/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1997, by Sun Microsystems, Inc.
 * All Rights reserved.
 */

#pragma	ident   "@(#)readdir.c 1.25     99/08/27 SMI"	/* SVr4.0 1.11 */

/*LINTLIBRARY*/

/*
 * readdir -- C library extension routine
 */

#include	<sys/feature_tests.h>

#if !defined(_LP64)
#pragma weak readdir64 = _readdir64
#endif
#pragma weak readdir = _readdir

#include	"synonyms.h"
#include	<sys/types.h>
#include	<sys/dirent.h>
#include	<dirent.h>
#include	<limits.h>
#include	<errno.h>
#include	"libc.h"

#ifdef _LP64

struct dirent *
readdir(DIR *dirp)
{
	struct dirent	*dp;	/* -> directory data */
	int saveloc = 0;

	if (dirp->dd_size != 0) {
		dp = (struct dirent *)&dirp->dd_buf[dirp->dd_loc];
		saveloc = dirp->dd_loc;   /* save for possible EOF */
		dirp->dd_loc += (int)dp->d_reclen;
	}
	if (dirp->dd_loc >= dirp->dd_size)
		dirp->dd_loc = dirp->dd_size = 0;

	if (dirp->dd_size == 0 && 	/* refill buffer */
	    (dirp->dd_size = getdents(dirp->dd_fd,
	    (struct dirent *)dirp->dd_buf, DIRBUF)) <= 0) {
		if (dirp->dd_size == 0)	/* This means EOF */
				dirp->dd_loc = saveloc;  /* EOF so save for */
							/* telldir */
		return (NULL);	/* error or EOF */
	}

	return ((struct dirent *)&dirp->dd_buf[dirp->dd_loc]);
}

#else	/* _LP64 */

/*
 * Welcome to the complicated world of large files on a small system.
 */

struct dirent64 *
readdir64(DIR *dirp)
{
	struct dirent64	*dp64;	/* -> directory data */
	int saveloc = 0;

	if (dirp->dd_size != 0) {
		dp64 = (struct dirent64 *)&dirp->dd_buf[dirp->dd_loc];
		/* was converted by readdir and needs to be reversed */
		if (dp64->d_ino == (ino64_t)-1) {
			struct dirent		*dp32;

			dp32 = (struct dirent *)(&dp64->d_off);
			dp64->d_ino = (ino64_t)dp32->d_ino;
			dp64->d_off = (off64_t)dp32->d_off;
			dp64->d_reclen = (unsigned short)(dp32->d_reclen +
				((char *)&dp64->d_off - (char *)dp64));
		}
		saveloc = dirp->dd_loc;   /* save for possible EOF */
		dirp->dd_loc += (int)dp64->d_reclen;
	}
	if (dirp->dd_loc >= dirp->dd_size)
		dirp->dd_loc = dirp->dd_size = 0;

	if (dirp->dd_size == 0 && 	/* refill buffer */
	    (dirp->dd_size = getdents64(dirp->dd_fd,
	    (struct dirent64 *)dirp->dd_buf, DIRBUF)) <= 0) {
		if (dirp->dd_size == 0)	/* This means EOF */
				dirp->dd_loc = saveloc;  /* EOF so save for */
							/* telldir */
		return (NULL);	/* error or EOF */
	}

	dp64 = (struct dirent64 *)&dirp->dd_buf[dirp->dd_loc];
	return (dp64);
}

/*
 * readdir now does translation of dirent64 entries into dirent entries.
 * We rely on the fact that dirents are smaller than dirent64s and we
 * reuse the space accordingly.
 */
struct dirent *
readdir(DIR *dirp)
{
	struct dirent64		*dp64;	/* -> directory data */
	struct dirent		*dp32;	/* -> directory data */

	if ((dp64 = readdir64(dirp)) == NULL)
		return (NULL);

	/*
	 * Make sure that the offset fits in 32 bits.
	 */
	if (((off_t)dp64->d_off != dp64->d_off &&
		(uint64_t)dp64->d_off > (uint64_t)UINT32_MAX) ||
		(dp64->d_ino > SIZE_MAX)) {
			errno = EOVERFLOW;
			return (NULL);
	}

	dp32 = (struct dirent *)(&dp64->d_off);
	dp32->d_off = (off_t)dp64->d_off;
	dp32->d_ino = (ino_t)dp64->d_ino;
	dp32->d_reclen = (unsigned short)(dp64->d_reclen -
	    ((char *)&dp64->d_off - (char *)dp64));
	dp64->d_ino = (ino64_t)-1; /* flag as converted for readdir64 */
	/* d_name d_reclen should not move */
	return (dp32);
}
#endif	/* _LP64 */
