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
 *	Copyright (c) 1986-1990,1994,1997-1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 *	Copyright (c) 1983-1989 by AT&T.
 *	All rights reserved.
 */

#ifndef	_VM_MHAT_H
#define	_VM_MHAT_H

#pragma ident	"@(#)mhat.h	1.73	98/01/06 SMI"
/*	From:	SVr4.0	"kernel:vm/hat.h	1.9"		*/

#include <sys/types.h>
#include <sys/t_lock.h>
#include <vm/faultcode.h>
#include <vm/page.h>
#include <vm/mach_page.h>
#include <sys/kstat.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	HAT_SUPPORTED_LOAD_FLAGS (HAT_LOAD | HAT_LOAD_LOCK | HAT_LOAD_ADV |\
		HAT_LOAD_CONTIG | HAT_LOAD_NOCONSIST |\
		HAT_LOAD_SHARE | HAT_LOAD_REMAP)

/*
 * VM - Hardware Address Translation management.
 *
 * This file describes the machine independent interfaces to
 * the hardware address translation management routines.  Other
 * machine specific interfaces and structures are defined
 * in <vm/hat_xxx.h>.  The hat layer manages the address
 * translation hardware as a cache driven by calls from the
 * higher levels of the VM system.
 */


#define	HAT_PRIVSIZ 4		/* number of words of private data storage */

struct hat {
	struct	hatops	*hat_op;	/* public ops for this hat */
	struct	hat	*hat_next;	/* for address space list */
	struct	as	*hat_as;	/* as this hat provides mapping for */
	uint_t	hat_data[HAT_PRIVSIZ];	/* private data optimization */
	kmutex_t hat_mutex;		/* protects hat, hatops */
	kmutex_t hat_unload_other;	/* protects UNLOAD_OTHER & free */
};

/*
 * The hment entry, hat mapping entry.
 * The mmu independent translation on the mapping list for a page
 */
struct hment {
	struct	page *hme_page;		/* what page this maps */
	struct	hment *hme_next;	/* next hment */
	uint_t	hme_hat : 16;		/* index into hats */
	uint_t	hme_impl : 8;		/* implementation hint */
	uint_t	hme_notused : 2;	/* extra bits */
	uint_t	hme_prot : 2;		/* protection */
	uint_t	hme_noconsist : 1;	/* mapping can't be kept consistent */
	uint_t	hme_ncpref: 1;		/* consistency resolution preference */
	uint_t	hme_nosync : 1;		/* ghost unload flag */
	uint_t	hme_valid : 1;		/* hme loaded flag */
	struct	hment *hme_prev;	/* prev hment */
};

/*
 * The hat operations
 */
struct hatops {
	void		(*h_init)(void);
	void		(*h_alloc)(struct hat *, struct as *);

	struct as 	*(*h_setup)(struct as *, int);
	void		(*h_free)(struct hat *, struct as *);
	int		(*h_dup)(struct hat *, struct as *, struct as *);
	void		(*h_swapin)(struct hat *, struct as *);
	void		(*h_swapout)(struct hat *, struct as *);

	void		(*h_memload)(struct hat *, struct as *, caddr_t, \
			    struct page *, uint_t, int);
	void		(*h_devload)(struct hat *, struct as *, caddr_t, \
			    devpage_t *, uint_t, uint_t, int);
	void		(*h_contig_memload)(struct hat *, struct as *, \
			    caddr_t, struct page *, uint_t, int, uint_t);
	void		(*h_contig_devload)(struct hat *, struct as *, \
			    caddr_t, devpage_t *, uint_t, uint_t, int, uint_t);
	void		(*h_unlock)(struct hat *, struct as *, caddr_t, uint_t);
	faultcode_t	(*h_fault)(struct hat *, caddr_t);

	void		(*h_chgprot)(struct as *, caddr_t, uint_t, uint_t);
	void		(*h_unload)(struct as *, caddr_t, uint_t, int);
	void		(*h_sync)(struct as *, caddr_t, uint_t, uint_t);

	void		(*h_pageunload)(struct page *, struct hment *);
	int		(*h_pagesync)(struct hat *, struct page *, \
			    struct hment *, uint_t);
	void		(*h_pagecachectl)(struct page *, uint_t);

	uint_t		(*h_getpfnum)(struct as *, caddr_t);

	int		(*h_map)(struct hat *, struct as *, caddr_t,
				uint_t, int);
	int		(*h_probe)(struct hat *, struct as *, caddr_t);
	void		(*h_lock_init)();
	int		(*h_share)(struct as *, caddr_t, struct as *, \
			    caddr_t, uint_t);
	void		(*h_unshare)(struct as *, caddr_t, uint_t);
	void		(*h_do_attr)(struct as *, caddr_t, size_t, uint_t, \
				int);
	uint_t		(*h_getattr)(struct as *, caddr_t, uint_t *);
	faultcode_t	(*h_softlock)(struct hat *, caddr_t, size_t *, \
				struct page **, uint_t);
	faultcode_t	(*h_pageflip)(struct hat *, caddr_t, caddr_t,
				size_t *, struct page **, struct page **);

};

