/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef	_SYS_FS_SWAPNODE_H
#define	_SYS_FS_SWAPNODE_H

#pragma ident	"@(#)swapnode.h	1.17	98/07/14 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

struct swapnode {
	struct swapnode		*swap_next;
	ulong_t			swap_vnum;	/* Unique id of this swapnode */
	struct vnode    	swap_vnode;
};

/*
 * Macros to convert a vnode to a swapnode, and vice versa.
 */
#define	VPTOSWAP(vp) ((struct swapnode *)((vp)->v_data))
#define	SWAPTOVP(swap) (&(swap)->swap_vnode)

/*
 * pointer to swapfs global data structures
 */
extern pgcnt_t swapfs_minfree;		/* amount of availrmem (in pages) */
					/* that is unavailable to swapfs */
extern pgcnt_t swapfs_desfree;

extern pgcnt_t swapfs_reserve;		/* amount of availrmem (in pages) */
					/* that is unavailable for swap */
					/* reservation to non-priv processes */

extern struct vnodeops swap_vnodeops;
extern struct vfsops swap_vfsops;
extern struct vnode *swapfs_getvp(ulong_t);

#ifdef SWAPFS_DEBUG
extern int swapfs_debug;
#define	SWAPFS_PRINT(X, S, Y1, Y2, Y3, Y4, Y5)	\
	if (swapfs_debug & (X)) 		\
		printf(S, Y1, Y2, Y3, Y4, Y5);
#define	SWAP_SUBR	0x01
#define	SWAP_VOPS	0x02
#define	SWAP_VFSOPS	0x04
#define	SWAP_PGC		0x08
#define	SWAP_PUTP	0x10
#else	/* SWAPFS_DEBUG */
#define	SWAPFS_PRINT(X, S, Y1, Y2, Y3, Y4, Y5)
#endif	/* SWAPFS_DEBUG */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FS_SWAPNODE_H */
