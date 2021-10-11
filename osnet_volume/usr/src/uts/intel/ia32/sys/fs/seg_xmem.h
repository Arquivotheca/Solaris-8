/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#ifndef	_IA32_SYS_FS_SEG_XMEM_H
#define	_IA32_SYS_FS_SEG_XMEM_H

#pragma ident	"@(#)seg_xmem.h	1.1	99/05/25 SMI"

#include <sys/map.h>

#ifdef	__cplusplus
extern "C" {
#endif


/*
 * Statistics for segxmem operations.
 *
 * No explicit locking to protect these stats.
 */
struct segxmemcnt {
	kstat_named_t	sx_fault;	/* number of segxmem_faults */
	kstat_named_t	sx_getmapflt;	/* number of segxmem_getmaps */
	kstat_named_t	sx_release;	/* releases with */
	kstat_named_t	sx_pagecreate;	/* pagecreates */
};


#if	defined(_KERNEL)

struct segxmem_crargs {
	struct	vnode	*xma_vp;	/* vnode maped from */
	u_offset_t	xma_offset;	/* starting offset for mapping */
	/* offset above could be invalid due to remaps, but ppa will be valid */
	page_t		***xma_ppa;	/* page list for this mapping */
	uint_t		xma_bshift;	/* for converting offset to block # */
	struct	cred	*xma_cred;	/* credentials */
	uchar_t		xma_prot;
};


struct	sx_prot {
	struct	sx_prot	*spc_next;	/* Next such one */
	pgcnt_t		spc_pageindex;	/* First page with changed prot */
	pgcnt_t		spc_numpages;	/* & number of such pages */
	uchar_t		spc_prot;
};

struct	segxmem_data {
	struct vnode	*sxd_vp;	/* vnode for this mapping */
	offset_t	sxd_offset;	/* & initial offset */
	/*
	 * The above may not be valid after remap, but ppa below will track
	 * the remaps.
	 */
	size_t		sxd_bsize;	/* block size */
	uint_t		sxd_bshift;	/* for converting offset to block # */
	size_t		sxd_softlockcnt;
	struct sx_prot	*sxd_spc;	/* linked list of changed protections */
	uchar_t		sxd_prot;
};

#define	sx_blocks(seg, sxd)	howmany((seg)->s_size, 1 << (sxd)->sxd_bshift)

/*
 * Public seg_xmem segment operations.
 */
extern int	segxmem_create(struct seg *, struct segxmem_crargs *);
/*
extern faultcode_t segxmem_fault(struct hat *, struct seg *, caddr_t, size_t,
		enum fault_type, enum seg_rw);
*/
extern caddr_t	segxmem_getmap(struct map *, struct vnode *, u_offset_t,
		size_t, page_t **, enum seg_rw);
extern void	segxmem_release(struct map *, caddr_t, size_t);
extern int	segxmem_remap(struct seg *, struct vnode *vp,  caddr_t, size_t,
		page_t ***, uchar_t);
extern void	segxmem_inval(struct seg *, struct vnode *, u_offset_t);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _IA32_SYS_FS_SEG_XMEM_H */
