/*
 * Copyright (c) 1990-1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)iob.h	1.6	96/04/08 SMI" /* from SunOS 4.1 */

/*
 * This stuff keeps track of an open file in the standalone I/O system.
 *
 * The definitions herein are *private* to ufs.c
 *
 * It includes an IOB for device addess, an inode, a buffer for reading
 * indirect blocks and inodes, and a buffer for the superblock of the
 * file system (if any).
 *
 * To make the boot block smaller, we're using a 'bnode' (for boot node)
 * struct instead of an inode struct. This contains just the common
 * data from the on-disk inode.
 */

struct saioreq {
	off_t	si_offset;
	char	*si_ma;			/* memory address to r/w */
	int	si_cc;			/* character count to r/w */
};

struct bnode 
{
	dev_t i_dev;			/* from inode struct */
	struct icommon i_ic;		/* disk inode struct */
};


struct iob {
	void		*i_si;		/* I/O handle for this file */
	struct {
		off_t	si_offset;
		char	*si_ma;		/* memory address to r/w */
		int	si_cc;		/* character count to r/w */
	} i_saio;			/* I/O request block */
	struct bnode	i_ino;		/* Inode for this file */
	char		i_buf[MAXBSIZE]; /* Buffer for reading inodes & dirs */
	union {
		struct fs ui_fs;	/* Superblock for file system */
		char dummy[SBSIZE];
	} i_un;
};

/*
 * XXX: i_fs conflicts with a definition in ufs_inode.h...
 */
#define	iob_fs		i_un.ui_fs
