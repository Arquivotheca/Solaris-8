/*
 * Copyright (c) 1993-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_ARCHSYSTM_H
#define	_SYS_ARCHSYSTM_H

#pragma ident	"@(#)archsystm.h	1.24	98/03/09 SMI"

/*
 * A selection of ISA-dependent interfaces
 */

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_KERNEL) && !defined(_ASM)

#include <sys/types.h>
#include <sys/regset.h>
#include <sys/model.h>

extern greg_t getfp(void);
extern greg_t getpsr(void);
extern uint_t getpil(void);
extern void setpil(uint_t);
extern greg_t gettbr(void);
extern void realsigprof(int, int);

extern uintptr_t shm_alignment;

struct proc;
struct _klwp;
extern void xregrestore(struct _klwp *, int);
extern int  copy_return_window(int);

extern void setgwins(struct _klwp *, gwindows_t *);
extern void getgwins(struct _klwp *, gwindows_t *);
#ifdef	__sparcv9
extern void setgwins32(struct _klwp *, gwindows32_t *);
extern void getgwins32(struct _klwp *, gwindows32_t *);
extern void setasrs(struct _klwp *, asrset_t);
extern void getasrs(struct _klwp *, asrset_t);
extern void setfpasrs(struct _klwp *, asrset_t);
extern void getfpasrs(struct _klwp *, asrset_t);
#endif	/* __sparcv9 */

extern void setgregs(struct _klwp *, gregset_t);
extern void getgregs(struct _klwp *, gregset_t);
extern void setfpregs(struct _klwp *, fpregset_t *);
extern void getfpregs(struct _klwp *, fpregset_t *);

#ifdef _SYSCALL32_IMPL
extern void setfpregs32(struct _klwp *, fpregset32_t *);
extern void getfpregs32(struct _klwp *, fpregset32_t *);
#endif

extern void vac_flushall(void);

extern void bind_hwcap(void);
extern void kern_use_hwinstr(int hwmul, int hwdiv);
extern int get_hwcap_flags(int inkernel);

extern int enable_mixed_bcp; /* patchable in /etc/system */

#ifdef __sparcv9cpu

extern u_longlong_t gettick(void);
extern int xcopyin_little(const void *, void *, size_t);
extern int xcopyout_little(const void *, void *, size_t);
extern void xregs_getgfiller(klwp_id_t lwp, caddr_t xrp);
extern void xregs_setgfiller(klwp_id_t lwp, caddr_t xrp);
extern void xregs_getfpfiller(klwp_id_t lwp, caddr_t xrp);
extern void xregs_setfpfiller(klwp_id_t lwp, caddr_t xrp);

#endif	/* __sparcv9cpu */

struct ucontext;
extern	void	xregs_clrptr(struct _klwp *, struct ucontext *);
extern	int	xregs_hasptr(struct _klwp *, struct ucontext *);
extern	caddr_t	xregs_getptr(struct _klwp *, struct ucontext *);
extern	void	xregs_setptr(struct _klwp *, struct ucontext *, caddr_t);
#ifdef __sparcv9
struct	ucontext32;
extern	void	xregs_clrptr32(struct _klwp *, struct ucontext32 *);
extern	int	xregs_hasptr32(struct _klwp *, struct ucontext32 *);
extern	caddr32_t xregs_getptr32(struct _klwp *, struct ucontext32 *);
extern	void	xregs_setptr32(struct _klwp *, struct ucontext32 *, caddr32_t);
#endif	/* __sparcv9 */
extern	void	xregs_getgregs(struct _klwp *, caddr_t);
extern	void	xregs_getfpregs(struct _klwp *, caddr_t);
extern	void	xregs_get(struct _klwp *, caddr_t);
extern	void	xregs_setgregs(struct _klwp *, caddr_t);
extern	void	xregs_setfpregs(struct _klwp *, caddr_t);
extern	void	xregs_set(struct _klwp *, caddr_t);
extern	int	xregs_getsize(struct proc *);

extern void doflush(void *);

#endif /* _KERNEL && !_ASM */


#if defined(_KERNEL)

/*
 * For binary compatability with SPARC/Solaris 1.  Needed in the
 * sparc assembly files.
 */
#define	OSYS_mmap	71

#endif /* _KERNEL */


/*
 * Flags used to hint at various performance enhancements available
 * on different SPARC processors.
 */
#define	AV_SPARC_HWMUL_32x32	1	/* 32x32-bit smul/umul is efficient */
#define	AV_SPARC_HWDIV_32x32	2	/* 32x32-bit sdiv/udiv is efficient */
#define	AV_SPARC_HWFSMULD	4	/* fsmuld is efficient */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_ARCHSYSTM_H */
