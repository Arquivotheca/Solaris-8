/*
 * Copyright (c) 1996,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef lint
#ident  "@(#)ident_ufs.c 1.6     97/06/09 SMI"
#endif

#include	<stdio.h>
#include	<fcntl.h>
#include	<rpc/types.h>
#include	<sys/types.h>
#include	<sys/fs/ufs_fs.h>

#include	<rmmount.h>

/*
 * We call it a ufs file system iff:
 *	The magic number for the superblock is correct.
 *
 */

int
ident_fs(int fd, char *rawpath, int *clean, int verbose)
{
	struct fs fs;

	if (lseek(fd, SBOFF, SEEK_SET) < 0) {
		perror("ufs seek");
		return (FALSE);
	}

	if (read(fd, &fs, sizeof (fs)) < 0) {
		perror("ufs read");
		return (FALSE);
	}

	if (fs.fs_clean == FSCLEAN || fs.fs_clean == FSSTABLE)
		*clean = TRUE;
	else
		*clean = FALSE;

	if (fs.fs_magic == FS_MAGIC) {
		return (TRUE);
	}
	return (FALSE);
}
