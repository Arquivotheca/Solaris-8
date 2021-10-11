/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _UNISTD_H
#define	_UNISTD_H

#pragma ident	"@(#)unistd.h	1.58	99/11/11 SMI"	/* SVr4.0 1.26	*/

#include <sys/feature_tests.h>

#include <sys/types.h>
#include <sys/unistd.h>

#ifdef	__cplusplus
extern "C" {
#endif

/* Symbolic constants for the "access" routine: */
#define	R_OK	4	/* Test for Read permission */
#define	W_OK	2	/* Test for Write permission */
#define	X_OK	1	/* Test for eXecute permission */
#define	F_OK	0	/* Test for existence of File */

#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
#define	F_ULOCK	0	/* Unlock a previously locked region */
#define	F_LOCK	1	/* Lock a region for exclusive use */
#define	F_TLOCK	2	/* Test and lock a region for exclusive use */
#define	F_TEST	3	/* Test a region for other processes locks */
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */

/* Symbolic constants for the "lseek" routine: */

#ifndef	SEEK_SET
#define	SEEK_SET	0	/* Set file pointer to "offset" */
#endif

#ifndef	SEEK_CUR
#define	SEEK_CUR	1	/* Set file pointer to current plus "offset" */
#endif

#ifndef	SEEK_END
#define	SEEK_END	2	/* Set file pointer to EOF plus "offset" */
#endif

#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
/* Path names: */
#define	GF_PATH	"/etc/group"	/* Path name of the "group" file */
#define	PF_PATH	"/etc/passwd"	/* Path name of the "passwd" file */
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */

/*
 * compile-time symbolic constants,
 * Support does not mean the feature is enabled.
 * Use pathconf/sysconf to obtain actual configuration value.
 */
#define	_POSIX_ASYNC_IO			1
#define	_POSIX_ASYNCHRONOUS_IO		1
#define	_POSIX_FSYNC			1
#define	_POSIX_JOB_CONTROL		1
#define	_POSIX_MAPPED_FILES		1
#define	_POSIX_MEMLOCK			1
#define	_POSIX_MEMLOCK_RANGE		1
#define	_POSIX_MEMORY_PROTECTION	1
#define	_POSIX_MESSAGE_PASSING		1
#define	_POSIX_PRIORITY_SCHEDULING	1
#define	_POSIX_REALTIME_SIGNALS		1
#define	_POSIX_SAVED_IDS		1
#define	_POSIX_SEMAPHORES		1
#define	_POSIX_SHARED_MEMORY_OBJECTS	1
#define	_POSIX_SYNC_IO			1
#define	_POSIX_SYNCHRONIZED_IO		1
#define	_POSIX_TIMERS			1
/*
 * POSIX.4a compile-time symbolic constants.
 */
#define	_POSIX_THREAD_SAFE_FUNCTIONS	1
#define	_POSIX_THREADS			1
#define	_POSIX_THREAD_ATTR_STACKADDR	1
#define	_POSIX_THREAD_ATTR_STACKSIZE	1
#define	_POSIX_THREAD_PROCESS_SHARED	1
#define	_POSIX_THREAD_PRIORITY_SCHEDULING	1

/*
 * Support for the POSIX.1 mutex protocol attribute. For realtime applications
 * which need mutexes to support priority inheritance/ceiling.
 */
#define	_POSIX_THREAD_PRIO_INHERIT	1
#define	_POSIX_THREAD_PRIO_PROTECT	1

#ifndef _POSIX_VDISABLE
#define	_POSIX_VDISABLE		0
#endif

#ifndef NULL
#if defined(_LP64) && !defined(__cplusplus)
#define	NULL	0L
#else
#define	NULL	0
#endif
#endif

#define	STDIN_FILENO	0
#define	STDOUT_FILENO	1
#define	STDERR_FILENO	2

/*
 * Large File Summit-related announcement macros.  The system supports both
 * the additional and transitional Large File Summit interfaces.  (The final
 * two macros provide a finer granularity breakdown of _LFS64_LARGEFILE.)
 */
#define	_LFS_LARGEFILE		1
#define	_LFS64_LARGEFILE	1
#define	_LFS64_STDIO		1
#define	_LFS64_ASYNCHRONOUS_IO	1

/* large file compilation environment setup */
#if !defined(_LP64) && _FILE_OFFSET_BITS == 64
#ifdef	__PRAGMA_REDEFINE_EXTNAME
#pragma redefine_extname	ftruncate	ftruncate64
#pragma redefine_extname	lseek		lseek64
#pragma redefine_extname	pread		pread64
#pragma redefine_extname	pwrite		pwrite64
#pragma redefine_extname	truncate	truncate64
#pragma redefine_extname	lockf		lockf64
#pragma	redefine_extname	tell		tell64
#else	/* __PRAGMA_REDEFINE_EXTNAME */
#define	ftruncate			ftruncate64
#define	lseek				lseek64
#define	pread				pread64
#define	pwrite				pwrite64
#define	truncate			truncate64
#define	lockf				lockf64
#define	tell				tell64
#endif	/* __PRAGMA_REDEFINE_EXTNAME */
#endif	/* !_LP64 && _FILE_OFFSET_BITS == 64 */

/* In the LP64 compilation environment, the APIs are already large file */
#if defined(_LP64) && defined(_LARGEFILE64_SOURCE)
#ifdef	__PRAGMA_REDEFINE_EXTNAME
#pragma redefine_extname	ftruncate64	ftruncate
#pragma redefine_extname	lseek64		lseek
#pragma redefine_extname	pread64		pread
#pragma redefine_extname	pwrite64	pwrite
#pragma redefine_extname	truncate64	truncate
#pragma redefine_extname	lockf64		lockf
#pragma redefine_extname	tell64		tell
#else	/* __PRAGMA_REDEFINE_EXTNAME */
#define	ftruncate64			ftruncate
#define	lseek64				lseek
#define	pread64				pread
#define	pwrite64			pwrite
#define	truncate64			truncate
#define	lockf64				lockf
#define	tell64				tell
#endif	/* __PRAGMA_REDEFINE_EXTNAME */
#endif	/* _LP64 && _LARGEFILE64_SOURCE */

#if defined(__STDC__)

extern int access(const char *, int);
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern int acct(const char *);
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
extern unsigned alarm(unsigned);
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
extern int brk(void *);
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
extern int chdir(const char *);
extern int chown(const char *, uid_t, gid_t);
#if (!defined(_POSIX_C_SOURCE) || defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern int chroot(const char *);
#endif /* (!defined(_POSIX_C_SOURCE) || defined(_XOPEN_SOURCE))... */
extern int close(int);
#if (defined(_XOPEN_SOURCE) && (_XOPEN_VERSION - 0 >= 4)) || \
	defined(__EXTENSIONS__)
extern size_t confstr(int, char *, size_t);
extern char *crypt(const char *, const char *);
#endif /* (defined(XOPEN_SOURCE) && (_XOPEN_VERSION - 0 >= 4))... */
#if !defined(_POSIX_C_SOURCE) || defined(_XOPEN_SOURCE) || \
	defined(__EXTENSIONS__)
extern char *ctermid(char *);
#endif /* (!defined(_POSIX_C_SOURCE) ... */
#ifdef _REENTRANT
extern char *ctermid_r(char *);
#endif /* _REENTRANT */
extern char *cuserid(char *);
extern int dup(int);
extern int dup2(int, int);
#if (defined(_XOPEN_SOURCE) && (_XOPEN_VERSION - 0 >= 4)) || \
	defined(__EXTENSIONS__)
extern void encrypt(char *, int);
#endif /* (defined(XOPEN_SOURCE) && (_XOPEN_VERSION - 0 >= 4))... */
extern int execl(const char *, const char *, ...);
extern int execle(const char *, const char *, ...);
extern int execlp(const char *, const char *, ...);
extern int execv(const char *, char *const *);
extern int execve(const char *, char *const *, char *const *);
extern int execvp(const char *, char *const *);
extern void _exit(int);
/*
 * The following fattach prototype is duplicated in <stropts.h>. The
 * duplication is necessitated by XPG4.2 which requires the prototype
 * be defined in <stropts.h>.
 */
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern int fattach(int, const char *);
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
extern int fchdir(int);
extern int fchown(int, uid_t, gid_t);
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern int fchroot(int);
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
#if !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE) || \
	(_POSIX_C_SOURCE > 2) || defined(__EXTENSIONS__)
extern int fdatasync(int);
#endif /* (!defined(_POSIX_C_SOURCE) && ! defined(_XOPEN_SOURCE))... */
/*
 * The following fdetach prototype is duplicated in <stropts.h>. The
 * duplication is necessitated by XPG4.2 which requires the prototype
 * be defined in <stropts.h>.
 */
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern int fdetach(const char *);
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
extern pid_t fork(void);
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern pid_t fork1(void);
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
extern long fpathconf(int, int);
#if !defined(_POSIX_C_SOURCE) || (_POSIX_C_SOURCE > 2) || \
	defined(__EXTENSIONS__)
extern int fsync(int);
#endif /* !defined(_POSIX_C_SOURCE) || (_POSIX_C_SOURCE > 2)... */
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	(defined(_LARGEFILE_SOURCE) && _FILE_OFFSET_BITS == 64) || \
	(_POSIX_C_SOURCE > 2) || defined(_XPG4_2) || defined(__EXTENSIONS__)
extern int ftruncate(int, off_t);
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
extern char *getcwd(char *, size_t);
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
extern int getdtablesize(void);
#endif
extern gid_t getegid(void);
extern uid_t geteuid(void);
extern gid_t getgid(void);
extern int getgroups(int, gid_t *);
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
extern long gethostid(void);
#endif
#if defined(_XPG4_2)
extern int gethostname(char *, size_t);
#elif  defined(__EXTENSIONS__) || \
	(!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))
