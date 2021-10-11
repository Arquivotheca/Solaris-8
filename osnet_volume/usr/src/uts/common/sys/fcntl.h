/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1994-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_FCNTL_H
#define	_SYS_FCNTL_H

#pragma ident	"@(#)fcntl.h	1.45	98/07/17 SMI"	/* SVr4.0 11.38	*/

#include <sys/feature_tests.h>

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Flag values accessible to open(2) and fcntl(2)
 * (the first three can only be set by open).
 */
#define	O_RDONLY	0
#define	O_WRONLY	1
#define	O_RDWR		2
#if defined(__EXTENSIONS__) || !defined(_POSIX_C_SOURCE)
#define	O_NDELAY	0x04	/* non-blocking I/O */
#endif /* defined(__EXTENSIONS__) || !defined(_POSIX_C_SOURCE) */
#define	O_APPEND	0x08	/* append (writes guaranteed at the end) */
#if defined(__EXTENSIONS__) || !defined(_POSIX_C_SOURCE) || \
	(_POSIX_C_SOURCE > 2) || defined(_XOPEN_SOURCE)
#define	O_SYNC		0x10	/* synchronized file update option */
#define	O_DSYNC		0x40	/* synchronized data update option */
#define	O_RSYNC		0x8000	/* synchronized file update option */
				/* defines read/write file integrity */
#endif /* defined(__EXTENSIONS__) || !defined(_POSIX_C_SOURCE) ... */
#define	O_NONBLOCK	0x80	/* non-blocking I/O (POSIX) */
#ifdef	SUN_SRC_COMPAT
#define	O_PRIV 		0x1000  /* Private access to file */
#endif /* SUN_SRC_COMPAT */
#ifdef	_LARGEFILE_SOURCE
#define	O_LARGEFILE	0x2000
#endif

/*
 * Flag values accessible only to open(2).
 */
#define	O_CREAT		0x100	/* open with file create (uses third arg) */
#define	O_TRUNC		0x200	/* open with truncation */
#define	O_EXCL		0x400	/* exclusive open */
#define	O_NOCTTY	0x800	/* don't allocate controlling tty (POSIX) */

/*
 * fcntl(2) requests
 *
 * N.B.: values are not necessarily assigned sequentially below.
 */
#define	F_DUPFD		0	/* Duplicate fildes */
#define	F_GETFD		1	/* Get fildes flags */
#define	F_SETFD		2	/* Set fildes flags */
#define	F_GETFL		3	/* Get file flags */
#define	F_SETFL		4	/* Set file flags */

/*
 * Applications that read /dev/mem must be built like the kernel.  A
 * new symbol "_KMEMUSER" is defined for this purpose.
 */
#if defined(_KERNEL) || defined(_KMEMUSER)
#define	F_O_GETLK	5	/* SVR3 Get file lock (need for rfs, across */
				/* the wire compatibility */
/* clustering: lock id contains both per-node sysid and node id */
#define	SYSIDMASK		0x0000ffff
#define	GETSYSID(id)		(id & SYSIDMASK)
#define	NODEIDMASK		0xffff0000
#define	BITS_IN_SYSID		16
#define	GETNLMID(sysid)		((int)(((uint_t)(sysid) & NODEIDMASK) >> \
				    BITS_IN_SYSID))

/* Clustering: Macro used for PXFS locks */
#define	GETPXFSID(sysid)	((int)(((uint_t)(sysid) & NODEIDMASK) >> \
				    BITS_IN_SYSID))
#endif	/* defined(_KERNEL) */

#define	F_CHKFL		8	/* Unused */
#define	F_DUP2FD	9	/* Duplicate fildes at third arg */

#define	F_ALLOCSP	10	/* Reserved */
#define	F_ISSTREAM	13	/* Is the file desc. a stream ? */
#define	F_PRIV		15	/* Turn on private access to file */
#define	F_NPRIV		16	/* Turn off private access to file */
#define	F_QUOTACTL	17	/* UFS quota call */
#define	F_BLOCKS	18	/* Get number of BLKSIZE blocks allocated */
#define	F_BLKSIZE	19	/* Get optimal I/O block size */
/*
 * Numbers 20-22 have been removed and should not be reused.
 */
#define	F_GETOWN	23	/* Get owner (socket emulation) */
#define	F_SETOWN	24	/* Set owner (socket emulation) */

#ifdef C2_AUDIT
#define	F_REVOKE	25	/* C2 Security. Revoke access to file desc. */
#endif

#define	F_HASREMOTELOCKS 26	/* Does vp have NFS locks; private to lock */
				/* manager */

/*
 * Commands that refer to flock structures.  The argument types differ between
 * the large and small file environments; therefore, the #defined values must
 * as well.
 */

#if defined(_LP64) || _FILE_OFFSET_BITS == 32
/* "Native" application compilation environment */
#define	F_SETLK		6	/* Set file lock */
#define	F_SETLKW	7	/* Set file lock and wait */
#define	F_FREESP	11	/* Free file space */
#define	F_GETLK		14	/* Get file lock */
#else
/* ILP32 large file application compilation environment version */
#define	F_SETLK		34	/* Set file lock */
#define	F_SETLKW	35	/* Set file lock and wait */
#define	F_FREESP	27	/* Free file space */
#define	F_GETLK		33	/* Get file lock */
#endif /* _LP64 || _FILE_OFFSET_BITS == 32 */

