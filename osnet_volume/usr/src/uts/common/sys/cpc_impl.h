/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_CPC_IMPL_H
#define	_SYS_CPC_IMPL_H

#pragma ident	"@(#)cpc_impl.h	1.2	99/11/20 SMI"

#include <sys/inttypes.h>
#include <sys/cpc_event.h>
#include <sys/systm.h>
#include <sys/proc.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * These system call subcodes and ioctls allow the implementation of the
 * libcpc library to store and retrieve performance counter data.  Subject
 * to arbitrary change without notice at any time.  Do not invoke them
 * directly!
 */
#define	_CPCIO_IOC			((((('c'<<8)|'p')<<8)|'c')<<8)

#define	CPCIO_BIND_EVENT		(_CPCIO_IOC | 0x1)
#define	CPCIO_TAKE_SAMPLE		(_CPCIO_IOC | 0x2)
#define	CPCIO_RELE			(_CPCIO_IOC | 0x3)

#define	CPC_BIND_EVENT			0
#define	CPC_TAKE_SAMPLE			1
#define	CPC_USR_EVENTS			2
#define	CPC_SYS_EVENTS			3
#define	CPC_INVALIDATE			4
#define	CPC_RELE			5

#if defined(__sparc)

typedef struct _kcpc_ctx {
	cpc_event_t	c_event;
	uint64_t	c_rawtick;	/* copy of %tick */
	uint64_t	c_rawpic;	/* copy of %pic */
	struct _kcpc_ctx *c_next;
	uint_t		c_flags;
} kcpc_ctx_t;

#elif defined(__i386)

typedef struct _kcpc_ctx {
	cpc_event_t	c_event;
	uint64_t	c_rawtsc;	/* copy of tsc */
	uint64_t	c_rawpic[2];	/* copy of 31-bits of PerfCtr{0,1} */
	struct _kcpc_ctx *c_next;
	uint_t		c_flags;
} kcpc_ctx_t;

#else
#error	"no performance counters?"
#endif	/* __sparc */

/*
 * c_flags values
 */
#define	KCPC_CTX_FREEZE		0x1	/* => no sampling */
#define	KCPC_CTX_SIGOVF		0x2	/* => send signal on overflow */
#define	KCPC_CTX_NONPRIV	0x4	/* => non-priv access to counters */
#define	KCPC_CTX_LWPINHERIT	0x8	/* => lwp_create inherits ctx */
#define	KCPC_CTX_INVALID	0x100	/* => context stolen; discard */

#define	KCPC_CTX_ALLFLAGS	((uint_t)0x10f)	/* for assertion checking! */

#if defined(_KERNEL)

#if defined(DEBUG)
extern kcpc_ctx_t *__ttocpcctx(kthread_t *);
#define	ttocpcctx(t)		__ttocpcctx(t)
#else
#define	ttocpcctx(t)		tsd_agent_get(t, kcpc_key)
#endif

#define	KCPC_VALID_CTX(ctx)	(((ctx)->c_flags & ~KCPC_CTX_ALLFLAGS) == 0)

extern void kcpc_hw_sample(kcpc_ctx_t *);
extern uint_t kcpc_hw_overflow_intr(caddr_t);
extern int kcpc_hw_overflow_intr_installed;
extern void kcpc_hw_overflow_trap(kthread_t *);
extern void kcpc_hw_save(kcpc_ctx_t *);
extern void kcpc_hw_restore(kcpc_ctx_t *);
extern int kcpc_hw_bind(kcpc_ctx_t *);
extern void kcpc_hw_setusrsys(kcpc_ctx_t *, int, int);
extern void kcpc_hw_clone(kcpc_ctx_t *, kcpc_ctx_t *);
extern int kcpc_hw_probe(void);
extern int kcpc_hw_add_ovf_intr(uint_t (*)(caddr_t));
extern void kcpc_hw_rem_ovf_intr(void);

#endif	/* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_CPC_IMPL_H */
