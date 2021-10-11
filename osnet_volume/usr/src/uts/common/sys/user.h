/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_USER_H
#define	_SYS_USER_H

#pragma ident	"@(#)user.h	1.59	99/08/31 SMI"

#include <sys/types.h>
#include <sys/signal.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * struct exdata is visible in and out of the kernel. This is because it
 * is referenced in <sys/core.h> which doesn't have this kind of magic.
 */
struct exdata {
	struct vnode	*vp;
	size_t	ux_tsize;	/* text size */
	size_t	ux_dsize;	/* data size */
	size_t	ux_bsize;	/* bss size */
	size_t	ux_lsize;	/* lib size */
	long	ux_nshlibs;	/* number of shared libs needed */
	short	ux_mach;	/* machine type */
	short	ux_mag;		/* magic number MUST be here */
	off_t	ux_toffset;	/* file offset to raw text */
	off_t	ux_doffset;	/* file offset to raw data */
	off_t	ux_loffset;	/* file offset to lib sctn */
	caddr_t	ux_txtorg;	/* start addr of text in mem */
	caddr_t	ux_datorg;	/* start addr of data in mem */
	caddr_t	ux_entloc;	/* entry location */
};

#ifdef	__cplusplus
}
#endif

#if defined(_KERNEL) || defined(_KMEMUSER)

#include <sys/param.h>
#include <sys/pcb.h>
#include <sys/siginfo.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/auxv.h>
#include <sys/errno.h>
#include <sys/t_lock.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Entry in the per-process list of open files.
 * Note: only certain fields are copied in flist_grow() and flist_fork().
 * This is indicated in brackets in the structure member comments.
 */
typedef struct uf_entry {
	kmutex_t	uf_lock;	/* per-fd lock [never copied] */
	struct file	*uf_file;	/* file pointer [grow, fork] */
	struct fpollinfo *uf_fpollinfo;	/* poll state [grow] */
	int		uf_refcnt;	/* LWPs accessing this file [grow] */
	int		uf_alloc;	/* right subtree allocs [grow, fork] */
	short		uf_flag;	/* fcntl F_GETFD flags [grow, fork] */
	short		uf_busy;	/* file is allocated [grow, fork] */
	kcondvar_t	uf_wanted_cv;	/* waiting for setf() [never copied] */
	kcondvar_t	uf_closing_cv;	/* waiting for close() [never copied] */
} uf_entry_t;

/*
 * Retired file lists -- see flist_grow() for details.
 */
typedef struct uf_rlist {
	struct uf_rlist	*ur_next;
	uf_entry_t	*ur_list;
	int		ur_nfiles;
} uf_rlist_t;

/*
 * Per-process file information.
 */
typedef struct uf_info {
	kmutex_t	fi_lock;	/* see below */
	kmutex_t	fi_pad;		/* unused -- remove in next release */
	int		fi_nfiles;	/* number of entries in fi_list[] */
	uf_entry_t *volatile fi_list;	/* current file list */
	uf_rlist_t	*fi_rlist;	/* retired file lists */
} uf_info_t;

/*
 * File list locking.
 *
 * Each process has a list of open files, fi_list, indexed by fd.
 * fi_list is an array of uf_entry_t structures, each with its own lock.
 * One might think that the correct way to lock a file descriptor would be:
 *
 *	ufp = fip->fi_list[fd];
 *	mutex_enter(&ufp->uf_lock);
 *
 * However, that construct is only safe if fi_lock is already held.  If not,
 * fi_list can change in the window between loading ufp and entering uf_lock.
 * The UF_ENTER() macro deals with this possibility.  UF_ENTER(ufp, fip, fd)
 * locks fd and sets ufp to fd's uf_entry.  The locking rules are as follows:
 *
 * (1) fi_lock protects fi_list and fi_nfiles.  It also protects the
 *     uf_alloc and uf_busy fields of every fd's ufp; see fd_find() for
 *     details on file descriptor allocation.
 *
 * (2) UF_ENTER(ufp, fip, fd) locks descriptor fd and sets ufp to point
 *     to the uf_entry_t for fd.  UF_ENTER() protects all fields in ufp
 *     except uf_alloc and uf_busy.  UF_ENTER(ufp, fip, fd) also prevents
 *     ufp->uf_alloc, ufp->uf_busy, fip->fi_list and fip->fi_nfiles from
 *     changing.
 *
 * (3) The lock ordering is (1), (2).
 *
 * (4) Note that fip->fi_list and fip->fi_nfiles cannot change while *any*
 *     file list lock is held.  Thus flist_grow() must acquire all such
 *     locks -- fi_lock and every fd's uf_lock -- to install a new file list.
 */