#if 	defined(_LARGEFILE64_SOURCE)

#if !defined(_LP64) || defined(_KERNEL)
/*
 * transitional large file interface version
 * These are only valid in a 32 bit application compiled with large files
 * option, for source compatibility, the 64-bit versions are mapped back
 * to the native versions.
 */
#define	F_SETLK64	34	/* Set file lock */
#define	F_SETLKW64	35	/* Set file lock and wait */
#define	F_FREESP64	27	/* Free file space */
#define	F_GETLK64	33	/* Get file lock */
#else
#define	F_SETLK64	6	/* Set file lock */
#define	F_SETLKW64	7	/* Set file lock and wait */
#define	F_FREESP64	11	/* Free file space */
#define	F_GETLK64	14	/* Get file lock */
#endif /* !_LP64 || _KERNEL */

#endif /* _LARGEFILE64_SOURCE */

#define	F_SHARE		40	/* Set a file share reservation */
#define	F_UNSHARE	41	/* Remove a file share reservation */

/*
 * File segment locking set data type - information passed to system by user.
 */

/* regular version, for both small and large file compilation environment */
typedef struct flock {
	short	l_type;
	short	l_whence;
	off_t	l_start;
	off_t	l_len;		/* len == 0 means until end of file */
	int	l_sysid;
	pid_t	l_pid;
	long	l_pad[4];		/* reserve area */
} flock_t;

#if defined(_SYSCALL32)

/* Kernel's view of ILP32 flock structure */

typedef struct flock32 {
	int16_t	l_type;
	int16_t	l_whence;
	off32_t	l_start;
	off32_t	l_len;		/* len == 0 means until end of file */
	int32_t	l_sysid;
	pid32_t	l_pid;
	int32_t	l_pad[4];		/* reserve area */
} flock32_t;

#endif /* _SYSCALL32 */

/* transitional large file interface version */

#if 	defined(_LARGEFILE64_SOURCE)

typedef struct flock64 {
	short	l_type;
	short	l_whence;
	off64_t	l_start;
	off64_t	l_len;		/* len == 0 means until end of file */
	int	l_sysid;
	pid_t	l_pid;
	long	l_pad[4];		/* reserve area */
} flock64_t;

#if defined(_SYSCALL32)

/* Kernel's view of ILP32 flock64 */

typedef struct flock64_32 {
	int16_t	l_type;
	int16_t	l_whence;
	off64_t	l_start;
	off64_t	l_len;		/* len == 0 means until end of file */
	int32_t	l_sysid;
	pid32_t	l_pid;
	int32_t	l_pad[4];		/* reserve area */
} flock64_32_t;

/* Kernel's view of LP64 flock64 */

typedef struct flock64_64 {
	int16_t	l_type;
	int16_t	l_whence;
	off64_t	l_start;
	off64_t	l_len;		/* len == 0 means until end of file */
	int32_t	l_sysid;
	pid32_t	l_pid;
	int64_t	l_pad[4];		/* reserve area */
} flock64_64_t;

#endif	/* _SYSCALL32 */

#endif /* _LARGEFILE64_SOURCE */

#if defined(_KERNEL) || defined(_KMEMUSER)
/* SVr3 flock type; needed for rfs across the wire compatibility */
typedef struct o_flock {
	int16_t	l_type;
	int16_t	l_whence;
	int32_t	l_start;
	int32_t	l_len;		/* len == 0 means until end of file */
	int16_t	l_sysid;
	int16_t	l_pid;
} o_flock_t;
#endif	/* defined(_KERNEL) */

/*
 * File segment locking types.
 */
#define	F_RDLCK		01	/* Read lock */
#define	F_WRLCK		02	/* Write lock */
#define	F_UNLCK		03	/* Remove lock(s) */
#define	F_UNLKSYS	04	/* remove remote locks for a given system */

/*
 * POSIX constants
 */

#define	O_ACCMODE	3	/* Mask for file access modes */
#define	FD_CLOEXEC	1	/* close on exec flag */

/*
 * DIRECTIO
 */
#if defined(__EXTENSIONS__) || \
	(!defined(_XOPEN_SOURCE) && !defined(_POSIX_C_SOURCE))
#define	DIRECTIO_OFF	(0)
#define	DIRECTIO_ON	(1)

/*
 * File share reservation type
 */
typedef struct fshare {
	short	f_access;
	short	f_deny;
	int	f_id;
} fshare_t;

/*
 * f_access values
 */
#define	F_RDACC		0x1	/* Read-only share access */
#define	F_WRACC		0x2	/* Write-only share access */
#define	F_RWACC		0x3	/* Read-Write share access */

/*
 * f_deny values
 */
#define	F_NODNY		0x0	/* Don't deny others access */
#define	F_RDDNY		0x1	/* Deny others read share access */
#define	F_WRDNY		0x2	/* Deny others write share access */
#define	F_RWDNY		0x3	/* Deny others read or write share access */
#define	F_COMPAT	0x8	/* Set share to old DOS compatibility mode */
#endif /* __EXTENSIONS__ || (!_XOPEN_SOURCE && !_POSIX_C_SOURCE) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FCNTL_H */
