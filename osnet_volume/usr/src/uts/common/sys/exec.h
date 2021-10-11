/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_EXEC_H
#define	_SYS_EXEC_H

#pragma ident	"@(#)exec.h	1.41	99/08/31 SMI"

#include <sys/systm.h>
#include <vm/seg.h>
#include <vm/seg_vn.h>
#include <sys/model.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Number of bytes to read for magic string
 */
#define	MAGIC_BYTES	8

#define	getexmag(x)	(((x)[0] << 8) + (x)[1])

typedef struct execa {
	const char *fname;
	const char **argp;
	const char **envp;
} execa_t;

typedef struct execenv {
	caddr_t ex_brkbase;
	size_t	ex_brksize;
	vnode_t *ex_vp;
	short   ex_magic;
} execenv_t;

#ifdef _KERNEL

#define	LOADABLE_EXEC(e)	((e)->exec_lock)
#define	LOADED_EXEC(e)		((e)->exec_func)

extern int nexectype;		/* number of elements in execsw */
extern struct execsw execsw[];
extern kmutex_t execsw_lock;

/*
 * User argument structure for stack image management
 */
typedef struct uarg {
	ssize_t	na;
	ssize_t	ne;
	ssize_t	nc;
	ssize_t arglen;
	char	*fname;
	char	*pathname;
	ssize_t	auxsize;
	caddr_t	stackend;
	size_t	stk_align;
	size_t	stk_size;
	char	*stk_base;
	char	*stk_strp;
	int	*stk_offp;
	size_t	usrstack_size;
	int	traceinval;
	model_t	to_model;
	model_t	from_model;
	size_t	to_ptrsize;
	size_t	from_ptrsize;
	size_t	ncargs;
	struct execsw *execswp;
} uarg_t;

/*
 * The following macro is a machine dependent encapsulation of
 * postfix processing to hide the stack direction from elf.c
 * thereby making the elf.c code machine independent.
 */
#define	execpoststack(ARGS, ARRAYADDR, BYTESIZE) \
	(copyout((caddr_t)(ARRAYADDR), (ARGS)->stackend, (BYTESIZE)) ? EFAULT \
		: (((ARGS)->stackend += (BYTESIZE)), 0))

/*
 * This provides the current user stack address for an object of size BYTESIZE.
 * Used to determine the stack address just before applying execpoststack().
 */
#define	stackaddress(ARGS, BYTESIZE)	((ARGS)->stackend)

/*
 * Macro to add attribute/values the aux vector under construction.
 */
#define	ADDAUX(p, a, v)	((p)->a_type = (a), ((p)++)->a_un.a_val = (v))

#define	INTPSZ	MAXPATHLEN
typedef struct intpdata {
	char	*intp;
	char	*intp_name;
	char	*intp_arg;
} intpdata_t;

struct execsw {
	char	*exec_magic;
	int	exec_magoff;
	int	exec_maglen;
	int	(*exec_func)(struct vnode *vp, struct execa *uap,
		    struct uarg *args, struct intpdata *idata, int level,
		    long *execsz, int setid, caddr_t exec_file,
		    struct cred *cred);
	int	(*exec_core)(struct vnode *vp, struct proc *p,
		    struct cred *cred, rlim64_t rlimit, int sig);
	krwlock_t	*exec_lock;
};

extern short elfmagic;
extern short intpmagic;
extern short javamagic;
#ifdef sparc
extern short aout_zmagic;
extern short aout_nmagic;
extern short aout_omagic;
#endif
#if defined(i386) || defined(__i386) || defined(__ia64)
extern short coffmagic;
#endif
extern short nomagic;

extern char elf32magicstr[];
extern char elf64magicstr[];
extern char intpmagicstr[];
extern char javamagicstr[];
#ifdef	sparc
extern char aout_nmagicstr[];
extern char aout_zmagicstr[];
extern char aout_omagicstr[];
#endif
#if defined(i386) || defined(__i386) || defined(__ia64)
extern char coffmagicstr[];
#endif
extern char nomagicstr[];

extern int exec_args(execa_t *, uarg_t *, intpdata_t *, void **);
extern int exec(const char *fname, const char **argp);
extern int exece(const char *fname, const char **argp, const char **envp);
extern int exec_common(const char *fname, const char **argp,
    const char **envp);
extern int gexec(vnode_t **vp, struct execa *uap, struct uarg *args,
    struct intpdata *idata, int level, long *execsz, caddr_t exec_file,
    struct cred *cred);
extern struct execsw *allocate_execsw(char *name, char *magic,
    size_t magic_size);
extern struct execsw *findexecsw(char *magic);
extern struct execsw *findexec_by_hdr(char *header);
extern struct execsw *findexec_by_magic(char *magic);
extern int execpermissions(struct vnode *vp, struct vattr *vattrp,
    struct uarg *args);
extern int execmap(vnode_t *vp, caddr_t addr, size_t len, size_t zfodlen,
    off_t offset, int prot, int page);
extern void setexecenv(struct execenv *ep);
extern int execopen(struct vnode **vpp, int *fdp);
extern int execclose(int fd);
extern void setregs(void);
extern void exec_set_sp(size_t);
extern int core_seg(proc_t *p, vnode_t *vp, off_t offset, caddr_t addr,
    size_t size, rlim64_t rlimit, cred_t *credp);
extern int execprep(void);

/* a.out stuff */

struct exec;

extern caddr_t gettmem(struct exec *exp);
extern caddr_t getdmem(struct exec *exp);
extern ulong_t getdfile(struct exec *exp);
extern uint_t gettfile(struct exec *exp);
extern int chkaout(struct exdata *exp);
extern void getexinfo(struct exdata *edp_in, struct exdata *edp_out,
    int *pagetext, int *pagedata);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_EXEC_H */
