/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * The enclosed is a private interface between system libraries and
 * the kernel.  It should not be used in any other way.  It may be
 * changed without notice in a minor release of Solaris.
 */

#ifndef	_SYS_SCHEDCTL_H
#define	_SYS_SCHEDCTL_H

#pragma ident	"@(#)schedctl.h	1.4	98/01/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#if !defined(_ASM)

#include <sys/types.h>
#include <sys/processor.h>
#ifdef	_KERNEL
#include <sys/mutex.h>
#include <sys/thread.h>
#include <sys/vnode.h>
#include <sys/cpuvar.h>
#include <sys/door.h>
#endif	/* _KERNEL */

/*
 * This "public" portion of the sc_shared data is used by libsched.
 */
typedef struct sc_public {
	volatile short	sc_nopreempt;
	volatile short	sc_yield;
} sc_public_t;

typedef struct sc_shared {
	volatile uint_t	sc_state;	/* current LWP state */
	volatile processorid_t sc_cpu;	/* last CPU on which LWP ran */
	volatile int	sc_priority;	/* priority of thread running on LWP */
	sc_public_t	sc_preemptctl;	/* preemption control data */
} sc_shared_t;

/*
 * Possible state settings.  These are same as the kernel thread states
 * except there is no zombie state.
 */
#define	SC_FREE		0x00
#define	SC_SLEEP	0x01
#define	SC_RUN		0x02
#define	SC_ONPROC	0x04
#define	SC_STOPPED	0x10

/* priority settings */
#define	SC_IGNORE	-2		/* don't send notifications */
#define	SC_NOPREEMPT	-3		/* don't preempt this lwp */

/* preemption control settings */
#define	SC_MAX_TICKS	2		/* max time preemption can be blocked */

/* _lwp_schedctl() flags */
#define	SC_STATE	0x01
#define	SC_BLOCK	0x02
#define	SC_PRIORITY	0x04
#define	SC_PREEMPT	0x08
#define	SC_DOOR		0x10

#ifdef	_KERNEL
typedef struct	sc_data {
	unsigned int	sc_flags;
	sc_shared_t	*sc_shared;
	kthread_id_t	sc_caller;
	long		sc_tag;
	sc_shared_t	*sc_uaddr;
} sc_data_t;

#define	schedctl_check(t, flag)	(((t)->t_schedctl != NULL) && \
				    ((t)->t_schedctl->sc_flags & (flag)))

int	schedctl(unsigned int, int, sc_shared_t **);
void	schedctl_init(void);
void	schedctl_cleanup(kthread_t *);
int	schedctl_block(kmutex_t *);
void	schedctl_unblock(void);
int	schedctl_get_nopreempt(kthread_t *);
void	schedctl_set_nopreempt(kthread_t *, short);
void	schedctl_set_yield(kthread_t *, short);
#else
int	_lwp_schedctl(unsigned int, int, sc_shared_t **);
#endif	/* _KERNEL */
#endif	/* !defined(_ASM) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SCHEDCTL_H */
