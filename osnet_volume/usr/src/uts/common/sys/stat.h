/*
 * Copyright (c) 1995-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_STAT_H
#define	_SYS_STAT_H

#pragma ident	"@(#)stat.h	1.2	99/05/19 SMI"

#include <sys/feature_tests.h>
#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * The implementation specific header <sys/time_impl.h> includes a
 * definition for timestruc_t needed by the stat structure.  However,
 * including either <time.h>, which includes <sys/time_impl.h>, or
 * including <sys/time_impl.h> directly will break both X/Open and
 * POSIX namespace. Preceeding tag, structure, and structure member
 * names with underscores eliminates the namespace breakage and at the
 * same time, with unique type names, eliminates the possibility of
 * timespec_t or timestruct_t naming conflicts that could otherwise
 * result based on the order of inclusion of <sys/stat.h> and
 * <sys/time.h>.  The header <sys/time_std_impl.h> contains the
 * standards namespace safe versions of these definitions.
 */
#if !defined(_XOPEN_SOURCE) && !defined(_POSIX_C_SOURCE) || \
	defined(__EXTENSIONS__)
#include <sys/time_impl.h>
#else
#include <sys/time_std_impl.h>
#endif /* !defined(_XOPEN_SOURCE) && !defined(_POSIX_C_SOURCE) || ... */

#define	_ST_FSTYPSZ 16		/* array size for file system type name */

/*
 * stat structure, used by stat(2) and fstat(2)
 */

#if defined(_KERNEL)

	/* Old SVID stat struct (SVR3.x) */

struct	o_stat {
	o_dev_t		st_dev;
	o_ino_t		st_ino;
	o_mode_t	st_mode;
	o_nlink_t	st_nlink;
	o_uid_t		st_uid;
	o_gid_t		st_gid;
	o_dev_t		st_rdev;
	off32_t		st_size;
	time32_t	st_atime;
	time32_t	st_mtime;
	time32_t	st_ctime;
};

	/* Expanded stat structure */

#if defined(_LP64)

struct stat {
	dev_t		st_dev;
	ino_t		st_ino;
	mode_t		st_mode;
	nlink_t		st_nlink;
	uid_t		st_uid;
	gid_t		st_gid;
	dev_t		st_rdev;
	off_t		st_size;
	timestruc_t	st_atim;
	timestruc_t	st_mtim;
	timestruc_t	st_ctim;
	blksize_t	st_blksize;
	blkcnt_t	st_blocks;
	char		st_fstype[_ST_FSTYPSZ];
};

struct stat64 {
	dev_t		st_dev;
	ino_t		st_ino;
	mode_t		st_mode;
	nlink_t		st_nlink;
	uid_t		st_uid;
	gid_t		st_gid;
	dev_t		st_rdev;
	off_t		st_size;
	timestruc_t	st_atim;
	timestruc_t	st_mtim;
	timestruc_t	st_ctim;
	blksize_t	st_blksize;
	blkcnt_t	st_blocks;
	char		st_fstype[_ST_FSTYPSZ];
};

#else	/* _LP64 */

struct	stat {
	dev_t		st_dev;
	long		st_pad1[3];	/* reserve for dev expansion, */
					/* sysid definition */
	ino_t		st_ino;
	mode_t		st_mode;
	nlink_t		st_nlink;
	uid_t		st_uid;
	gid_t		st_gid;
	dev_t		st_rdev;
	long		st_pad2[2];
	off_t		st_size;
	long		st_pad3;	/* pad for future off_t expansion */
	timestruc_t	st_atim;
	timestruc_t	st_mtim;
	timestruc_t	st_ctim;
	blksize_t	st_blksize;
	blkcnt_t	st_blocks;
	char		st_fstype[_ST_FSTYPSZ];
	long		st_pad4[8];	/* expansion area */
};

struct  stat64 {
	dev_t		st_dev;
	long		st_pad1[3];	/* reserve for dev expansion, */
				/* sysid definition */
	ino64_t		st_ino;
	mode_t		st_mode;
	nlink_t		st_nlink;
	uid_t		st_uid;
	gid_t		st_gid;
	dev_t		st_rdev;
	long		st_pad2[2];
	off64_t		st_size;	/* large file support */
	timestruc_t	st_atim;
	timestruc_t	st_mtim;
	timestruc_t	st_ctim;
	blksize_t	st_blksize;
	blkcnt64_t	st_blocks;	/* large file support */
	char		st_fstype[_ST_FSTYPSZ];
	long		st_pad4[8];	/* expansion area */
};

