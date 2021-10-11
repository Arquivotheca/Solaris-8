/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_TIMER_H
#define	_SYS_TIMER_H

#pragma ident	"@(#)timer.h	1.17	99/06/05 SMI"

#include <sys/types.h>
#include <sys/proc.h>
#include <sys/thread.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	_KERNEL

#define	_TIMER_MAX 32

/*
 * Bit values for the it_lock field.
 */
#define	ITLK_LOCKED		0x01
#define	ITLK_WANTED		0x02
#define	ITLK_REMOVE		0x04

#define	IT_PERLWP		0x01
#define	IT_SIGNAL		0x02

struct clock_backend;

typedef struct itimer {
	itimerspec_t	it_itime;
	hrtime_t	it_hrtime;
	ushort_t	it_flags;
	ushort_t	it_lock;
	void		*it_arg;
	sigqueue_t	*it_sigq;
	klwp_t		*it_lwp;
	struct proc	*it_proc;
	kcondvar_t	it_cv;
	int		it_blockers;
	int		it_pending;
	int		it_overrun;
	struct clock_backend *it_backend;
} itimer_t;

typedef struct clock_backend {
	struct sigevent clk_default;
	int (*clk_clock_settime)(timespec_t *);
	int (*clk_clock_gettime)(timespec_t *);
	int (*clk_clock_getres)(timespec_t *);
	int (*clk_timer_create)(itimer_t *, struct sigevent *);
	int (*clk_timer_settime)(itimer_t *, int, struct itimerspec *);
	int (*clk_timer_gettime)(itimer_t *, struct itimerspec *);
	int (*clk_timer_delete)(itimer_t *);
	void (*clk_timer_lwpbind)(itimer_t *);
} clock_backend_t;

extern void clock_add_backend(clockid_t clock, clock_backend_t *backend);

extern void timer_fire(itimer_t *);
extern void timer_lwpbind();

extern	void	timer_func(sigqueue_t *);
extern	void	timer_exit(void);
extern	void	timer_lwpexit(void);
extern	clock_t	hzto(struct timeval *);
extern	clock_t	timespectohz(timespec_t *, timespec_t);
extern	int	itimerspecfix(timespec_t *);
extern	void	timespecadd(timespec_t *, timespec_t *);
extern	void	timespecsub(timespec_t *, timespec_t *);
extern	void	timespecfix(timespec_t *);
extern	int	xgetitimer(uint_t, struct itimerval *, int);
extern	int	xsetitimer(uint_t, struct itimerval *, int);

#define	timerspecisset(tvp)		((tvp)->tv_sec || (tvp)->tv_nsec)
#define	timerspeccmp(tvp, uvp)		(((tvp)->tv_sec - (uvp)->tv_sec) ? \
	((tvp)->tv_sec - (uvp)->tv_sec):((tvp)->tv_nsec - (uvp)->tv_nsec))
#define	timerspecclear(tvp)		((tvp)->tv_sec = (tvp)->tv_nsec = 0)

struct oldsigevent {
	/* structure definition prior to notification attributes member */
	int		_notify;
	union {
		int		_signo;
		void		(*_notify_function)(union sigval);
	} _un;
	union sigval	_value;
};

#if defined(_SYSCALL32)

struct oldsigevent32 {
	int32_t		_notify;
	union {
		int32_t		_signo;
		caddr32_t	_notify_function;
	} _un;
	union sigval32	_value;
};

#endif	/* _SYSCALL32 */
#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_TIMER_H */
