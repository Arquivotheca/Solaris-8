/*
 * Copyright (c) 1987-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_CORE_H
#define	_SYS_CORE_H

#pragma ident	"@(#)core.h	1.36	99/03/31 SMI"

#ifndef _KERNEL
#include <sys/reg.h>
#endif /* _KERNEL */

#include <sys/exechdr.h>
#include <sys/pcb.h>
#include <sys/user.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	CORE_MAGIC	0x080456
#define	CORE_NAMELEN	16		/* Related to MAXCOMLEN in user.h */

/*
 * Format of the beginning of a `new' core file.
 * The `old' core file consisted of dumping the u area.
 * In the `new' core format, this structure is followed
 * copies of the data and  stack segments.  Finally the user
 * struct is dumped at the end of the core file for programs
 * which really need to know this kind of stuff.  The length
 * of this struct in the core file can be found in the
 * c_len field.  When struct core is changed, c_fpstatus
 * and c_fparegs should start at long word boundaries (to
 * make the floating pointing signal handler run more efficiently).
 */
struct core {
	int	c_magic;		/* Corefile magic number */
	int	c_len;			/* Sizeof (struct core) */
#ifdef _KERNEL
	gregset_t c_regs;		/* General purpose registers */
#else
	struct	regs c_regs;		/* General purpose registers */
#endif /* _KERNEL */
	struct 	exdata c_exdata;	/* Executable header */
	int	c_signo;		/* Killing signal, if any */
	int	c_tsize;		/* Text size (bytes) */
	int	c_dsize;		/* Data size (bytes) */
	int	c_ssize;		/* Stack size (bytes) */
	char	c_cmdname[CORE_NAMELEN + 1]; /* Command name */
	struct	fpu c_fpu;		/* external FPU state */
#if defined(sparc) || defined(__sparc)
	struct	fq c_fpu_q[MAXFPQ];	/* fpu exception queue */
#endif
	int	c_ucode;		/* Exception no. from u_code */
};

#ifdef	_KERNEL

extern int core(int sig);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_CORE_H */