#endif	/* _LP64 */

#else /* !defined(_KERNEL) */

/*
 * large file compilation environment setup
 */
#if !defined(_LP64) && _FILE_OFFSET_BITS == 64
#ifdef	__PRAGMA_REDEFINE_EXTNAME
#pragma redefine_extname	fstat	fstat64
#pragma redefine_extname	stat	stat64

#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
#pragma	redefine_extname	lstat	lstat64
#endif
#else	/* __PRAGMA_REDEFINE_EXTNAME */
#define	fstat	fstat64
#define	stat	stat64
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
#define	lstat	lstat64
#endif
#endif	/* __PRAGMA_REDEFINE_EXTNAME */
#endif	/* !_LP64 && _FILE_OFFSET_BITS == 64 */

/*
 * In the LP64 compilation environment, map large file interfaces
 * back to native versions where possible.
 */
#if defined(_LP64) && defined(_LARGEFILE64_SOURCE)
#ifdef	__PRAGMA_REDEFINE_EXTNAME
#pragma	redefine_extname	fstat64	fstat
#pragma	redefine_extname	stat64	stat
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
#pragma	redefine_extname	lstat64	lstat
#endif
#else	/* __PRAGMA_REDEFINE_EXTNAME */
#define	fstat64	fstat
#define	stat64	stat
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
#define	lstat64	lstat
#endif
#endif	/* __PRAGMA_REDEFINE_EXTNAME */
#endif	/* _LP64 && _LARGEFILE64_SOURCE */

/*
 * User level stat structure definitions.
 */

#if defined(_LP64)

struct stat {
	dev_t		st_dev;
	ino_t		st_ino;
	mode_t		st_mode;
	nlink_t		st_nlink;
	uid_t		st_uid;
	gid_t		st_gid;
	dev_t		st_rdev;
	off_t		st_size;
#if !defined(_XOPEN_SOURCE) && !defined(_POSIX_C_SOURCE) || \
	defined(__EXTENSIONS__)
	timestruc_t	st_atim;
	timestruc_t	st_mtim;
	timestruc_t	st_ctim;
#else
	_timestruc_t	st_atim;
	_timestruc_t	st_mtim;
	_timestruc_t	st_ctim;
#endif
	blksize_t	st_blksize;
	blkcnt_t	st_blocks;
	char		st_fstype[_ST_FSTYPSZ];
};

#else	/* _LP64 */

struct	stat {
	dev_t		st_dev;
	long		st_pad1[3];	/* reserved for network id */
	ino_t		st_ino;
	mode_t		st_mode;
	nlink_t		st_nlink;
	uid_t		st_uid;
	gid_t		st_gid;
	dev_t		st_rdev;
	long		st_pad2[2];
	off_t		st_size;
#if _FILE_OFFSET_BITS != 64
	long		st_pad3;	/* future off_t expansion */
#endif
#if !defined(_XOPEN_SOURCE) && !defined(_POSIX_C_SOURCE) || \
	defined(__EXTENSIONS__)
	timestruc_t	st_atim;
	timestruc_t	st_mtim;
	timestruc_t	st_ctim;
#else
	_timestruc_t	st_atim;
	_timestruc_t	st_mtim;
	_timestruc_t	st_ctim;
#endif
	blksize_t	st_blksize;
	blkcnt_t	st_blocks;
	char		st_fstype[_ST_FSTYPSZ];
	long		st_pad4[8];	/* expansion area */
};

#endif	/* _LP64 */

/* transitional large file interface version */
#if	defined(_LARGEFILE64_SOURCE) && !((_FILE_OFFSET_BITS == 64) && \
	    !defined(__PRAGMA_REDEFINE_EXTNAME))
#if defined(_LP64)

struct stat64 {
	dev_t		st_dev;
	ino_t		st_ino;
	mode_t		st_mode;
	nlink_t		st_nlink;
	uid_t		st_uid;
	gid_t		st_gid;
	dev_t		st_rdev;
	off_t		st_size;
#if !defined(_XOPEN_SOURCE) && !defined(_POSIX_C_SOURCE) || \
	defined(__EXTENSIONS__)
	timestruc_t	st_atim;
	timestruc_t	st_mtim;
	timestruc_t	st_ctim;
#else
	_timestruc_t	st_atim;
	_timestruc_t	st_mtim;
	_timestruc_t	st_ctim;
#endif
	blksize_t	st_blksize;
	blkcnt_t	st_blocks;
	char		st_fstype[_ST_FSTYPSZ];
};

