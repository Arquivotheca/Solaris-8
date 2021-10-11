/*
 * Copyright (c) 1991-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_LWP_H
#define	_SYS_LWP_H

#pragma ident	"@(#)lwp.h	1.30	98/01/06 SMI"

#include <sys/synch.h>
#include <sys/ucontext.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * lwp create flags
 */
#define	LWP_DETACHED	0x00000040
#define	LWP_SUSPENDED	0x00000080

/*
 * The following flag is reserved. An application should never use it as a flag
 * to _lwp_create(2).
 */
#define	__LWP_ASLWP	0x00000100

/*
 * Definitions for user programs calling into the _lwp interface.
 */
struct lwpinfo {
	timestruc_t lwp_utime;
	timestruc_t lwp_stime;
	long	    lwpinfo_pad[64];
};

#if defined(_SYSCALL32)

/* Kernel's view of user ILP32 lwpinfo structure */

struct lwpinfo32 {
	timestruc32_t	lwp_utime;
	timestruc32_t	lwp_stime;
	int32_t		lwpinfo_pad[64];
};

#endif	/* _SYSCALL32 */

#ifndef _KERNEL

typedef unsigned int lwpid_t;

void		_lwp_makecontext(ucontext_t *, void ((*)(void *)),
		    void *, void *, caddr_t, size_t);
int		_lwp_create(ucontext_t *, uint_t, lwpid_t *);
int		_lwp_kill(lwpid_t, int);
int		_lwp_info(struct lwpinfo *);
void		_lwp_exit(void);
int		_lwp_wait(lwpid_t, lwpid_t *);
lwpid_t		_lwp_self(void);
int		_lwp_suspend(lwpid_t);
int		_lwp_suspend2(lwpid_t, int *);
int		_lwp_continue(lwpid_t);
void		_lwp_setprivate(void *);
void*		_lwp_getprivate(void);

#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_LWP_H */
