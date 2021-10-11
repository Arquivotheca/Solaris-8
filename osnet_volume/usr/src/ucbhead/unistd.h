/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 */

#ifndef _UNISTD_H
#define	_UNISTD_H

#pragma ident	"@(#)unistd.h	1.6	97/06/13 SMI"	/* SVr4.0 1.1	*/

#include <sys/fcntl.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Symbolic constants for the "access" routine: */
#define	R_OK	4	/* Test for Read permission */
#define	W_OK	2	/* Test for Write permission */
#define	X_OK	1	/* Test for eXecute permission */
#define	F_OK	0	/* Test for existence of File */

#define	F_ULOCK	0	/* Unlock a previously locked region */
#define	F_LOCK	1	/* Lock a region for exclusive use */
#define	F_TLOCK	2	/* Test and lock a region for exclusive use */
#define	F_TEST	3	/* Test a region for other processes locks */


/* Symbolic constants for the "lseek" routine: */
#define	SEEK_SET	0	/* Set file pointer to "offset" */
#define	SEEK_CUR	1	/* Set file pointer to current plus "offset" */
#define	SEEK_END	2	/* Set file pointer to EOF plus "offset" */

/* Path names: */
#define	GF_PATH	"/etc/group"	/* Path name of the "group" file */
#define	PF_PATH	"/etc/passwd"	/* Path name of the "passwd" file */


/* command names for POSIX sysconf */
#define	_SC_ARG_MAX	1
#define	_SC_CHILD_MAX	2
#define	_SC_CLK_TCK	3
#define	_SC_NGROUPS_MAX 4
#define	_SC_OPEN_MAX	5
#define	_SC_JOB_CONTROL	6
#define	_SC_SAVED_IDS	7
#define	_SC_VERSION	8
#define	_SC_PASS_MAX	9
#define	_SC_LOGNAME_MAX	10
#define	_SC_PAGESIZE	11
#define	_SC_XOPEN_VERSION	12
/* 13 reserved for SVr4-ES/MP _SC_NACLS_MAX */
/* 14 reserved for SVr4-ES/MP _SC_NPROC_CONF */
/* 15 reserved for SVr4-ES/MP _SC_NPROC_ONLN */
#define	_SC_STREAM_MAX	16
#define	_SC_TZNAME_MAX	17

/* command names for POSIX pathconf */

#define	_PC_LINK_MAX	1
#define	_PC_MAX_CANON	2
#define	_PC_MAX_INPUT	3
#define	_PC_NAME_MAX	4
#define	_PC_PATH_MAX	5
#define	_PC_PIPE_BUF	6
#define	_PC_NO_TRUNC	7
#define	_PC_VDISABLE	8
#define	_PC_CHOWN_RESTRICTED	9
#define	_PC_LAST	9

/*
 * compile-time symbolic constants,
 * Support does not mean the feature is enabled.
 * Use pathconf/sysconf to obtain actual configuration value.
 */

#define	_POSIX_JOB_CONTROL	1
#define	_POSIX_SAVED_IDS	1

#ifndef _POSIX_VDISABLE
#define	_POSIX_VDISABLE		0
#endif

#define	STDIN_FILENO	0
#define	STDOUT_FILENO	1
#define	STDERR_FILENO	2

/* Current version of POSIX */
#define	_POSIX_VERSION		199009L

/* large file compilation environment setup */
#if !defined(_LP64) && _FILE_OFFSET_BITS == 64
#ifdef __PRAGMA_REDEFINE_EXTNAME
#pragma	redefine_extname	lseek	lseek64
#else
#define	lseek			lseek64
#endif
#endif  /* !_LP64 && _FILE_OFFSET_BITS == 64 */

#if defined(_LP64) && defined(_LARGEFILE64_SOURCE)
#ifdef __PRAGMA_REDEFINE_EXTNAME
#pragma	redefine_extname	lseek64	lseek
#else
#define	lseek64			lseek
#endif
#endif  /* _LP64 && _LARGEFILE64_SOURCE */

#if defined(__STDC__)
extern pid_t getpid(void);
extern pid_t getppid(void);
extern pid_t getpgrp(void);
extern uid_t getuid(void);
extern int setpgid(pid_t, pid_t);
extern int setpgrp(pid_t, pid_t);	/* BSD */

extern int stime(const time_t *);

extern long pathconf(const char *, int);
extern long sysconf(int);

extern char *getwd(char *);
extern long gethostid(void);

extern ssize_t read(int, void *, size_t);
extern ssize_t write(int, const void *, size_t);
extern int ioctl(int, int, ...);
extern int close(int);
extern off_t lseek(int, off_t, int);
#else
extern off_t lseek();
#endif

#if defined(_LARGEFILE64_SOURCE) && !((_FILE_OFFSET_BITS == 64) && \
	!defined(__PRAGMA_REDEFINE_EXTNAME))
#if defined(__STDC__)
extern off64_t lseek64(int, off64_t, int);
#else
extern off64_t lseek64();
#endif
#endif  /* _LARGEFILE64_SOURCE... */

#ifdef __cplusplus
}
#endif

#endif	/* _UNISTD_H */