#define	UF_ENTER(ufp, fip, fd)					\
	for (;;) {						\
		uf_entry_t *_flist = (fip)->fi_list;		\
		ufp = &_flist[fd];				\
		ASSERT((fd) < (fip)->fi_nfiles);		\
		mutex_enter(&ufp->uf_lock);			\
		if (_flist == (fip)->fi_list)			\
			break;					\
		mutex_exit(&ufp->uf_lock);			\
	}

#define	UF_EXIT(ufp)	mutex_exit(&ufp->uf_lock)

#define	PSARGSZ		80	/* Space for exec arguments (used by ps(1)) */

#define	PSCOMSIZ	14

#define	MAXCOMLEN	16	/* <= MAXNAMLEN, >= sizeof (ac_comm) */

typedef struct {		/* kernel syscall set type */
	uint_t	word[9];	/* space for syscall numbers [1..288] */
} k_sysset_t;

/*
 * __KERN_NAUXV_IMPL is defined as a convenience sizing mechanism
 * for the portions of the kernel that care about aux vectors.
 *
 * Applications that need to know how many aux vectors the kernel
 * supplies should use the proc(4) interface to read /proc/PID/auxv.
 *
 * This value should not be changed in a patch.
 */
#if defined(sparc) || defined(__sparc)
#define	__KERN_NAUXV_IMPL 19
#elif defined(i386) || defined(__i386)
#define	__KERN_NAUXV_IMPL 21
#elif defined(__ia64)
#define	__KERN_NAUXV_IMPL 21
#endif /* sparc || _sparc */

struct execsw;

/*
 * The user structure; one allocated per process.  Contains all the
 * per-process data that doesn't need to be referenced while the
 * process is swapped.
 */
typedef	struct	user {

	/*
	 * Fields that require no explicit locking
	 */
	int	u_execid;
	long	u_execsz;
	pgcnt_t	u_tsize;		/* text size (pages) */
	pgcnt_t	u_dsize;		/* data size (pages) */
	time_t u_start;
	clock_t	u_ticks;
	kcondvar_t u_cv;		/* user structure's condition var */

	/*
	 * Executable file info.
	 */
	struct exdata	u_exdata;
	auxv_t  u_auxv[__KERN_NAUXV_IMPL]; /* aux vector from exec */
	char	u_psargs[PSARGSZ];	/* arguments from exec */
	char	u_comm[MAXCOMLEN + 1];

	/*
	 * Initial values of argc, argv and envp to main(), plus
	 * the initial address of the aux vector, for /proc
	 */
	int	u_argc;
	int	u_envc;
	int	u_auxvc;
	uintptr_t u_argv;
	uintptr_t u_envp;
	uintptr_t u_auxvp;

	/*
	 * protected by p_lock
	 */
	struct vnode *u_cdir;		/* current directory */
	struct vnode *u_rdir;		/* root directory */
	mode_t	u_cmask;		/* mask for file creation */
	long	u_mem;
	short	u_nshmseg;		/* # shm segments currently attached */
	char	u_acflag;		/* accounting flag */
	char	u_systrap;		/* /proc: any syscall mask bits set? */

	/*
	 * Protected by pidlock
	 */
	k_sysset_t u_entrymask;		/* /proc syscall stop-on-entry mask */
	k_sysset_t u_exitmask;		/* /proc syscall stop-on-exit mask */
	k_sigset_t u_signodefer;	/* signals defered when caught */
	k_sigset_t u_sigonstack;	/* signals taken on alternate stack */
	k_sigset_t u_sigresethand;	/* signals reset when caught */
	k_sigset_t u_sigrestart;	/* signals that restart system calls */
	k_sigset_t u_sigmask[MAXSIG];	/* signals held while in catcher */
	void	(*u_signal[MAXSIG])();	/* Disposition of signals */

	/*
	 * Updates to individual fields in u_rlimit are not atomic and to
	 * ensure a meaningful set of numbers, p_lock is used whenever
	 * the field in u_rlimit is read/modified such as
	 * getrlimit() or setrlimit().  The "saved" rlimits are used to
	 * hold the limits across system calls.  Initially introduced
	 * by the LFS API, this is now possible for all resource limits
	 * on the 64-bit kernel.
	 */
	struct rlimit64 u_rlimit[RLIM_NLIMITS]; /* resource usage limits */
#if defined(_LP64)
	struct rlimit64	u_saved_rlimit[RLIM_NLIMITS];
#else
	struct rlimit64	u_saved_lf_rlimit;	/* RLIMIT_FSIZE only */
#endif
	uf_info_t	u_finfo;	/* open file information */

#if defined(i386) || defined(__i386) || defined(__ia64)
	void    (*u_sigreturn)();	/* For SVR532 signal handling cleanup */
#endif	/* __i386 || __ia64 */
	struct execsw *u_execsw;	/* ptr to exec switch table entry */
} user_t;