/*
 * Special flag for SX to to tmpunload. Note that generic hat requires
 * machine dependent hat uses bits in 0xff000000. SRMMU uses most of
 * those 8 bits so this is the only bit left for SX. See hat.h and hat_srmmu.h.
 */
#define	SX_TMPUNLOAD	(0x80000000)

/*
 * Modes used for mmu_do_attr()
 */
#define	MHAT_CLRATTR	(1)
#define	MHAT_SETATTR	(2)
#define	MHAT_CHGATTR	(3)

/*
 * Return code for hat_pagesync & pageunoad.
 */
#define	HAT_DONE	0x0
#define	HAT_RESTART	0x1
#define	HAT_VAC_DONE	0x2

/*
 * srmmu specific flags for page_t
 */
#define	P_PNC	0x8		/* non-caching is permanent bit */
#define	P_TNC	0x10		/* non-caching is temporary bit */

#define	PP_GENERIC_ATTR(pp)	(((machpage_t *)(pp))->p_nrm & \
				    (P_MOD | P_REF | P_RO))
#define	PP_ISMOD(pp)		(((machpage_t *)(pp))->p_nrm & P_MOD)
#define	PP_ISREF(pp)		(((machpage_t *)(pp))->p_nrm & P_REF)
#define	PP_ISNC(pp)		(((machpage_t *)(pp))->p_nrm & (P_PNC|P_TNC))
#define	PP_ISPNC(pp)		(((machpage_t *)(pp))->p_nrm & P_PNC)
#define	PP_ISTNC(pp)		(((machpage_t *)(pp))->p_nrm & P_TNC)
#define	PP_ISRO(pp)		(((machpage_t *)(pp))->p_nrm & P_RO)

#define	PP_SETMOD(pp)		(((machpage_t *)(pp))->p_nrm |= P_MOD)
#define	PP_SETREF(pp)		(((machpage_t *)(pp))->p_nrm |= P_REF)
#define	PP_SETREFMOD(pp)	(((machpage_t *)(pp))->p_nrm |= (P_REF|P_MOD))
#define	PP_SETPNC(pp)		(((machpage_t *)(pp))->p_nrm |= P_PNC)
#define	PP_SETTNC(pp)		(((machpage_t *)(pp))->p_nrm |= P_TNC)
#define	PP_SETRO(pp)		(((machpage_t *)(pp))->p_nrm |= P_RO)
#define	PP_SETREFRO(pp)		(((machpage_t *)(pp))->p_nrm |= (P_REF|P_RO))

#define	PP_CLRMOD(pp)		(((machpage_t *)(pp))->p_nrm &= ~P_MOD)
#define	PP_CLRREF(pp)		(((machpage_t *)(pp))->p_nrm &= ~P_REF)
#define	PP_CLRREFMOD(pp)	(((machpage_t *)(pp))->p_nrm &= ~(P_REF|P_MOD))
#define	PP_CLRPNC(pp)		(((machpage_t *)(pp))->p_nrm &= ~P_PNC)
#define	PP_CLRTNC(pp)		(((machpage_t *)(pp))->p_nrm &= ~P_TNC)
#define	PP_CLRRO(pp)		(((machpage_t *)(pp))->p_nrm &= ~P_RO)

/*
 * The entries of the table of hat types.
 */
struct hatsw {
	char		*hsw_name;	/* type name string */
	struct hatops	*hsw_ops;	/* hat operations vector */
};

extern	struct hatsw hattab[];
extern	int nhattab;			/* # of entries in hattab array */

#ifdef	_KERNEL

/*
 * Macros for the hat operations
 */

#define	HATOP_ALLOC(hat, as) \
		(*(hat)->hat_op->h_alloc)(hat, as)

#define	HATOP_SETUP(as, flag) \
		(*(as)->a_hat->hat_op->h_setup)(as, flag)

#define	HATOP_FREE(hat, as) \
		(*(hat)->hat_op->h_free)(hat, as)

#define	HATOP_DUP(hat, as, newas) \
		(*(hat)->hat_op->h_dup)(hat, as, newas)

#define	HATOP_SWAPIN(hat, as) \
		(*(hat)->hat_op->h_swapin)(hat, as)

#define	HATOP_SWAPOUT(hat, as) \
		(*(hat)->hat_op->h_swapout)(hat, as)

#define	HATOP_MEMLOAD(hat, as, addr, pp, prot, flags) \
		(*(hat)->hat_op->h_memload)(hat, as, addr, pp, prot, flags)

