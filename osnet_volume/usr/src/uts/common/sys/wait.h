/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_WAIT_H
#define	_SYS_WAIT_H

#pragma ident	"@(#)wait.h	1.21	97/04/08 SMI"	/* SVr4.0 1.10 */

#include <sys/feature_tests.h>

#include <sys/types.h>

#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
#include <sys/resource.h>	/* Added for XSH4.2 */
#include <sys/siginfo.h>
#include <sys/procset.h>
#endif /* !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)... */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * arguments to wait functions
 */

#define	WUNTRACED	0004	/* wait for processes stopped by signals */
#define	WNOHANG		0100	/* non blocking form of wait	*/


#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
#define	WEXITED		0001	/* wait for processes that have exited	*/
#define	WTRAPPED	0002	/* wait for processes stopped while tracing */
#define	WSTOPPED	WUNTRACED /* backwards compatibility */
#define	WCONTINUED	0010	/* wait for processes continued */
#define	WNOWAIT		0200	/* non destructive form of wait */
#define	_WNOCHLD	0400	/* SunOS private, not for application use */
#define	WOPTMASK (WEXITED|WTRAPPED|WSTOPPED|WCONTINUED|WNOHANG|WNOWAIT|_WNOCHLD)
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */

/*
 * macros for stat return from wait functions
 */

#if !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)

#define	WSTOPFLG		0177
#define	WCONTFLG		0177777
#define	WCOREFLG		0200
#define	WSIGMASK		0177

#define	WLOBYTE(stat)		((int)((stat)&0377))
#define	WHIBYTE(stat)		((int)(((stat)>>8)&0377))
#define	WWORD(stat)		((int)((stat))&0177777)

#define	WIFCONTINUED(stat)	(WWORD(stat) == WCONTFLG)
#define	WCOREDUMP(stat)		((stat)&WCOREFLG)

#endif /* !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)... */

#define	WIFEXITED(stat)		((int)((stat)&0xFF) == 0)
#define	WIFSIGNALED(stat)	((int)((stat)&0xFF) > 0 && \
				    (int)((stat)&0xFF00) == 0)
#define	WIFSTOPPED(stat)	((int)((stat)&0xFF) == 0177 && \
				    (int)((stat)&0xFF00) != 0)
#define	WEXITSTATUS(stat)	((int)(((stat)>>8)&0xFF))
#define	WTERMSIG(stat)		((int)((stat)&0x7F))
#define	WSTOPSIG(stat)		((int)(((stat)>>8)&0xFF))


#if !defined(_KERNEL)
#if defined(__STDC__)

extern pid_t wait(int *);
extern pid_t waitpid(pid_t, int *, int);
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
extern int waitid(idtype_t, id_t, siginfo_t *, int);
extern pid_t wait3(int *, int, struct rusage *);
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */

#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern pid_t wait4(pid_t, int *, int, struct rusage *);
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */

#else /* __STDC__ */

extern pid_t wait();
extern pid_t waitpid();
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
extern int waitid();
extern pid_t wait3();
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */

#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
extern pid_t wait4();
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */

#endif	/* __STDC__ */
#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_WAIT_H */
