/*
 * Copyright (c) 1993, 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_SX_CMEMIO_H
#define	_SYS_SX_CMEMIO_H

#pragma ident	"@(#)sx_cmemio.h	1.19	98/01/06 SMI"

#include <sys/ioccom.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ioctl(2) commands for mapping in pre-allocated physically contiguous DRAM
 * for use by SX graphics libraries.
 */

#define	SX_CMEMIOC	('C' << 8)

#define	SX_CMEM_CREATE (SX_CMEMIOC | 1)

struct sx_cmem_create {
	uint_t	scm_size;
	int	scm_type;
	off_t	scm_offset;
};

/*
 * defines for scm_type
 */
#define	SX_CMEM_SHARED	0
#define	SX_CMEM_PRIVATE	1

/*
 * tmp ioctl for debugging, remove by fcs
 */

#define	SX_CMEM_VALID_VA	(SX_CMEMIOC | 2)

struct sx_cmem_valid_va {
	caddr_t	scm_vaddr;
	uint_t	scm_len;
	caddr_t	scm_base_vaddr;
	uint_t	scm_base_len;
};

#define	SX_CMEM_GET_RMAPINFO	(SX_CMEMIOC | 3)
/*
 * Size of the resource map for pre-allocated physically contiguous DRAM.
 */

#define	SX_CMEM_MAPSZ		0x200

#define	SX_CMEM_CHNK_NUM	100

#define	SX_CMEM_GET_CONFIG	(SX_CMEMIOC | 4)

struct sx_cmem_config {
	int	scm_meminstalled;	/* size of physical installed mem */
	int	scm_cmem_mbreq;	/* size of cmem requested */
	int	scm_cmem_mbrsv;	/* what's actually reserved */
	int	scm_cmem_mbleftreq;	/* size of mem should left for system */
	int	scm_cmem_mbleft;	/* what's actually left for system */
	int	scm_cmem_frag;	/* fragment allowed or not */
	int	scm_cmem_chunks;	/* # of reserved cmem chunks */
	int	scm_cmem_chunksz[SX_CMEM_CHNK_NUM]; /* size of each chunk */
};

#ifdef	_KERNEL

#define	SX_CMEM_MAPNAME	"sx_cmem_rmap"

/*
 * Each contiguous memory reservation is represented by a cmresv struct.
 * They can be found in two ways, by the device that created it thru
 * its clb_rlist list, and by its "offset" thru the cmem_table.
 */

struct cmresv {
	uint_t	cmr_offset;		/* unique offset for ever allocation */
	uint_t	cmr_size;		/* size of allocation */
	int	cmr_pfbase;		/* starting pfn */
	int	cmr_refcnt;		/* # of ref'ing segs and clones */
	void	*cmr_proc;		/* owner for private allocations */
	struct	cmresv *cmr_next;	/* next in list off cloneblk */
};

/*
 * Each open of the sx cmem device gets cloned so we can tell when it
 * gets closed.  There is one cloneblk for every CLONEBLKSZ active opens.
 */
#define	CLONEBLKSZ	12
struct cloneblk {
	char	clb_use[CLONEBLKSZ];		/* allocated or not */
	struct	cmresv *clb_rlist[CLONEBLKSZ];	/* list of cm reserved */
	struct	cloneblk *clb_next;		/* next cloneblk */
};

#define	CMTBLSZ		32
#define	CMSEGSZ		0x1000000
#define	CMEM_MKOFFSET(l1, l2)	((((l1) * CMTBLSZ) + (l2)) * CMSEGSZ)

/*
 * segment driver create function args
 */
struct segsx_cmem_crargs {
	uint_t	offset;
	uint_t	prot;
	dev_t	dev;
};

/*
 * segment driver private data
 */
struct segsx_cmem_data {
	struct	cmresv *cm;
	dev_t	dev;	/* Device number for this snode */
	uint_t	offset;
	uint_t	prot;	/* Protection for this segment */
	uint_t	maxprot; /* Maximum allowable protection for this segment */
	vnode_t	*vp;	/* vnode of the cmem device */
};

struct sx_cmem_list {
	page_t  *scl_lpp;	/* beginning page struct pointer */
	page_t  *scl_hpp;	/* ending page struct pointer */
	uint_t   scl_lpfn;	/* beginning page frame number */
	uint_t   scl_hpfn;	/* ending page frame number */
	uint_t   scl_pages;	/* number of contig pages */
	struct sx_cmem_list   *scl_next;
};


#endif	/* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_SX_CMEMIO_H */
