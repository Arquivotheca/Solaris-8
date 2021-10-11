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
 *	Copyright (c) 1986-1989,1997-1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 *	Copyright (c) 1983-1989 by AT&T.
 *	All rights reserved.
 */

#ifndef	_SYS_SWAP_H
#define	_SYS_SWAP_H

#pragma ident	"@(#)swap.h	1.38	99/10/05 SMI"

#include <sys/isa_defs.h>
#include <sys/feature_tests.h>
#include <vm/anon.h>
#include <sys/fs/swapnode.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if !defined(_LP64) && _FILE_OFFSET_BITS == 64
#error  "Cannot use swapctl in the large files compilation environment"
#endif

/* The following are for the swapctl system call */

#define	SC_ADD		1	/* add a specified resource for swapping */
#define	SC_LIST		2	/* list all the swapping resources */
#define	SC_REMOVE	3	/* remove the specified swapping resource */
#define	SC_GETNSWP	4	/* get number of swap resources configured */
#define	SC_AINFO	5	/* get anonymous memory resource information */

typedef struct swapres {
	char	*sr_name;	/* pathname of the resource specified */
	off_t	sr_start;	/* starting offset of the swapping resource */
	off_t 	sr_length;	/* length of the swap area */
} swapres_t;

typedef struct swapent {
	char 	*ste_path;	/* get the name of the swap file */
	off_t	ste_start;	/* starting block for swapping */
	off_t	ste_length;	/* length of swap area */
	long	ste_pages;	/* numbers of pages for swapping */
	long	ste_free;	/* numbers of ste_pages free */
	int	ste_flags;	/* see below */
} swapent_t;

typedef struct swaptable {
	int	swt_n;			/* number of swapents following */
	struct	swapent	swt_ent[1];	/* array of swt_n swapents */
} swaptbl_t;


#if defined(_SYSCALL32)

/* Kernel's view of user ILP32 swapres and swapent structures */

typedef struct swapres32 {
	caddr32_t sr_name;	/* pathname of the resource specified */
	off32_t	sr_start;	/* starting offset of the swapping resource */
	off32_t	sr_length;	/* length of the swap area */
} swapres32_t;

typedef struct swapent32 {
	caddr32_t ste_path;	/* get the name of the swap file */
	off32_t	ste_start;	/* starting block for swapping */
	off32_t	ste_length;	/* length of swap area */
	int32_t	ste_pages;	/* numbers of pages for swapping */
	int32_t	ste_free;	/* numbers of ste_pages free */
	int32_t	ste_flags;	/* see below */
} swapent32_t;

typedef struct	swaptable32 {
	int32_t	swt_n;			/* number of swapents following */
	struct	swapent32 swt_ent[1];	/* array of swt_n swapents */
} swaptbl32_t;

#endif	/* _SYSCALL32 */

#if defined(_KERNEL)
extern int swapctl(int, void *, int *);
#if defined(_LP64) && defined(_SYSCALL32)
extern int swapctl32(int, void *, int *);
#endif /* _LP64 && _SYSCALL32 */
#else /* !_KERNEL */
#if defined(__STDC__)
extern int swapctl(int, void *);
#else
extern int swapctl();
#endif
#endif /* _KERNEL */


/* ste_flags values */

#define	ST_INDEL	0x01		/* Deletion of file is in progress. */
					/* Prevents others from deleting or */
					/* allocating from it */
#define	ST_DOINGDEL	0x02		/* Set during deletion of file */
					/* Clearing during deletion signals */
					/* that you want to add the file back */
					/* again, and will eventually cause */
					/* it to be added back */

/*
 * VM - virtual swap device.
 */