#else	/* _LP64 */

struct	stat64 {
	dev_t		st_dev;
	long		st_pad1[3];	/* reserved for network id */
	ino64_t		st_ino;
	mode_t		st_mode;
	nlink_t		st_nlink;
	uid_t		st_uid;
	gid_t		st_gid;
	dev_t		st_rdev;
	long		st_pad2[2];
	off64_t		st_size;
#if !defined(_XOPEN_SOURCE) && !defined(_POSIX_C_SOURCE) || \
	defined(__EXTENSIONS__)
	timestruc_t	st_atim;
	timestruc_t	st_mtim;
	timestruc_t	st_ctim;
#else
	_timestruc_t    st_atim;
	_timestruc_t    st_mtim;
	_timestruc_t    st_ctim;
#endif
	blksize_t	st_blksize;
	blkcnt64_t	st_blocks;
	char		st_fstype[_ST_FSTYPSZ];
	long		st_pad4[8];	/* expansion area */
};

#endif	/* _LP64 */
#endif

#if !defined(_XOPEN_SOURCE) && !defined(_POSIX_C_SOURCE) || \
	defined(__EXTENSIONS__)
#define	st_atime	st_atim.tv_sec
#define	st_mtime	st_mtim.tv_sec
#define	st_ctime	st_ctim.tv_sec
#else
#define	st_atime	st_atim.__tv_sec
#define	st_mtime	st_mtim.__tv_sec
#define	st_ctime	st_ctim.__tv_sec
#endif /* !defined(_XOPEN_SOURCE) && !defined(_POSIX_C_SOURCE) || ... */

#endif /* end defined(_KERNEL) */

#if defined(_SYSCALL32)

/*
 * Kernel's view of user ILP32 stat and stat64 structures
 */

struct stat32 {
	dev32_t		st_dev;
	int32_t		st_pad1[3];
	ino32_t		st_ino;
	mode32_t	st_mode;
	nlink32_t	st_nlink;
	uid32_t		st_uid;
	gid32_t		st_gid;
	dev32_t		st_rdev;
	int32_t		st_pad2[2];
	off32_t		st_size;
	int32_t		st_pad3;
	timestruc32_t	st_atim;
	timestruc32_t	st_mtim;
	timestruc32_t	st_ctim;
	int32_t		st_blksize;
	blkcnt32_t	st_blocks;
	char		st_fstype[_ST_FSTYPSZ];
	int32_t		st_pad4[8];
};

#ifdef __ia64
#pragma pack(4)		/* data offset compatibility with ia32 */
#endif
struct stat64_32 {
	dev32_t		st_dev;
	int32_t		st_pad1[3];
	ino64_t		st_ino;
	mode32_t	st_mode;
	nlink32_t	st_nlink;
	uid32_t		st_uid;
	gid32_t		st_gid;
	dev32_t		st_rdev;
	int32_t		st_pad2[2];
	off64_t		st_size;
	timestruc32_t	st_atim;
	timestruc32_t	st_mtim;
	timestruc32_t	st_ctim;
	int32_t		st_blksize;
	blkcnt64_t	st_blocks;
	char		st_fstype[_ST_FSTYPSZ];
	int32_t		st_pad4[8];
};
#ifdef __ia64
#pragma pack()
#endif

#endif	/* _SYSCALL32 */

/* MODE MASKS */

/* de facto standard definitions */

#define	S_IFMT		0xF000	/* type of file */
#define	S_IAMB		0x1FF	/* access mode bits */
#define	S_IFIFO		0x1000	/* fifo */
#define	S_IFCHR		0x2000	/* character special */
#define	S_IFDIR		0x4000	/* directory */
#define	S_IFNAM		0x5000  /* XENIX special named file */
#define	S_INSEM		0x1	/* XENIX semaphore subtype of IFNAM */
#define	S_INSHD		0x2	/* XENIX shared data subtype of IFNAM */
#define	S_IFBLK		0x6000	/* block special */
#define	S_IFREG		0x8000	/* regular */
#define	S_IFLNK		0xA000	/* symbolic link */
#define	S_IFSOCK	0xC000	/* socket */
#define	S_IFDOOR	0xD000	/* door */
#define	S_ISUID		0x800	/* set user id on execution */
#define	S_ISGID		0x400	/* set group id on execution */
#define	S_ISVTX		0x200	/* save swapped text even after use */
#define	S_IREAD		00400	/* read permission, owner */
#define	S_IWRITE	00200	/* write permission, owner */
#define	S_IEXEC		00100	/* execute/search permission, owner */
#define	S_ENFMT		S_ISGID	/* record locking enforcement flag */

