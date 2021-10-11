/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1995-1997,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_DIRENT_H
#define	_SYS_DIRENT_H

#pragma ident	"@(#)dirent.h	1.32	99/05/04 SMI"	/* SVr4.0 11.11 */

#include <sys/feature_tests.h>

#ifdef	__cplusplus
extern "C" {
#endif


/*
 * File-system independent directory entry.
 */
typedef struct dirent {
	ino_t		d_ino;		/* "inode number" of entry */
	off_t		d_off;		/* offset of disk directory entry */
	unsigned short	d_reclen;	/* length of this record */
	char		d_name[1];	/* name of file */
} dirent_t;

#if defined(_SYSCALL32)

/* kernel's view of user ILP32 dirent */

typedef	struct dirent32 {
	ino32_t		d_ino;		/* "inode number" of entry */
	off32_t		d_off;		/* offset of disk directory entry */
	uint16_t	d_reclen;	/* length of this record */
	char		d_name[1];	/* name of file */
} dirent32_t;

#endif	/* _SYSCALL32 */

#ifdef	_LARGEFILE64_SOURCE

/*
 * transitional large file interface version AND
 * kernel internal version
 */
#ifdef __ia64
#pragma pack(4)		/* data offset compatibility with ia32 */
#endif
typedef struct dirent64 {
	ino64_t		d_ino;		/* "inode number" of entry */
	off64_t		d_off;		/* offset of disk directory entry */
	unsigned short	d_reclen;	/* length of this record */
	char		d_name[1];	/* name of file */
} dirent64_t;
#ifdef __ia64
#pragma pack()
#endif

#endif	/* _LARGEFILE64_SOURCE */

#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
#if defined(_KERNEL)
#define	DIRENT64_RECLEN(namelen)	\
	((offsetof(dirent64_t, d_name[0]) + 1 + (namelen) + 7) & ~ 7)
#define	DIRENT32_RECLEN(namelen)	\
	((offsetof(dirent_t, d_name[0]) + 1 + (namelen) + 3) & ~ 3)
#endif

#if !defined(_KERNEL)

/*
 * large file compilation environment setup
 *
 * In the LP64 compilation environment, map large file interfaces
 * back to native versions where possible. (This only works because
 * a 'struct dirent' == 'struct dirent64').
 */

#if !defined(_LP64) && _FILE_OFFSET_BITS == 64
#ifdef	__PRAGMA_REDEFINE_EXTNAME
#pragma redefine_extname	getdents	getdents64
#else
#define	getdents		getdents64
#endif
#endif	/* !_LP64 && _FILE_OFFSET_BITS == 64 */

#if defined(_LP64) && defined(_LARGEFILE64_SOURCE)
#ifdef	__PRAGMA_REDEFINE_EXTNAME
#pragma	redefine_extname	getdents64	getdents
#else
#define	getdents64		getdents
#define	dirent64		dirent
#endif
#endif	/* _LP64 && _LARGEFILE64_SOURCE */

#if defined(__STDC__)
extern int getdents(int, struct dirent *, size_t);
#else
extern int getdents();
#endif

/* N.B.: transitional large file interface version deliberately not provided */

#endif /* !defined(_KERNEL) */
#endif /* (!defined(_POSIX_C_SOURCE)  && !defined(_XOPEN_SOURCE)) ... */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DIRENT_H */