extern int gethostname(char *, int);
#endif
extern char *getlogin(void);
#if (defined(_XOPEN_SOURCE) && (_XOPEN_VERSION - 0 >= 4)) || \
	defined(__EXTENSIONS__)
extern int  getopt(int, char *const *, const char *);
extern char *optarg;
extern int  opterr, optind, optopt;
extern char *getpass(const char *);
#endif /* (defined(_XOPEN_SOURCE) && (_XOPEN_VERSION - 0 >= 4))... */
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
extern int getpagesize(void);
extern pid_t getpgid(pid_t);
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
extern pid_t getpid(void);
extern pid_t getppid(void);
extern pid_t getpgrp(void);
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
char *gettxt(const char *, const char *);
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
extern pid_t getsid(pid_t);
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
extern uid_t getuid(void);
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
extern char *getwd(char *);
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
/*
 * The following ioctl prototype is duplicated in <stropts.h>. The
 * duplication is necessitated by XPG4.2 which requires the prototype
 * be defined in <stropts.h>.
 */
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern int ioctl(int, int, ...);
#endif
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern int isaexec(const char *, char *const *, char *const *);
#endif
extern int isatty(int);
extern int link(const char *, const char *);
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
extern int lchown(const char *, uid_t, gid_t);
#endif
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__) || \
	(defined(_LARGEFILE_SOURCE) && _FILE_OFFSET_BITS == 64)
