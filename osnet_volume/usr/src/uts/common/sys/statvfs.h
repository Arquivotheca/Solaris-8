/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1997,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_STATVFS_H
#define	_SYS_STATVFS_H

#pragma ident	"@(#)statvfs.h	1.24	99/05/04 SMI"	/* SVr4.0 1.10 */

#include <sys/feature_tests.h>
#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Structure returned by statvfs(2).
 */

#define	_FSTYPSZ	16
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#ifndef FSTYPSZ
#define	FSTYPSZ	_FSTYPSZ
#endif
#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

typedef struct statvfs {
	unsigned long	f_bsize;	/* fundamental file system block size */
	unsigned long	f_frsize;	/* fragment size */
	fsblkcnt_t	f_blocks;	/* total blocks of f_frsize on fs */
	fsblkcnt_t	f_bfree;	/* total free blocks of f_frsize */
	fsblkcnt_t	f_bavail;	/* free blocks avail to non-superuser */
	fsfilcnt_t	f_files;	/* total file nodes (inodes) */
	fsfilcnt_t	f_ffree;	/* total free file nodes */
	fsfilcnt_t	f_favail;	/* free nodes avail to non-superuser */
	unsigned long	f_fsid;		/* file system id (dev for now) */
	char		f_basetype[_FSTYPSZ];	/* target fs type name, */
						/* null-terminated */
	unsigned long	f_flag;		/* bit-mask of flags */
	unsigned long	f_namemax;	/* maximum file name length */
	char		f_fstr[32];	/* filesystem-specific string */
#if !defined(_LP64)
	unsigned long 	f_filler[16];	/* reserved for future expansion */
#endif
} statvfs_t;

#if defined(_SYSCALL32)

/* Kernel view of user ILP32 statvfs structure */

typedef struct statvfs32 {
	uint32_t	f_bsize;	/* fundamental file system block size */
	uint32_t	f_frsize;	/* fragment size */
	fsblkcnt32_t	f_blocks;	/* total blocks of f_frsize on fs */
	fsblkcnt32_t	f_bfree;	/* total free blocks of f_frsize */
	fsblkcnt32_t	f_bavail;	/* free blocks avail to non-superuser */
	fsfilcnt32_t	f_files;	/* total file nodes (inodes) */
	fsfilcnt32_t	f_ffree;	/* total free file nodes */
	fsfilcnt32_t	f_favail;	/* free nodes avail to non-superuser */
	uint32_t	f_fsid;		/* file system id (dev for now) */
	char		f_basetype[_FSTYPSZ];	/* target fs type name, */
						/* null-terminated */
	uint32_t	f_flag;		/* bit-mask of flags */
	uint32_t	f_namemax;	/* maximum file name length */
	char		f_fstr[32];	/* filesystem-specific string */
	uint32_t	f_filler[16];	/* reserved for future expansion */
} statvfs32_t;

#endif	/* _SYSCALL32 */

/* transitional large file interface version */
#if	defined(_LARGEFILE64_SOURCE)
typedef struct statvfs64 {
	unsigned long	f_bsize;	/* preferred file system block size */
	unsigned long	f_frsize;	/* fundamental file system block size */
	fsblkcnt64_t	f_blocks;	/* total blocks of f_frsize */
	fsblkcnt64_t	f_bfree;	/* total free blocks of f_frsize */
	fsblkcnt64_t	f_bavail;	/* free blocks avail to non-superuser */
	fsfilcnt64_t	f_files;	/* total # of file nodes (inodes) */
	fsfilcnt64_t	f_ffree;	/* total # of free file nodes */
	fsfilcnt64_t	f_favail;	/* free nodes avail to non-superuser */
	unsigned long	f_fsid;		/* file system id (dev for now) */
	char		f_basetype[FSTYPSZ];	/* target fs type name, */
						/* null-terminated */
	unsigned long	f_flag;		/* bit-mask of flags */
	unsigned long	f_namemax;	/* maximum file name length */
	char		f_fstr[32];	/* filesystem-specific string */
#if !defined(_LP64)
	unsigned long	f_filler[16];	/* reserved for future expansion */
#endif	/* _LP64 */
} statvfs64_t;
#endif

#if defined(_SYSCALL32)

/* Kernel view of user ILP32 statvfs64 structure */

#ifdef __ia64
#pragma pack(4)		/* data offset compatibility with ia32 */
#endif
typedef struct statvfs64_32 {
	uint32_t	f_bsize;	/* preferred file system block size */
	uint32_t	f_frsize;	/* fundamental file system block size */
	fsblkcnt64_t	f_blocks;	/* total blocks of f_frsize */
	fsblkcnt64_t	f_bfree;	/* total free blocks of f_frsize */
	fsblkcnt64_t	f_bavail;	/* free blocks avail to non-superuser */
	fsfilcnt64_t	f_files;	/* total # of file nodes (inodes) */
	fsfilcnt64_t	f_ffree;	/* total # of free file nodes */
	fsfilcnt64_t	f_favail;	/* free nodes avail to non-superuser */
	uint32_t	f_fsid;		/* file system id (dev for now) */
	char		f_basetype[FSTYPSZ];	/* target fs type name, */
						/* null-terminated */
	uint32_t	f_flag;		/* bit-mask of flags */
	uint32_t	f_namemax;	/* maximum file name length */
	char		f_fstr[32];	/* filesystem-specific string */
	uint32_t	f_filler[16];	/* reserved for future expansion */
} statvfs64_32_t;
#ifdef __ia64
#pragma pack()
#endif

#endif	/* _SYSCALL32 */

/*
 * Flag definitions.
 */

#define	ST_RDONLY	0x01	/* read-only file system */
#define	ST_NOSUID	0x02	/* does not support setuid/setgid semantics */
#define	ST_NOTRUNC	0x04	/* does not truncate long file names */

#if !defined(_KERNEL)
/*
 * large file compilation environment setup
 */
#if !defined(_LP64) && _FILE_OFFSET_BITS == 64
#ifdef __PRAGMA_REDEFINE_EXTNAME
#pragma redefine_extname	statvfs		statvfs64
#pragma redefine_extname	fstatvfs	fstatvfs64
#else
#define	statvfs			statvfs64
#define	fstatvfs		fstatvfs64
#endif
#endif	/* !_LP64 && _FILE_OFFSET_BITS == 64 */

#if defined(_LP64) && defined(_LARGEFILE64_SOURCE)
/*
 * In the LP64 compilation environment, map large file interfaces
 * back to native versions where possible.
 */
#ifdef __PRAGMA_REDEFINE_EXTNAME
#pragma	redefine_extname	statvfs64	statvfs
#pragma	redefine_extname	fstatvfs64	fstatvfs
#else
#define	statvfs64		statvfs
#define	fstatvfs64		fstatvfs
#endif
#endif	/* _LP64 && _LARGEFILE64_SOURCE */

#if defined(__STDC__)
int statvfs(const char *, statvfs_t *);
int fstatvfs(int, statvfs_t *);

/* transitional large file interface versions */
#if	defined(_LARGEFILE64_SOURCE) && !((_FILE_OFFSET_BITS == 64) && \
	    !defined(__PRAGMA_REDEFINE_EXTNAME))
int statvfs64(const char *, statvfs64_t *);
int fstatvfs64(int, statvfs64_t *);
#endif	/* _LARGEFILE64_SOURCE... */
#endif	/* defined(__STDC__) */
#endif	/* !defined(_KERNEL) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_STATVFS_H */
