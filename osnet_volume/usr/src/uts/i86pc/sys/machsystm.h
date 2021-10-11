/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_MACHSYSTM_H
#define	_SYS_MACHSYSTM_H

#pragma ident	"@(#)machsystm.h	1.14	99/03/23 SMI"

/*
 * Numerous platform-dependent interfaces that don't seem to belong
 * in any other header file.
 *
 * This file should not be included by code that purports to be
 * platform-independent.
 *
 */

#include <sys/machparam.h>
#include <sys/varargs.h>
#include <sys/thread.h>
#include <sys/cpuvar.h>
#include <vm/mach_page.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _KERNEL

extern processorid_t getbootcpuid(void);
extern void mp_halt(char *);

extern int Cpudelay;
extern void setcpudelay(void);

extern void init_intr_threads(struct cpu *);
extern void init_clock_thread(void);

extern void send_dirint(int, int);
extern void siron(void);

extern void return_instr(void);

extern int pokefault;

struct memconf {
	pfn_t	mcf_spfn;	/* begin page fram number */
	pfn_t	mcf_epfn;	/* end page frame number */
};
struct mmuinfo {
	int	mmu_node_priv_pages;
					/*
					 * node private pages. The number of
					 * pages that should be pre allocated
					 * for the purpose of replication.
					 */
	size_t	mmu_extra_pp_sz;
					/*
					 * Extra bytes required for every
					 * page structure in addition to
					 * sizeof (machpage_t).
					 */
	caddr_t	mmu_extra_ppp;
					/*
					 * pointer to  a space, big enough
					 * to contain extra bytes for pages
					 * known at startup time.
					 */
	pfn_t	mmu_highest_pfn;
					/*
					 * highest page frame number that can
					 * be mapped by the mmu module
					 */
	caddr_t	mmu_name;
};

struct system_hardware {
	int		hd_nodes;		/* number of nodes */
	int		hd_cpus_per_node; 	/* max cpus in a node */
	struct memconf 	hd_mem[MAXNODES];
						/*
						 * memory layout for each
						 * node.
						 */
};
extern struct system_hardware system_hardware;
extern struct mmuinfo mmuinfo;
extern void get_system_configuration(void);
extern void psm_pageinit(machpage_t *, uint32_t);
extern void mmu_init(void);
extern void post_startup_mmu_initialization(void);
extern void mmu_setup_kvseg(pfn_t);
extern int cpuid2nodeid(int);
extern void *kmem_node_alloc(size_t, int, int);
#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_MACHSYSTM_H */