extern int lockf(int, int, off_t);
#endif
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
extern int readlink(const char *, char *, size_t);
#endif
extern off_t lseek(int, off_t, int);
#if (!defined(_POSIX_C_SOURCE) || defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern int nice(int);
#endif /* (!defined(_POSIX_C_SOURCE) || defined(_XOPEN_SOURCE))... */
extern long pathconf(const char *, int);
extern int pause(void);
extern int pipe(int *);
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern offset_t llseek(int, offset_t, int);
#endif
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	(defined(_LARGEFILE_SOURCE) && _FILE_OFFSET_BITS == 64) || \
	defined(__EXTENSIONS__)
extern off_t tell(int);
#endif
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern int mincore(caddr_t, size_t, char *);
#endif
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	(defined(_LARGEFILE_SOURCE) && _FILE_OFFSET_BITS == 64) || \
	defined(_XPG5) || defined(__EXTENSIONS__)
extern ssize_t pread(int, void *, size_t, off_t);
#endif
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern void profil(unsigned short *, size_t, unsigned long, unsigned int);
#endif
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	(_POSIX_C_SOURCE > 2) || defined(__EXTENSIONS__)
extern int pthread_atfork(void (*) (void), void (*) (void), void (*) (void));
#endif
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern long ptrace(int, pid_t, long, long);
#endif
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	(defined(_LARGEFILE_SOURCE) && _FILE_OFFSET_BITS == 64) || \
	defined(_XPG5) || defined(__EXTENSIONS__)
