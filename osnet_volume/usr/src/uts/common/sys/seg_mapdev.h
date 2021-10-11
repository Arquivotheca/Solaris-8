/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	Copyright (c) 1986-1990,1993,1994,1997-1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 *	Copyright (c) 1983-1989 by AT&T.
 *	All rights reserved.
 */


#ifndef	_SYS_SEG_MAPDEV_H
#define	_SYS_SEG_MAPDEV_H

#pragma ident	"@(#)seg_mapdev.h	1.11	98/01/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Structure whose pointer is passed to the segmapdev_create routine
 */

struct segmapdev_crargs {
	int	(*mapfunc)(dev_t dev, off_t off, int prot); /* map function */
	dev_t	dev;		/* device number */
	u_offset_t offset;	/* starting offset */
	uchar_t	prot;		/* protection */
	uchar_t	maxprot;	/* maximum protection */
	uint_t	flags;
	struct ddi_mapdev_ctl *m_ops;	/* Mapdev ops struct */
	void	*private_data;		/* Driver private data */
	ddi_mapdev_handle_t *handle;	/* Return the address of the segment */
};

struct segmapdev_ctx {
	kmutex_t		lock;
	kcondvar_t		cv;
	dev_t			dev; /* Device to which we are mapping */
	int			refcnt; /* Number of threads with mappings */
	uint_t			oncpu;	/* this context is running on a cpu */
	timeout_id_t		timeout; /* Timeout ID */
	struct segmapdev_ctx	*next;
	ulong_t			id;	/* handle grouping id */
};

/*
 * (Semi) private data maintained by the segmapdev driver per
 * segment mapping
 *
 * The segment lock is necessary to protect fields that are modified
 * when the "read" version of the address space lock is held.  This lock
 * is not needed when the segment operation has the "write" version of
 * the address space lock (it would be redundant).
 *
 * The following fields in segdev_data are read-only when the address
 * space is "read" locked, and don't require the segment lock:
 *
 *	vp
 *	offset
 *	mapfunc
 *	maxprot
 */
struct segmapdev_data {
	kmutex_t	lock;	/* protects segdev_data */
	kcondvar_t	wait;	/* makes driver callback single threaded */
	int	(*mapfunc)(dev_t dev, off_t off, int prot);
				/* really returns struct pte, not int */
	u_offset_t	offset;	/* device offset for start of mapping */
	struct	vnode *vp;	/* vnode associated with device */
	uchar_t	pageprot;	/* true if per page protections present */
	uchar_t	prot;		/* current segment prot if pageprot == 0 */
	uchar_t	maxprot;	/* maximum segment protections */
	uint_t	flags;		/* Fault handling flags */
	struct	vpage *vpage;	/* per-page protection information, if needed */
	uchar_t	pagehat_flags;	/* true if per page hat_access_flags */
	uint_t	hat_flags;	/* current HAT FLAGS segment */
	uint_t	*vpage_hat_flags;	/* per-page information, if needed */
	uchar_t	*vpage_inter;	/* per-pages to intercept/nointercpet */
	struct hat	*hat;	/* hat used to fault segment in */
	enum fault_type	type;	/* type of fault */
	enum seg_rw	rw;	/* type of access at fault */
	void	*private_data;
	dev_t	dev;		/* Device doing the mapping */
	struct segmapdev_ctx *devctx;
	struct ddi_mapdev_ctl m_ops;  /* Mapdev ops struct */
	clock_t	timeout_length; /* Number of clock ticks to keep ctx */
};

/*
 * Flags used by the segment fault handling routines.
 */
#define	SEGMAPDEV_INTER		0x01 /* Driver interested in faults */
#define	SEGMAPDEV_FAULTING	0x02 /* Segment is faulting */

#ifdef _KERNEL

static int segmapdev_create(struct seg *, void *);
static int segmapdev_inter(struct seg *, caddr_t, off_t);
static int segmapdev_nointer(struct seg *, caddr_t, off_t);
static int segmapdev_set_access_attr(struct seg *, caddr_t, off_t, uint_t);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SEG_MAPDEV_H */
