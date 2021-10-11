/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any 	*/
/*	actual or intended publication of such source code.	*/

#ifndef	_SYS_FS_S5_FS_H
#define	_SYS_FS_S5_FS_H

#pragma ident	"@(#)s5_fs.h	1.3	98/01/06 SMI"

#include <sys/t_lock.h>		/* for kmutex_t */

#ifdef	__cplusplus
extern "C" {
#endif

#define	BBSIZE		512
#define	SBSIZE		512
#define	BBOFF		((off_t)(0))
#define	SBOFF		((off_t)(BBOFF + BBSIZE))
#define	BBLOCK		((daddr_t)(0))
#define	SBLOCK		((daddr_t)(BBLOCK + BBSIZE / DEV_BSIZE))

#define	SUPERBOFF	SBOFF

/*
 * The root inode is the root of the file system.
 * Inode 0 can't be used for normal purposes and
 * historically bad blocks were linked to inode 1,
 * thus the root inode is 2. (inode 1 is no longer used for
 * this purpose, however numerous dump tapes make this
 * assumption, so we are stuck with it)
 * The lost+found directory is given the next available
 * inode when it is created by ``mkfs''.
 */
#define	S5ROOTINO	((ino_t)2)	/* i number of all roots */
#define	LOSTFOUNDINO    (S5ROOTINO + 1)

/*
 * Mutex to control the number of active file system sync's
 */
extern kmutex_t s5_syncbusy;


/*
 * Super block for a file system.
 *
 */
#define	NICINOD	100	/* number of superblock inodes */
#define	NICFREE 50	/* number of superblock free blocks */

#define	s_bshift  s_type	/* so far, type is just bsize shift factor */
#define	FsMINBSHIFT	1
#define	FsMINBSIZE	512
#define	FsBSIZE(bshift)		(FsMINBSIZE << ((bshift) - FsMINBSHIFT))

#define	FsMAGIC	0xfd187e20	/* s_magic */
#define	FsOKAY	0x7c269d38	/* s_state: clean */
#define	FsACTIVE	0x5e72d81a	/* s_state: active */
#define	FsBAD	0xcb096f43	/* s_state: bad root */
#define	FsBADBLK	0xbadbc14b	/* s_state: bad block corrupted it */

/* Old symbols for specific s_type values. */
#define	Fs1b	1	/* 512-byte blocks */
#define	Fs2b	2	/* 1024-byte blocks */
#define	Fs4b	3	/* 2048-byte blocks */

struct	filsys {
	ushort_t s_isize;	/* size in blocks of i-list */
	daddr_t	s_fsize;	/* size in blocks of entire volume */
	short	s_nfree;	/* number of addresses in s_free */
	daddr_t	s_free[NICFREE]; /* free block list */
		/* S5 inode definition cannot change for EFT */
	short	s_ninode;	/* number of i-nodes in s_inode */
	o_ino_t	s_inode[NICINOD]; /* free i-node list */
	char	s_flock;	/* lock during free list manipulation */
	char	s_ilock;	/* lock during i-list manipulation */
	char  	s_fmod; 	/* super block modified flag */
	char	s_ronly;	/* mounted read-only flag */
	time_t	s_time; 	/* last super block update */
	short	s_dinfo[4];	/* device information */
	daddr_t	s_tfree;	/* total free blocks */
	o_ino_t	s_tinode;	/* total free inodes */
	char	s_fname[6];	/* file system name */
	char	s_fpack[6];	/* file system pack name */
	long	s_fill[12];	/* adjust to make sizeof filsys */
	long	s_state;	/* file system state */
	long	s_magic;	/* magic number to indicate new file system */
	ulong_t	s_type;		/* type of new file system */
};


/*
 * Macros for handling inode numbers:
 *	inode number to file system block offset.
 *	inode number to file system block address.
 */

#define	getfs(vfsp)  ((struct filsys *)	\
	((struct s5vfs *)vfsp->vfs_data)->vfs_bufp->b_un.b_addr)

#define	blksize(s5vfs) ((s5vfs)->vfs_bsize)

/*
 * Turn file system block numbers into disk block addresses.
 * This maps file system blocks to device size blocks.
 */
#define	fsbtodb(fs, b)	((b) << (fs)->s_bshift - 1)

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FS_S5_FS_H */