extern ssize_t pwrite(int, const void *, size_t, off_t);
#endif
extern ssize_t read(int, void *, size_t);
#if (!defined(_POSIX_C_SOURCE) && (_XOPEN_VERSION - 0 < 4)) || \
	defined(__EXTENSIONS__)
extern int rename(const char *, const char *);
#endif /* (!defined(_POSIX_C_SOURCE) && (_XOPEN_VERSION - 0 < 4))... */
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern int resolvepath(const char *, char *, size_t);
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
extern int rmdir(const char *);
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
extern void *sbrk(intptr_t);
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
extern int setgid(gid_t);
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern int setegid(gid_t);
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern int setgroups(int, const gid_t *);
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
extern int setpgid(pid_t, pid_t);
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
extern pid_t setpgrp(void);
extern int setregid(gid_t, gid_t);
extern int setreuid(uid_t, uid_t);
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
extern pid_t setsid(void);
extern int setuid(uid_t);
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern int seteuid(uid_t);
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
extern unsigned sleep(unsigned);
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern int stime(const time_t *);
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
#if defined(_XOPEN_SOURCE) && (_XOPEN_VERSION - 0 >= 4)
/* __EXTENSIONS__ makes the SVID Third Edition prototype in stdlib.h visible */
extern void swab(const void *, void *, ssize_t);
#endif /* defined(_XOPEN_SOURCE) && (_XOPEN_VERSION - 0 >= 4) */
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
extern int symlink(const char *, const char *);
extern void sync(void);
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
extern long sysconf(int);
#if defined(_XPG5)
#ifdef __PRAGMA_REDEFINE_EXTNAME
#pragma redefine_extname sysconf __sysconf_xpg5
#else /* __PRAGMA_REDEFINE_EXTNAME */
extern long __sysconf_xpg5(int);
#define	sysconf __sysconf_xpg5
#endif  /* __PRAGMA_REDEFINE_EXTNAME */
#endif /* defined(_XPG5) */
extern pid_t tcgetpgrp(int);
extern int tcsetpgrp(int, pid_t);
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern off_t tell(int);
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	(defined(_LARGEFILE_SOURCE) && _FILE_OFFSET_BITS == 64) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
extern int truncate(const char *, off_t);
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
extern char *ttyname(int);
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
extern useconds_t ualarm(useconds_t, useconds_t);
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
extern int unlink(const char *);
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
extern int usleep(useconds_t);
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
extern pid_t vfork(void);
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern void vhangup(void);
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
extern ssize_t write(int, const void *, size_t);
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern void yield(void);
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */

/* transitional large file interface versions */
#if	defined(_LARGEFILE64_SOURCE) && !((_FILE_OFFSET_BITS == 64) && \
	    !defined(__PRAGMA_REDEFINE_EXTNAME))
extern int ftruncate64(int, off64_t);
extern off64_t lseek64(int, off64_t, int);
extern ssize_t	pread64(int, void *, size_t, off64_t);
extern ssize_t	pwrite64(int, const void *, size_t, off64_t);
extern off64_t	tell64(int);
extern int	truncate64(const char *, off64_t);
extern int	lockf64(int, int, off64_t);
#endif	/* _LARGEFILE64_SOURCE */