#define	HATOP_DEVLOAD(hat, as, addr, dp, pf, prot, flags) \
		(*(hat)->hat_op->h_devload)(hat, as, addr, dp, pf, prot, flags)

#define	HATOP_CONTIG_MEMLOAD(hat, as, addr, pp, prot, flags, len) \
		(*(hat)->hat_op->h_contig_memload)(hat, as, addr, \
			pp, prot, flags, len)

#define	HATOP_CONTIG_DEVLOAD(hat, as, addr, dp, pf, prot, flags, len) \
		(*(hat)->hat_op->h_contig_devload)(hat, as, addr, \
			dp, pf, prot, flags, len)

#define	HATOP_UNLOCK(hat, as, addr, len) \
		(*(hat)->hat_op->h_unlock)(hat, as, addr, len)

#define	HATOP_FAULT(hat, addr) \
		(*(hat)->hat_op->h_fault)(hat, addr)

#define	HATOP_CHGPROT(hat, as, addr, len, vprot) \
		(*(hat)->hat_op->h_chgprot)(as, addr, len, vprot)

#define	HATOP_UNLOAD(hat, as, addr, len, flags) \
		(*(hat)->hat_op->h_unload)(as, addr, len, flags)

#define	HATOP_SYNC(hat, as, addr, len, flags) \
		(*(hat)->hat_op->h_sync)(as, addr, len, flags)

#define	HATOP_PAGEUNLOAD(hat, pp, hme) \
		(*(hat)->hat_op->h_pageunload)(pp, hme)

#define	HATOP_PAGESYNC(hat, pp, hme, flag) \
		(*(hat)->hat_op->h_pagesync)(hat, pp, hme, flag)

#define	HATOP_PAGECACHECTL(hsw, pp, flag) \
		(*(hsw)->hsw_ops->h_pagecachectl)(pp, flag)

#define	HATOP_GETKPFNUM(hat, addr) \
		(*(hat)->hat_op->h_getkpfnum)(addr)

#define	HATOP_GETPFNUM(hat, as, addr) \
		(*(hat)->hat_op->h_getpfnum)(as, addr)

#define	HATOP_MAP(hat, as, addr, len, flags) \
		(*(hat)->hat_op->h_map)(hat, as, addr, len, flags)

#define	HATOP_PROBE(hat, as, addr) \
		(*(hat)->hat_op->h_probe)(hat, as, addr)

#define	HATOP_SHARE(hat, das, daddr, sas, saddr, len) \
		(*(hat)->hat_op->h_share)(das, daddr, sas, saddr, len)

#define	HATOP_UNSHARE(hat, as, addr, len) \
		(*(hat)->hat_op->h_unshare)(as, addr, len)

#define	HATOP_UNMAP(hat, as, addr, len, flags) \
		(*(hat)->hat_op->h_unmap)(as, addr, len, flags)

#define	HATOP_GETATTR(hat, as, addr, attr) \
		(*(hat)->hat_op->h_getattr)(as, addr, attr)

#define	HATOP_DO_ATTR(hat, as, addr, len, attr, mode) \
		(*(hat)->hat_op->h_do_attr)(as, addr, len, attr, mode)

#define	HATOP_SOFTLOCK(hat, addr, lenp, ppp, flags) \
		(*(hat)->hat_op->h_softlock)(hat, addr, lenp, ppp, flags)

#define	HATOP_PAGEFLIP(hat, addr_to, kaddr, lenp, pp_to, pp_from) \
		(*(hat)->hat_op->h_pageflip)(hat, addr_to, kaddr, lenp, \
		pp_to, pp_from)

/*
 * SRMMU specific hat functions
 */
void	hat_pagecachectl(struct page *, int);
void	hat_page_badecc(ulong_t);

/* flags for hat_pagecachectl */
#define	HAT_CACHE	0x0
#define	HAT_UNCACHE	0x1
#define	HAT_TMPNC	0x2

/*
 * Old Hat locking functions
 */
void	ohat_mlist_enter(struct page *);
void	ohat_mlist_exit(struct page *);
int	ohat_mlist_held(struct page *);

/*
 * Old Hat memory error code
 */
int hat_kill_procs(struct page *, caddr_t);

struct hat *ohat_alloc(struct as *, struct hatops *);
void	ohat_free(struct hat *, struct as *);

void	ohat_contig_memload(struct hat *, struct as *,
		    caddr_t, struct page *, uint_t, int, uint_t);

void	hme_sub(struct hment *hme, page_t *pp);
void	hme_add(struct hment *hme, page_t *pp);

extern int nhats;
extern struct hat *hats;
extern struct hat *hatsNHATS;

#endif /* _KERNEL */
#ifdef	__cplusplus
}
#endif

#endif	/* _VM_MHAT_H */
