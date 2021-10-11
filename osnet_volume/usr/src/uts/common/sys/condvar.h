/*
 * Copyright (c) 1991-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * condvar.h:
 *
 * definitions for thread synchronization primitives: condition variables
 * This is the public part of the interface to condition variables. The
 * private (implementation-specific) part is in <arch>/sys/condvar_impl.h.
 */

#ifndef _SYS_CONDVAR_H
#define	_SYS_CONDVAR_H

#pragma ident	"@(#)condvar.h	1.10	98/02/18 SMI"

#ifndef	_ASM
#include <sys/types.h>
#ifdef _KERNEL
#include <sys/mutex.h>
#endif	/* _KERNEL */
#endif	/* _ASM */

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef	_ASM

/*
 * Condtion variables.
 */

typedef struct _kcondvar {
	ushort_t	_opaque;
} kcondvar_t;

typedef	enum {
	CV_DEFAULT,
	CV_DRIVER
} kcv_type_t;


#if defined(_KERNEL)

/*
 * condition variable function prototypes
 */

extern	void	cv_init(kcondvar_t *, char *, kcv_type_t, void *);
extern  void	cv_destroy(kcondvar_t *);
extern	void	cv_wait(kcondvar_t *, kmutex_t *);
extern	void	cv_wait_stop(kcondvar_t *, kmutex_t *);
extern	clock_t	cv_timedwait(kcondvar_t *, kmutex_t *, clock_t);
extern	int	cv_wait_sig(kcondvar_t *, kmutex_t *);
extern	clock_t	cv_timedwait_sig(kcondvar_t *, kmutex_t *, clock_t);
extern	int	cv_wait_sig_swap(kcondvar_t *, kmutex_t *);
extern	void	cv_signal(kcondvar_t *);
extern	void	cv_broadcast(kcondvar_t *);

#endif	/* defined(_KERNEL) */

#endif	/* _ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_CONDVAR_H */
