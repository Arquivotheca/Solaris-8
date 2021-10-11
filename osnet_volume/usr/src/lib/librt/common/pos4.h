/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All Rights reserved.
 *
 * THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI
 * The copyright notice above does not evidence any
 * actual or intended publication of such source code.
 *
 *
 * pos4.h:  Header file for the POSIX.4 [sic] library
 *
 * Contains largely extern definitions for functions which librt
 * finds elsewhere.  The only real meat is the definition of pos4_jmptab_t,
 * which is the jump table by which we call the right seamphore routines
 * (either the _lwp_sema_* family or the sema_* family of calls, depending
 * on whether or not the application is linked with libthread.
 */

#ifndef	_POS4_H
#define	_POS4_H

#pragma ident	"@(#)pos4.h	1.8	99/12/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <aio.h>
#include <time.h>
#include <signal.h>
#include <siginfo.h>
#include <unistd.h>
#include <semaphore.h>

extern long _lsemvaluemax;

typedef struct pos4_jmptab {
	int (*sema_init)(void *, int, int, void *);
	int (*sema_wait)(void *);
	int (*sema_trywait)(void *);
	int (*sema_post)(void *);
	int (*sema_destroy)(void *);
} pos4_jmptab_t;

extern const pos4_jmptab_t *pos4_jmptab;

extern int __aio_cancel(int, struct aiocb *);
extern int __aio_cancel64(int, struct aiocb64 *);
extern int __aio_error(const struct aiocb *);
extern int __aio_error64(const struct aiocb64 *);
extern int __aio_fsync(int, struct aiocb *, int);
extern int __aio_fsync64(int, struct aiocb64 *, int);
extern int __aio_read(struct aiocb *);
extern int __aio_read64(struct aiocb64 *);
extern ssize_t __aio_return(struct aiocb *);
extern ssize_t __aio_return64(struct aiocb64 *);
extern int __aio_suspend(const struct aiocb * const list[], int,
    const struct timespec *);
extern int __aio_suspend64(const struct aiocb64 * const list[], int,
    const struct timespec *);
extern int __aio_write(struct aiocb *);
extern int __aio_write64(struct aiocb64 *);
extern int __lio_listio(int, struct aiocb * const list[], int,
    struct sigevent *);
extern int __lio_listio64(int, struct aiocb64 * const list[], int,
    struct sigevent *);

extern int __clock_getres(clockid_t, struct timespec *);
extern int __clock_gettime(clockid_t, struct timespec *);
extern int __clock_settime(clockid_t, const struct timespec *);
extern int __timer_create(clockid_t, struct sigevent *, timer_t *);
extern int __timer_delete(timer_t);
extern int __timer_getoverrun(timer_t);
extern int __timer_gettime(timer_t, struct itimerspec *);
extern int __timer_settime(timer_t, int, const struct itimerspec *,
    struct itimerspec *);
extern int __nanosleep(const struct timespec *, struct timespec *);

extern int __sigtimedwait(const sigset_t *, siginfo_t *,
    const struct timespec *);
extern int __sigqueue(pid_t pid, int signo, const union sigval value,
    int si_code);
extern int _thr_main(void);
extern void _thr_yield(void);
extern int _librt_sema_wait(sem_t *);

#pragma weak	pthread_setcancelstate

#ifdef	__cplusplus
}
#endif

#endif	/* _POS4_H */
