/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_STATFS_H
#define	_SYS_STATFS_H

#pragma ident	"@(#)statfs.h	1.9	96/12/03 SMI"	/* SVr4.0 11.10 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Structure returned by statfs(2) and fstatfs(2).
 * This structure and associated system calls have been replaced
 * by statvfs(2) and fstatvfs(2) and will be removed from the system
 * in a near-future release.
 */

struct	statfs {
	short	f_fstyp;	/* File system type */
	long	f_bsize;	/* Block size */
	long	f_frsize;	/* Fragment size (if supported) */
	long	f_blocks;	/* Total number of blocks on file system */
	long	f_bfree;	/* Total number of free blocks */
	ino_t	f_files;	/* Total number of file nodes (inodes) */
	ino_t	f_ffree;	/* Total number of free file nodes */
	char	f_fname[6];	/* Volume name */
	char	f_fpack[6];	/* Pack name */
};

#ifdef _SYSCALL32

struct statfs32 {
	int16_t	f_fstyp;
	int32_t	f_bsize;
	int32_t f_frsize;
	int32_t	f_blocks;
	int32_t	f_bfree;
	ino32_t	f_files;
	ino32_t	f_ffree;
	char	f_fname[6];
	char	f_fpack[6];
};

#endif	/* _SYSCALL32 */

#if defined(__STDC__) && !defined(_KERNEL)
int statfs(const char *, struct statfs *, int, int);
int fstatfs(int, struct statfs *, int, int);
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_STATFS_H */
