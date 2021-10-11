/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_PRSYSTM_H
#define	_SYS_PRSYSTM_H

#pragma ident	"@(#)prsystm.h	1.35	99/09/14 SMI"

#include <sys/isa_defs.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(_KERNEL)

extern kmutex_t pr_pidlock;
extern kcondvar_t *pr_pid_cv;

struct prfpregset;
struct pstatus;
struct lwpstatus;
struct psinfo;
struct lwpsinfo;
struct prcred;

struct seg;
struct regs;
struct watched_page;

/*
 * These are functions in the procfs module that are
 * called from the kernel proper and from other modules.
 */
extern uint_t pr_getprot(struct seg *, int, void **,
	caddr_t *, caddr_t *, caddr_t);
extern size_t pr_getsegsize(struct seg *, int);
extern int  pr_isobject(struct vnode *);
extern int  pr_isself(struct vnode *);
extern void prinvalidate(struct user *);
extern void prgetstatus(proc_t *, struct pstatus *);
extern void prgetlwpstatus(kthread_t *, struct lwpstatus *);
extern void prgetpsinfo(proc_t *, struct psinfo *);
extern void prgetlwpsinfo(kthread_t *, struct lwpsinfo *);
extern void prgetprfpregs(klwp_t *, struct prfpregset *);
extern void prgetprxregs(klwp_t *, caddr_t);
extern int  prgetprxregsize(proc_t *);
#if defined(lint)
/* Work around lint confusion between old and new prcred definitions */
extern void prgetcred();
#else
extern void prgetcred(proc_t *, struct prcred *);
#endif
extern int  prnsegs(struct as *, int);
extern void prfree(proc_t *);
extern void prexit(proc_t *);
extern void prlwpexit(kthread_t *);
extern void prexecstart(void);
extern void prexecend(void);
extern void prrelvm(void);
extern void prbarrier(proc_t *);
extern void prstop(int, int);
extern void prnotify(struct vnode *);
extern void prstep(klwp_t *, int);
extern void prnostep(klwp_t *);
extern void prdostep(void);
extern int  prundostep(void);
extern int  prhasfp(void);
extern int  prhasx(proc_t *);
extern caddr_t prmapin(struct as *, caddr_t, int);
extern void prmapout(struct as *, caddr_t, caddr_t, int);
extern int  pr_watch_emul(struct regs *, caddr_t, enum seg_rw);
extern void pr_free_my_pagelist(void);
extern int  pr_allstopped(proc_t *);
#if defined(sparc) || defined(__sparc)
struct gwindows;
extern	int	prnwindows(klwp_t *);
extern	void	prgetwindows(klwp_t *, struct gwindows *);
#endif
#if defined(__sparcv9)
extern	void	prgetasregs(klwp_t *, asrset_t);
extern	void	prsetasregs(klwp_t *, asrset_t);
#endif	/* __sparcv9 */

#ifdef _SYSCALL32_IMPL
struct prfpregset32;
struct pstatus32;
struct lwpstatus32;
struct psinfo32;
struct lwpsinfo32;
extern void prgetstatus32(proc_t *, struct pstatus32 *);
extern void prgetlwpstatus32(kthread_t *, struct lwpstatus32 *);
extern void prgetpsinfo32(proc_t *, struct psinfo32 *);
extern void prgetlwpsinfo32(kthread_t *, struct lwpsinfo32 *);
extern void prgetprfpregs32(klwp_t *, struct prfpregset32 *);
#if defined(sparc) || defined(__sparc)
struct gwindows32;
void		prgetwindows32(klwp_t *, struct gwindows32 *);
#endif
#endif	/* _SYSCALL32_IMPL */

#endif	/* defined (_KERNEL) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PRSYSTM_H */