struct	swapinfo {
	ulong_t si_soff;		/* starting offset (bytes) of file */
	ulong_t si_eoff;		/* ending offset (bytes) of file */
	struct	vnode *si_vp;		/* vnode (commonvp if device) */
	struct	swapinfo *si_next;	/* next swap area */
	int	si_allocs;		/* # of conseq. allocs from this area */
	short	si_flags;		/* flags defined below */
	pgcnt_t	si_npgs;		/* number of pages of swap space */
	pgcnt_t	si_nfpgs;		/* number of free pages of swap space */
	int 	si_pnamelen;		/* swap file name length + 1 */
	char 	*si_pname;		/* swap file name */
	ssize_t	si_mapsize;		/* # bytes allocated for bitmap */
	uint_t 	*si_swapslots;		/* bitmap of slots, unset == free */
	pgcnt_t	si_hint;		/* first page to check if free */
	ssize_t	si_checkcnt;		/* # of checks to find freeslot */
	ssize_t	si_alloccnt;		/* used to find ave checks */
};

/*
 * Stuff to convert an anon slot pointer to a page name.
 * Because the address of the slot (ap) is a unique identifier, we
 * use it to generate a unique (vp,off), as shown below.
 *
 *  	|<-- PAGESHIFT -->|<------32 - PAGESHIFT------>|
 *	   vp index bits	off bits
 *
 * The off bits are shifted PAGESHIFT to directly form a page aligned
 * offset; the vp index bits map 1-1 to a vnode.
 *
 * Note: if we go to 64 bit offsets, we could use all the bits as the
 * unique offset and just have one vnode.
 */

		/* XXX64	change to 64 bits */
#define	AN_OFFSHIFT	(PAGESHIFT)		/* off shift */
#define	AN_VPSHIFT	(32 - AN_OFFSHIFT)	/* vp shift */
#define	AN_VPSIZE	(1 << AN_OFFSHIFT)	/* # of possible vp indexes */

/*
 * Convert from an anon slot to associated vnode and offset.
 */
#define	swap_xlate(AP, VPP, OFFP)					\
{									\
	*(VPP) = (AP)->an_vp;						\
	*(OFFP) = (AP)->an_off;						\
}
#define	swap_xlate_nopanic	swap_xlate


/*
 * Get a vnode name for an anon slot
 */
#define	swap_alloc(AP)							\
{									\
	(AP)->an_vp = swapfs_getvp((uintptr_t)(AP) >> AN_VPSHIFT);	\
	(AP)->an_off = (anoff_t)((uintptr_t)(AP) << AN_OFFSHIFT);	\
}

/*
 * Free the page name for the specified anon slot.
 * For now there's nothing to do.
 */
#define	swap_free(AP)


/* Flags for swap_phys_alloc */
#define	SA_NOT 	0x01 	/* Must have slot from swap dev other than input one */

/* Special error codes for swap_newphysname() */
#define	SE_NOSWAP	-1	/* No physical swap slots available */
#define	SE_NOANON	-2	/* No anon slot for swap slot */

#ifdef _KERNEL
extern struct anon *swap_anon(struct vnode *vp, u_offset_t off);
extern int swap_phys_alloc(struct vnode **vpp, u_offset_t *offp, size_t *lenp,
    uint_t flags);
extern void swap_phys_free(struct vnode *vp, u_offset_t off, size_t len);
extern int swap_getphysname(struct vnode *vp, u_offset_t off,
    struct vnode **pvpp, u_offset_t *poffp);
extern int swap_newphysname(struct vnode *vp, u_offset_t offset,
    u_offset_t *offp, size_t *lenp, struct vnode **pvpp, u_offset_t *poffp);

extern struct swapinfo *swapinfo;
extern int swap_debug;
#endif	/* _KERNEL */

#ifdef SWAP_DEBUG
#define	SW_RENAME	0x01
#define	SW_RESV		0x02
#define	SW_ALLOC	0x04
#define	SW_CTL		0x08
#define	SWAP_PRINT(f, s, x1, x2, x3, x4, x5) \
		if (swap_debug & f) \
			printf(s, x1, x2, x3, x4, x5);
#else	/* SWAP_DEBUG */
#define	SWAP_PRINT(f, s, x1, x2, x3, x4, x5)
#endif	/* SWAP_DEBUG */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SWAP_H */
