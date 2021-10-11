/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 */

#ifndef _SYS_MEMNODE_H
#define	_SYS_MEMNODE_H

#pragma ident	"@(#)memnode.h	1.6	97/05/19 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This file defines the mappings between physical addresses and memory
 * nodes. Memory nodes are defined by log2(MAX_MEM_NODES) highest order bits
 * of physical address of cacheable memory. Current sun4u machines have all
 * memory at node 0. On wildfire MAX_MEM_NODES is 4 and mem_node_pfn_shift is
 * 25.
 */

/* This macro will differ for NUMA platforms */
#define	MAX_MEM_NODES	(1)

#define	PFN_2_MEM_NODE(pfn)		\
	(((pfn) >> mem_node_pfn_shift) & (MAX_MEM_NODES - 1))

/*
 * Extract the node's offset part of the pfn. That is all the bits
 * of the pfn except the top ones that designate node id.
 */

#define	PFN_2_MEM_NODE_OFF(pfn)		\
		((pfn) & ~((MAX_MEM_NODES - 1) << mem_node_pfn_shift))

#define	MEM_NODE_2_PFN(mnode, mnoff)	\
		(((mnode) << mem_node_pfn_shift) | (mnoff))

/* This macro will differ for NUMA platforms */
#define	CPUID_2_MEM_NODE(cpuid)		(0)

struct	mem_node_conf {
	int	exists;
	pfn_t	physmax;
};

extern struct mem_node_conf	mem_node_config[];
extern int			mem_node_pfn_shift;

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MEMNODE_H */
