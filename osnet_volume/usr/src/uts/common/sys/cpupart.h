/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_CPUPART_H
#define	_SYS_CPUPART_H

#pragma ident	"@(#)cpupart.h	1.3	98/01/06 SMI"

#include <sys/types.h>
#include <sys/processor.h>
#include <sys/cpuvar.h>
#include <sys/disp.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _KERNEL

typedef int	cpupartid_t;

/* partition operations */

/* operations of cpu_partition */
#define	CP_CREATE	1
#define	CP_DESTROY	2
#define	CP_ATTACH	3
#define	CP_SET		4
#define	CP_GET		5
#define	CP_CPUS		6

/* special partition id */
#define	CP_NONE		-1

/* level numbers */
#define	CP_DEFAULT	0
#define	CP_SYSTEM	1
#define	CP_PRIVATE	2

#define	CP_PUBLIC(cp)	((cp)->cp_level == CP_SYSTEM || \
			    (cp)->cp_level == CP_DEFAULT)

typedef struct cpupart {
	cpupartid_t	cp_id;		/* partition ID */
	uint_t		cp_level;	/* level or type of partition */
	struct cpupart	*cp_next;	/* next partition in list */
	struct cpupart	*cp_prev;	/* previous partition in list */
	struct cpupart	*cp_base;	/* base or "parent" partition */
	struct cpu	*cp_cpulist;	/* processor list */
	int		cp_ncpus;	/* number of online processors */
	disp_t		cp_kp_queue;	/* partition-wide kpreempt queue */
} cpupart_t;

extern cpupart_t	cp_default;

extern void		cpupart_initialize_default();

extern int		cpupart_create(cpupartid_t *cpid, uint_t attributes);
extern int		cpupart_destroy(cpupartid_t cpid);
extern cpupartid_t	cpupart_query_cpu(cpu_t *cp);
extern cpupartid_t	cpupart_query_thread(kthread_id_t tp);
extern int		cpupart_attach_cpu(cpupartid_t cpid, cpu_t *cp);
extern int		cpupart_get_level(cpupartid_t cpid, uint_t *levelp);
extern int		cpupart_get_cpus(cpupartid_t cpid, processorid_t *cpus,
				    uint_t *numcpus);
extern int		cpupart_bind_thread(kthread_id_t tp, cpupartid_t cpid);
extern void		cpupart_migrate(void);

extern cpupart_t	*cp_list_head;
extern kmutex_t		cp_list_lock;

/* platform-specific hook for exec */
extern void		(*cpupart_exec_hook)();

extern int		num_partitions;

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_CPUPART_H */