/* the following macros are for POSIX conformance */

#define	S_IRWXU		00700	/* read, write, execute: owner */
#define	S_IRUSR		00400	/* read permission: owner */
#define	S_IWUSR		00200	/* write permission: owner */
#define	S_IXUSR		00100	/* execute permission: owner */
#define	S_IRWXG		00070	/* read, write, execute: group */
#define	S_IRGRP		00040	/* read permission: group */
#define	S_IWGRP		00020	/* write permission: group */
#define	S_IXGRP		00010	/* execute permission: group */
#define	S_IRWXO		00007	/* read, write, execute: other */
#define	S_IROTH		00004	/* read permission: other */
#define	S_IWOTH		00002	/* write permission: other */
#define	S_IXOTH		00001	/* execute permission: other */


#define	S_ISFIFO(mode)	(((mode)&0xF000) == 0x1000)
#define	S_ISCHR(mode)	(((mode)&0xF000) == 0x2000)
#define	S_ISDIR(mode)	(((mode)&0xF000) == 0x4000)
#define	S_ISBLK(mode)	(((mode)&0xF000) == 0x6000)
#define	S_ISREG(mode)	(((mode)&0xF000) == 0x8000)
#define	S_ISLNK(mode)	(((mode)&0xF000) == 0xa000)
#define	S_ISSOCK(mode)	(((mode)&0xF000) == 0xc000)
#define	S_ISDOOR(mode)	(((mode)&0xF000) == 0xd000)

/* POSIX.4 macros */
#define	S_TYPEISMQ(_buf)	(0)
#define	S_TYPEISSEM(_buf)	(0)
#define	S_TYPEISSHM(_buf)	(0)

#if defined(i386) || defined(__i386) || (defined(__ia64) && defined(_KERNEL))
/*
 * A version number is included in the x86 SVR4 stat and mknod interfaces
 * so that SVR3 binaries can be supported.  The ia64 kernel needs to be
 * aware of this because it supports x86 applications too.
 */

#define	_R3_MKNOD_VER	1	/* SVR3.0 mknod */
#define	_MKNOD_VER	2	/* current version of mknod */
#define	_R3_STAT_VER	1	/* SVR3.0 stat */
#define	_STAT_VER	2	/* current version of stat */
#endif

#if !defined(_KERNEL)

#if defined(__STDC__)

#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	(_POSIX_C_SOURCE > 2) || defined(_XPG4_2) || \
	defined(__EXTENSIONS__)
extern int fchmod(int, mode_t);
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || ... */

extern int chmod(const char *, mode_t);
extern int mkdir(const char *, mode_t);
extern int mkfifo(const char *, mode_t);
extern mode_t umask(mode_t);

/* transitional large file interfaces */
#if	defined(_LARGEFILE64_SOURCE) && !((_FILE_OFFSET_BITS == 64) && \
	    !defined(__PRAGMA_REDEFINE_EXTNAME))
extern int fstat64(int, struct stat64 *);
extern int stat64(const char *, struct stat64 *);
extern int lstat64(const char *, struct stat64 *);
#endif

#else /* !__STDC__ */

#if !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE) || \
	(_POSIX_C_SOURCE > 2) || defined(_XPG4_2) || \
	defined(__EXTENSIONS__)
extern int fchmod();
#endif /* !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE) || ... */

extern int chmod();
extern int mkdir();
extern int mkfifo();
extern mode_t umask();

/* transitional large file interfaces */
#if	defined(_LARGEFILE64_SOURCE) && !((_FILE_OFFSET_BITS == 64) && \
	    !defined(__PRAGMA_REDEFINE_EXTNAME))
extern int fstat64();
extern int stat64();
extern int lstat64();
#endif

#endif /* defined(__STDC__) */

#include <sys/stat_impl.h>

#endif /* !defined(_KERNEL) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_STAT_H */