#else  /* __STDC__ */

extern int access();
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern int acct();
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
extern unsigned alarm();
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
extern int brk();
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
extern int chdir();
extern int chown();
#if (!defined(_POSIX_C_SOURCE) || defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern int chroot();
#endif /* (!defined(_POSIX_C_SOURCE) || defined(_XOPEN_SOURCE))... */
extern int close();
#if (defined(_XOPEN_SOURCE) && (_XOPEN_VERSION - 0 >= 4)) || \
	defined(__EXTENSIONS__)
extern size_t confstr();
extern char *crypt();
#endif /* (defined(XOPEN_SOURCE) && (_XOPEN_VERSION - 0 >= 4))... */
#if !defined(_POSIX_C_SOURCE) || defined(_XOPEN_SOURCE) || \
	defined(__EXTENSIONS__)
extern char *ctermid();
#endif /* (!defined(_POSIX_C_SOURCE) ... */
#ifdef _REENTRANT
extern char *ctermid_r();
#endif /* _REENTRANT */
extern char *cuserid();
extern int dup();
extern int dup2();
#if (defined(_XOPEN_SOURCE) && (_XOPEN_VERSION - 0 >= 4)) || \
	defined(__EXTENSIONS__)
extern void encrypt();
#endif /* (defined(_XOPEN_SOURCE) && (_XOPEN_VERSION - 0 >= 4))... */
extern int execl();
extern int execle();
extern int execlp();
extern int execv();
extern int execve();
extern int execvp();
extern void _exit();
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern int fattach();
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
extern int fchdir();
extern int fchown();
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) ... */
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern int fchroot();
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
#if !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE) || \
	(_POSIX_C_SOURCE > 2) || defined(__EXTENSIONS__)
extern int fdatasync();
#endif /* (!defined(_POSIX_C_SOURCE) && ! defined(_XOPEN_SOURCE))... */
#if !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)
extern int fdetach();
#endif /* !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE) */
extern pid_t fork();
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern pid_t fork1();
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
extern long fpathconf();
#if !defined(_POSIX_C_SOURCE) || (_POSIX_C_SOURCE > 2) || \
	defined(__EXTENSIONS__)
extern int fsync();
#endif /* !defined(_POSIX_C_SOURCE) || (_POSIX_C_SOURCE > 2)... */
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	(defined(_LARGEFILE_SOURCE) && _FILE_OFFSET_BITS == 64) || \
	(_POSIX_C_SOURCE > 2) || defined(_XPG4_2) || defined(__EXTENSIONS__)
extern int ftruncate();
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
extern char *getcwd();
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
extern int getdtablesize();
#endif
extern gid_t getegid();
extern uid_t geteuid();
extern gid_t getgid();
extern int getgroups();
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
extern long gethostid();
#endif
extern char *getlogin();
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
#if (defined(_XOPEN_SOURCE) && (_XOPEN_VERSION - 0 >= 4)) || \
	defined(__EXTENSIONS__)
extern int  getopt();
extern char *optarg;
extern int  opterr, optind, optopt;
extern char *getpass();
#endif /* (defined(_XOPEN_SOURCE) && (_XOPEN_VERSION - 0 >= 4))... */
extern int getpagesize();
extern pid_t getpgid();
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
extern pid_t getpid();
extern pid_t getppid();
extern pid_t getpgrp();
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
char *gettxt();
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
extern pid_t getsid();
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
extern uid_t getuid();
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
extern char *getwd();
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern int ioctl();
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern int isaexec();
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
extern int isatty();
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
extern int lchown();
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
extern int link();
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern offset_t llseek();
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__) || \
	(defined(_LARGEFILE_SOURCE) && _FILE_OFFSET_BITS == 64)
