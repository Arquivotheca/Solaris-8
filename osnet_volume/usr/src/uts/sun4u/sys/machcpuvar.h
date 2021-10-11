/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_MACHCPUVAR_H
#define	_SYS_MACHCPUVAR_H

#pragma ident	"@(#)machcpuvar.h	1.32	99/06/05 SMI"

#include <sys/intr.h>
#include <sys/clock.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _STARFIRE
/*
 * Starfire's cpu upaids are not the same
 * as cpuids.
 * XXX - our obp took the liberty of
 * converting cpu upaids into cpuids when
 * presenting it as upa-portid property.
 */
#define	UPAID_TO_CPUID(upaid)	(upaid)	/* XXX */
#define	CPUID_TO_UPAID(upaid)	(((upaid & 0x3C) << 1) |	\
				((upaid & 0x40) >> 4) |		\
				(upaid &0x3))
#else
/*
 * The mid is the same as the cpu id.
 * We might want to change this later
 */
#define	UPAID_TO_CPUID(upaid)	(upaid)
#define	CPUID_TO_UPAID(cpuid)	(cpuid)
#endif	/* _STARFIRE */

#ifndef	_ASM

#include <sys/obpdefs.h>

/*
 * Machine specific fields of the cpu struct
 * defined in common/sys/cpuvar.h.
 */
struct	machcpu {
	struct machpcb	*mpcb;
	int		mutex_ready;
	int		in_prom;
	int		tl1_hdlr;
	char		*cpu_info;
	u_longlong_t	tmp1;		/* per-cpu tmps */
	u_longlong_t	tmp2;		/*  used in trap processing */
	struct intr_req intr_pool[INTR_PENDING_MAX];	/* intr pool */
	struct intr_req *intr_head[PIL_LEVELS];		/* intr que heads */
	struct intr_req *intr_tail[PIL_LEVELS];		/* intr que tails */
	int		intr_pool_added;		/* add'l intr pool */
	boolean_t	poke_cpu_outstanding;
};

/*
 * The OpenBoot Standalone Interface supplies the kernel with
 * implementation dependent parameters through the devinfo/property mechanism
 */
#define	MAXSYSNAME	20

struct cpu_node {
	char	name[MAXSYSNAME];
	int	implementation;
	int	version;
	int	upaid;
	dnode_t	nodeid;
	uint_t	clock_freq;
	union {
		int	dummy;
	}	u_info;
	int	ecache_size;
};

extern struct cpu_node cpunodes[];

#endif	/* _ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MACHCPUVAR_H */
