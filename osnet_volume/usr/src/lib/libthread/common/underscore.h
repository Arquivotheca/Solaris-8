/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ifndef _UNDERSCORE_H
#define	_UNDERSCORE_H

#pragma ident	"@(#)underscore.h	1.12	99/10/25 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * underscore.h:
 *	Function declarations for those defined in libc that are not
 *	declared extern in a header file. ie those preceeded by '_'.
 */

int	_lock_try(uchar_t *);
int	__sigaction(int, const struct sigaction *, struct sigaction *);
int	_sigemptyset(sigset_t *);
int	_sigaddset(sigset_t *, int);
int	__sigprocmask(int, sigset_t *, sigset_t *);
int	_sigfillset(sigset_t *);
int	_sigdelset(sigset_t *, int);
int	_sigismember(const sigset_t *, int);
int	_signotifywait();
void	__mt_sigpending(sigset_t *);

int	_gettimeofday(struct timeval *, struct timezone *);
int	__setitimer(int, const struct itimerval *, struct itimerval *);
int	_mprotect(void *, size_t, int);
int	_munmap(void *, size_t);
void *	_memset(void *, int, size_t);
void *	_memcpy(void *, const void *, size_t);
void *	_sbrk(int);
int	_open(caddr_t, int);
void	_close(int);
pid_t	_getpid();
ssize_t	_write(int, void *, size_t);

int	_lwp_sigredirect(int, int, int *);
int	_lwp_sigtimedwait(const sigset_t *, siginfo_t *,
				const struct timespec *, int *);
int	___lwp_cond_wait(lwp_cond_t *, lwp_mutex_t *, timestruc_t *);
void	_xregs_clrptr(ucontext_t *);

int	_getcontext(ucontext_t *);
int	_getrlimit(int, struct rlimit *);
int	__setcontext(const ucontext_t *);
int	__getcontext(ucontext_t *);
pid_t	__fork1(void);
pid_t	__fork(void);
void	_yield(void);
void	_libc_prepare_atfork(void);
void	_libc_parent_atfork(void);
void	_libc_child_atfork(void);
int	___lwp_mutex_lock(mutex_t *mp);
int	___lwp_mutex_trylock(mutex_t *mp);
int	___lwp_mutex_wakeup(mutex_t *mp);
int	___lwp_mutex_unlock(mutex_t *mp);
int	___lwp_mutex_init(mutex_t *mp, int type);
extern void *_dlopen(const char *, int);
extern void *_dlsym(void *, const char *);
extern int _dlclose(void *);
extern uid_t _geteuid(void);
extern int _pthread_mutex_lock(pthread_mutex_t *mp);
extern int _pthread_mutex_unlock(pthread_mutex_t *mp);
extern int _pthread_setschedparam(pthread_t thread, int policy,
					const struct sched_param *param);
#ifdef __cplusplus
}
#endif

#endif /* _UNDERSCORE_H */