extern int lockf();
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
extern off_t lseek();
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern int mincore();
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
#if (!defined(_POSIX_C_SOURCE) || defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern int nice();
#endif /* (!defined(_POSIX_C_SOURCE) || defined(_XOPEN_SOURCE))... */
extern long pathconf();
extern int pause();
extern int pipe();
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	(defined(_LARGEFILE_SOURCE) && _FILE_OFFSET_BITS == 64) || \
	defined(_XPG5) || defined(__EXTENSIONS__)
extern ssize_t pread();
#endif
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern void profil();
extern long ptrace();
#endif
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	(_POSIX_C_SOURCE > 2) || defined(__EXTENSIONS__)
extern int pthread_atfork();
#endif
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	(defined(_LARGEFILE_SOURCE) && _FILE_OFFSET_BITS == 64) || \
	defined(_XPG5) || defined(__EXTENSIONS__)
extern ssize_t pwrite();
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
extern ssize_t read();
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
extern int readlink();
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
#if (!defined(_POSIX_C_SOURCE) && (_XOPEN_VERSION - 0 < 4)) || \
	defined(__EXTENSIONS__)
extern int rename();
#endif /* (!defined(_POSIX_C_SOURCE) && (_XOPEN_VERSION - 0 < 4))... */
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern int resolvepath();
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
extern int rmdir();
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
extern void *sbrk();
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
extern int setgid();
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern int setegid();
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern int setgroups();
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
extern int setpgid();
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
extern pid_t setpgrp();
extern int setregid();
extern int setreuid();
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
extern pid_t setsid();
extern int setuid();
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern int seteuid();
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
extern unsigned sleep();
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern int stime();
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
#if defined(_XOPEN_SOURCE) && (_XOPEN_VERSION - 0 >= 4)
/* __EXTENSIONS__ makes the SVID Third Edition prototype in stdlib.h visible */
extern void swab();
#endif /* defined(_XOPEN_SOURCE) && (_XOPEN_VERSION - 0 >= 4) */
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
extern int symlink();
extern void sync();
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
extern long sysconf();
#if defined(_XPG5)
#ifdef __PRAGMA_REDEFINE_EXTNAME
#pragma redefine_extname sysconf __sysconf_xpg5
#else /* __PRAGMA_REDEFINE_EXTNAME */
extern long __sysconf_xpg5();
#define	sysconf __sysconf_xpg5
#endif  /* __PRAGMA_REDEFINE_EXTNAME */
#endif	/* defined(_XPG5) */
extern pid_t tcgetpgrp();
extern int tcsetpgrp();
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	(defined(_LARGEFILE_SOURCE) && _FILE_OFFSET_BITS == 64) || \
	defined(__EXTENSIONS__)
extern off_t tell();
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	(defined(_LARGEFILE_SOURCE) && _FILE_OFFSET_BITS == 64) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
extern int truncate();
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
extern char *ttyname();
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
extern useconds_t ualarm();
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
extern int unlink();
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
extern int usleep();
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */
#if !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
extern pid_t vfork();
#endif /* !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE) */
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern void vhangup();
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) ... */
extern ssize_t write();
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern void yield();
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */

/* transitional large file interface versions */
#if	defined(_LARGEFILE64_SOURCE) && !((_FILE_OFFSET_BITS == 64) && \
	    !defined(__PRAGMA_REDEFINE_EXTNAME))
extern int ftruncate64();
extern off64_t lseek64();
extern ssize_t pread64();
extern ssize_t pwrite64();
extern off64_t tell64();
extern int truncate64();
extern int lockf64();
#endif	/* _LARGEFILE64_SOURCE */

#endif /* __STDC__ */

/*
 * This atrocity is necessary on sparc because registers modified
 * by the child get propagated back to the parent via the window
 * save/restore mechanism.
 */
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
#if defined(__sparc)
#pragma unknown_control_flow(vfork)
#endif
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */

/*
 * getlogin_r() & ttyname_r() prototypes are defined here.
 */