#include <sys/proc.h>			/* cannot include before user defined */

#ifdef	_KERNEL
#ifdef	sun
#define	u	(curproc->p_user)	/* user is now part of proc structure */
#endif  /* sun */

#define	P_FINFO(p)	(&(p)->p_user.u_finfo)

/*
 * Define macros to access the current rlimit values via the proc
 * structure.  Large file support solidified the concept of
 * RLIM64_INFINITY as a token and not as a number.  The mapping from
 * the concept "infinity" to an actual number is, in general, dependent
 * on the data model of the process - hence on the 64-bit kernel we
 * call an implementation function to aid with the mapping.
 */

#define	UNLIMITED_CUR(p, type)	\
	(PTOU(p)->u_rlimit[(type)].rlim_cur == RLIM64_INFINITY)

#if defined(_SYSCALL32_IMPL) || defined(__lint)

#define	P_CURLIMIT(p, type)	p_curlimit((p), (type), (p)->p_model)
#define	P_CURLIMIT32(p, type)	p_curlimit((p), (type), DATAMODEL_ILP32)

#else	/* _SYSCALL32_IMPL || __lint */

#define	P_CURLIMIT(p, type)			\
	(UNLIMITED_CUR(p, type) ?		\
	    rlim_infinity_map[(type)] : PTOU(p)->u_rlimit[(type)].rlim_cur)

#define	P_CURLIMIT32(p, type)	P_CURLIMIT(p, type)

#endif	/* _SYSCALL32_IMPL || __lint */

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#else	/* defined(_KERNEL) || defined(_KMEMUSER) */

/*
 * Here, we define a fake version of struct user for programs
 * (debuggers) that use ptrace() to read and modify the saved
 * registers directly in the u-area.  ptrace() has been removed
 * from the operating system and now exists as a library function
 * in libc, built on the /proc process filesystem.  The ptrace()
 * library function provides access only to the members of the
 * fake struct user defined here.
 *
 * User-level programs that must know the real contents of struct
 * user will have to define _KMEMUSER before including <sys/user.h>.
 * Such programs also become machine specific. Carefully consider
 * the consequences of your actions.
 */

#include <sys/regset.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	PSARGSZ		80	/* Space for exec arguments (used by ps(1)) */

typedef	struct	user {
	gregset_t	u_reg;		/* user's saved registers */
	greg_t		*u_ar0;		/* address of user's saved R0 */
	char	u_psargs[PSARGSZ];	/* arguments from exec */
	void	(*u_signal[MAXSIG])();	/* Disposition of signals */
	int		u_code;		/* fault code on trap */
	caddr_t		u_addr;		/* fault PC on trap */
} user_t;

#ifdef	__cplusplus
}
#endif

#endif	/* defined(_KERNEL) || defined(_KMEMUSER) */

#endif	/* _SYS_USER_H */