/*
 * Previous releases of Solaris, starting at 2.3, provided definitions of
 * various functions as specified in POSIX.1c, Draft 6.  For some of these
 * functions, the final POSIX 1003.1c standard had a different number of
 * arguments and return values.
 *
 * The following segment of this header provides support for the standard
 * interfaces while supporting applications written under earlier
 * releases.  The application defines appropriate values of the feature
 * test macros _POSIX_C_SOURCE and _POSIX_PTHREAD_SEMANTICS to indicate
 * whether it was written to expect the Draft 6 or standard versions of
 * these interfaces, before including this header.  This header then
 * provides a mapping from the source version of the interface to an
 * appropriate binary interface.  Such mappings permit an application
 * to be built from libraries and objects which have mixed expectations
 * of the definitions of these functions.
 *
 * For applications using the Draft 6 definitions, the binary symbol is
 * the same as the source symbol, and no explicit mapping is needed.  For
 * the standard interface, the function func() is mapped to the binary
 * symbol _posix_func().  The preferred mechanism for the remapping is a
 * compiler #pragma.  If the compiler does not provide such a #pragma, the
 * header file defines a static function func() which calls the
 * _posix_func() version; this is required if the application needs to
 * take the address of func().
 *
 * NOTE: Support for the Draft 6 definitions is provided for compatibility
 * only.  New applications/libraries should use the standard definitions.
 */

#if	defined(__EXTENSIONS__) || defined(_REENTRANT) || \
	(_POSIX_C_SOURCE - 0 >= 199506L) || defined(_POSIX_PTHREAD_SEMANTICS)

#if	defined(__STDC__)

#if	(_POSIX_C_SOURCE - 0 >= 199506L) || defined(_POSIX_PTHREAD_SEMANTICS)

#ifdef __PRAGMA_REDEFINE_EXTNAME
extern int getlogin_r(char *, int);
extern int ttyname_r(int, char *, size_t);
#pragma redefine_extname getlogin_r __posix_getlogin_r
#pragma redefine_extname ttyname_r __posix_ttyname_r
#else  /* __PRAGMA_REDEFINE_EXTNAME */

static int
getlogin_r(char *__name, int __len)
{
	extern int __posix_getlogin_r(char *, int);
	return (__posix_getlogin_r(__name, __len));
}
static int
ttyname_r(int __fildes, char *__buf, size_t __size)
{
	extern int __posix_ttyname_r(int, char *, size_t);
	return (__posix_ttyname_r(__fildes, __buf, __size));
}
#endif /* __PRAGMA_REDEFINE_EXTNAME */

#else  /* (_POSIX_C_SOURCE - 0 >= 199506L) || ... */

extern char *getlogin_r(char *, int);
extern char *ttyname_r(int, char *, int);

#endif /* (_POSIX_C_SOURCE - 0 >= 199506L) || ... */

#else  /* __STDC__ */

#if (_POSIX_C_SOURCE - 0 >= 199506L) || defined(_POSIX_PTHREAD_SEMANTICS)

#ifdef __PRAGMA_REDEFINE_EXTNAME
extern int getlogin_r();
extern int ttyname_r();
#pragma redefine_extname getlogin_r __posix_getlogin_r
#pragma redefine_extname ttyname_r __posix_ttyname_r
#else  /* __PRAGMA_REDEFINE_EXTNAME */

static int
getlogin_r(__name, __len)
	char *__name;
	int __len;
{
	extern int __posix_getlogin_r();
	return (__posix_getlogin_r(__name, __len));
}
static int
ttyname_r(__fildes, __buf, __size)
	int __fildes;
	char *__buf;
	size_t __size;
{
	extern int __posix_ttyname_r();
	return (__posix_ttyname_r(__fildes, __buf, __size));
}
#endif /* __PRAGMA_REDEFINE_EXTNAME */

#else  /* (_POSIX_C_SOURCE - 0 >= 199506L) || ... */

extern char *getlogin_r();
extern char *ttyname_r();

#endif /* (_POSIX_C_SOURCE - 0 >= 199506L) || ... */

#endif /* __STDC__ */

#endif /* defined(__EXTENSIONS__) || (__STDC__ == 0 ... */

#ifdef	__cplusplus
}
#endif

#endif /* _UNISTD_H */
