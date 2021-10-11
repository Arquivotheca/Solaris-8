/*
 * Copyright (c) 1986-1990, 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	Copyright (c) 1986-1990, 1997 by Sun Microsystems, Inc.
 * 	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 *
 */

#pragma ident	"@(#)vm_swap.c	1.90	98/12/14 SMI"
/*	From:	SVr4.0	"kernel:vm/vm_swap.c	1.47"		*/

/*
 * Each physical swap area has an associated bitmap representing
 * its physical storage. The bitmap records which swap slots are
 * currently allocated or freed.  Allocation is done by searching
 * through the bitmap for the first free slot. Thus, there's
 * no linear relation between offset within the swap device and the
 * address (within its segment(s)) of the page that the slot backs;
 * instead, it's an arbitrary one-to-one mapping.
 *
 * Associated with each swap area is a swapinfo structure.  These
 * structures are linked into a linear list that determines the
 * ordering of swap areas in the logical swap device.  Each contains a
 * pointer to the corresponding bitmap, the area's size, and its
 * associated vnode.
 */

#include <sys/types.h>
#include <sys/inttypes.h>
#include <sys/param.h>
#include <sys/t_lock.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/pathname.h>
#include <sys/cmn_err.h>
#include <sys/vtrace.h>
#include <sys/swap.h>
#include <sys/dumphdr.h>
#include <sys/debug.h>
#include <sys/fs/snode.h>
#include <sys/fs/swapnode.h>

#include <vm/as.h>
#include <vm/seg.h>
#include <vm/page.h>
#include <vm/seg_vn.h>
#include <vm/hat.h>
#include <vm/anon.h>
#include <vm/seg_map.h>

/*
 * To balance the load among multiple swap areas, we don't allow
 * more than swap_maxcontig allocations to be satisfied from a
 * single swap area before moving on to the next swap area.  This
 * effectively "interleaves" allocations among the many swap areas.
 */
int swap_maxcontig;	/* set by anon_init() to 1 Mb */

#define	MINIROOTSIZE	12000	/* ~6 Meg XXX */

/*
 * XXX - this lock is a kludge. It serializes some aspects of swapadd() and
 * swapdel() (namely VOP_OPEN, VOP_CLOSE, VN_RELE).  It protects against
 * somebody swapadd'ing and getting swap slots from a vnode, while someone
 * else is in the process of closing or rele'ing it.
 */
static kmutex_t swap_lock;

kmutex_t swapinfo_lock;

/*
 * protected by the swapinfo_lock
 */
struct swapinfo	*swapinfo;

static	struct	swapinfo *silast;
static	int	nswapfiles;

static u_offset_t	swap_getoff(struct swapinfo *);
static int	swapadd(struct vnode *, u_long, u_long, char *);
static int	swapdel(struct vnode *, u_long);
static int	swapslot_free(struct vnode *, u_offset_t, struct swapinfo *);

/*
 * swap device bitmap allocation macros
 */
#define	MAPSHIFT	5
#define	NBBW		(NBPW * NBBY)	/* number of bits per word */
#define	TESTBIT(map, i)		(((map)[(i) >> MAPSHIFT] & (1 << (i) % NBBW)))
#define	SETBIT(map, i)		(((map)[(i) >> MAPSHIFT] |= (1 << (i) % NBBW)))
#define	CLEARBIT(map, i)	(((map)[(i) >> MAPSHIFT] &= ~(1 << (i) % NBBW)))

int swap_debug = 0;	/* set for debug printf's */
int swap_verify = 0;	/* set to verify slots when freeing and allocating */

u_int swapalloc_maxcontig;

/*
 * Allocate a range of up to *lenp contiguous slots (page) from a physical
 * swap device. Flags are one of:
 *	SA_NOT  Must have a slot from a physical swap device other than the
 * 		the one containing input (*vpp, *offp).
 * Less slots than requested may be returned. *lenp allocated slots are
 * returned starting at *offp on *vpp.
 * Returns 1 for a successful allocation, 0 for couldn't allocate any slots.
 */
int
swap_phys_alloc(
	struct vnode **vpp,
	u_offset_t *offp,
	size_t *lenp,
	u_int flags)
{
	struct swapinfo *sip;
	offset_t soff, noff;
	size_t len;

	mutex_enter(&swapinfo_lock);
	sip = silast;

	/* Find a desirable physical device and allocate from it. */
	do {
		if (sip == NULL)
			break;
		if (!(sip->si_flags & ST_INDEL) &&
		    (spgcnt_t)sip->si_nfpgs > 0) {
			/* Caller wants other than specified swap device */
			if (flags & SA_NOT) {
				if (*vpp != sip->si_vp ||
				    *offp < sip->si_soff ||
				    *offp >= sip->si_eoff)
					goto found;
			/* Caller is loose, will take anything */
			} else
				goto found;
		} else if (sip->si_nfpgs == 0)
			sip->si_allocs = 0;
		if ((sip = sip->si_next) == NULL)
			sip = swapinfo;
	} while (sip != silast);
	mutex_exit(&swapinfo_lock);
	return (0);
found:
	soff = swap_getoff(sip);
	sip->si_nfpgs--;
	if (soff == -1) {
		cmn_err(CE_PANIC, "swap_alloc: swap_getoff failed!\n");
	}
	for (len = PAGESIZE; len < *lenp; len += PAGESIZE) {
		if (sip->si_nfpgs == 0)
			break;
		if (swapalloc_maxcontig && len >= swapalloc_maxcontig)
			break;
		noff = swap_getoff(sip);
		if (noff == -1) {
			break;
		} else if (noff != soff + len) {
			CLEARBIT(sip->si_swapslots, btop(noff - sip->si_soff));
			break;
		}
		sip->si_nfpgs--;
	}
	*vpp = sip->si_vp;
	*offp = soff;
	*lenp = len;
	ASSERT((spgcnt_t)sip->si_nfpgs >= 0);
	sip->si_allocs += btop(len);
	if (sip->si_allocs >= swap_maxcontig) {
		sip->si_allocs = 0;
		if ((silast = sip->si_next) == NULL)
			silast = swapinfo;
	}
	TRACE_2(TR_FAC_VM, TR_SWAP_ALLOC,
		"swap_alloc:vp %p offset %lx",
		sip->si_vp, soff);
	mutex_exit(&swapinfo_lock);
	return (1);
}

int swap_backsearch = 0;

/*
 * Get a free offset on swap device sip.
 * Return >=0 offset if succeeded, -1 for failure.
 */
static u_offset_t
swap_getoff(struct swapinfo *sip)
{
	u_int *sp, *ep;
	size_t aoff, boff, poff, slotnumber;

	ASSERT(MUTEX_HELD(&swapinfo_lock));

	sip->si_alloccnt++;
	for (sp = &sip->si_swapslots[sip->si_hint >> MAPSHIFT],
	    ep = &sip->si_swapslots[sip->si_mapsize / NBPW]; sp < ep; sp++) {
		if (*sp != (u_int) 0xffffffff)
			goto foundentry;
		else
			sip->si_checkcnt++;
	}
	SWAP_PRINT(SW_ALLOC,
	    "swap_getoff: couldn't find slot from hint %ld to end\n",
	    sip->si_hint, 0, 0, 0, 0);
	/*
	 * Go backwards? Check for faster method XXX
	 */
	if (swap_backsearch) {
		for (sp = &sip->si_swapslots[sip->si_hint >> MAPSHIFT],
		    ep = sip->si_swapslots; sp > ep; sp--) {
			if (*sp != (u_int) 0xffffffff)
				goto foundentry;
			else
				sip->si_checkcnt++;
		}
	} else {
		for (sp = sip->si_swapslots,
		    ep = &sip->si_swapslots[sip->si_hint >> MAPSHIFT];
		    sp < ep; sp++) {
			if (*sp != (u_int) 0xffffffff)
				goto foundentry;
			else
				sip->si_checkcnt++;
		}
	}
	if (*sp == 0xffffffff) {
		cmn_err(CE_WARN, "No free swap slots!\n");
		return ((u_offset_t)-1);
	}

foundentry:
	/*
	 * aoff is the page number offset (in bytes) of the si_swapslots
	 * array element containing a free page
	 *
	 * boff is the page number offset of the free page
	 * (i.e. cleared bit) in si_swapslots[aoff].
	 */
	aoff = ((char *)sp - (char *)sip->si_swapslots) * NBBY;

	for (boff = (sip->si_hint % NBBW); boff < NBBW; boff++) {
		if (!TESTBIT(sip->si_swapslots, aoff + boff))
			goto foundslot;
		else
			sip->si_checkcnt++;
	}
	for (boff = 0; boff < (sip->si_hint % NBBW); boff++) {
		if (!TESTBIT(sip->si_swapslots, aoff + boff))
			goto foundslot;
		else
			sip->si_checkcnt++;
	}
	cmn_err(CE_PANIC, "swap_getoff: didn't find slot in word hint %ld\n",
	    sip->si_hint);

foundslot:
	/*
	 * Return the offset of the free page in swap device.
	 * Convert page number of byte offset and add starting
	 * offset of swap device.
	 */
	slotnumber = aoff + boff;
	SWAP_PRINT(SW_ALLOC, "swap_getoff: allocating slot %ld\n",
	    slotnumber, 0, 0, 0, 0);
	poff = ptob(slotnumber);
	if (poff + sip->si_soff >= sip->si_eoff)
		printf("ptob(aoff(%ld) + boff(%ld))(%ld) >= eoff(%ld)\n",
		    aoff, boff, ptob(slotnumber), (long)sip->si_eoff);
	ASSERT(poff < sip->si_eoff);
	/*
	 * We could verify here that the slot isn't already allocated
	 * by looking through all the anon slots.
	 */
	SETBIT(sip->si_swapslots, slotnumber);
	sip->si_hint = slotnumber + 1;	/* hint = next slot */
	return (poff + sip->si_soff);
}

/*
 * Free a swap page.
 */
void
swap_phys_free(struct vnode *vp, u_offset_t off, size_t len)
{
	struct swapinfo *sip;
	ssize_t pagenumber, npage;

	mutex_enter(&swapinfo_lock);
	sip = swapinfo;

	do {
		if (sip->si_vp == vp &&
		    sip->si_soff <= off && off < sip->si_eoff) {
			for (pagenumber = btop(off - sip->si_soff),
			    npage = btop(len) + pagenumber;
			    pagenumber < npage; pagenumber++) {
				SWAP_PRINT(SW_ALLOC,
				    "swap_phys_free: freeing slot %ld on "
				    "sip %p\n",
				    pagenumber, sip, 0, 0, 0);
				if (!TESTBIT(sip->si_swapslots, pagenumber)) {
					cmn_err(CE_PANIC,
					    "swap_phys_free: freeing free slot "
					    "%p,%lx\n", (void *)vp,
					    ptob(pagenumber) + sip->si_soff);
				}
				CLEARBIT(sip->si_swapslots, pagenumber);
				sip->si_nfpgs++;
			}
			ASSERT(sip->si_nfpgs <= sip->si_npgs);
			mutex_exit(&swapinfo_lock);
			return;
		}
	} while ((sip = sip->si_next) != NULL);
	cmn_err(CE_PANIC, "swap_phys_free");
	/*NOTREACHED*/
}

/*
 * Return the anon struct corresponding for the given
 * <vnode, off> if it is part of the virtual swap device.
 * Return the anon struct if found, otherwise NULL.
 */
struct anon *
swap_anon(struct vnode *vp, u_offset_t off)
{
	struct anon *ap;

	ASSERT(MUTEX_HELD(&anonhash_lock[AH_LOCK(vp, off)]));

	for (ap = anon_hash[ANON_HASH(vp, off)]; ap != NULL; ap = ap->an_hash) {
		if (ap->an_vp == vp && ap->an_off == off)
			return (ap);
	}
	return (NULL);
}


/*
 * Determine if the vp offset range overlap a swap device.
 */
int
swap_in_range(struct vnode *vp, u_offset_t offset, size_t len)
{
	struct swapinfo *sip;
	u_offset_t eoff;

	eoff = offset + len;
	ASSERT(eoff > offset);

	mutex_enter(&swapinfo_lock);
	sip = swapinfo;
	if (vp && sip) {
		do {
			if (vp != sip->si_vp || eoff <= sip->si_soff ||
			    offset >= sip->si_eoff)
				continue;
			mutex_exit(&swapinfo_lock);
			return (1);
		} while ((sip = sip->si_next) != NULL);
	}
	mutex_exit(&swapinfo_lock);
	return (0);
}

/*
 * See if name is one of our swap files
 * even though lookupname failed.
 * This can be used by swapdel to delete
 * swap resources on remote machines
 * where the link has gone down.
 */
static struct vnode *
swapdel_byname(
	char 	*name,			/* pathname to delete */
	u_long lowblk) 	/* Low block number of area to delete */
{
	struct swapinfo **sipp, *osip;
	u_offset_t soff;

	/*
	 * Find the swap file entry for the file to
	 * be deleted. Skip any entries that are in
	 * transition.
	 */

	soff = ptob(btopr(lowblk << SCTRSHFT)); /* must be page aligned */

	mutex_enter(&swapinfo_lock);
	for (sipp = &swapinfo; (osip = *sipp) != NULL; sipp = &osip->si_next) {
		if ((strcmp(osip->si_pname, name) == 0) &&
		    (osip->si_soff == soff) && (osip->si_flags == 0)) {
			struct vnode *vp = osip->si_vp;

			VN_HOLD(vp);
			mutex_exit(&swapinfo_lock);
			return (vp);
		}
	}
	mutex_exit(&swapinfo_lock);
	return (NULL);
}


/*
 * New system call to manipulate swap files.
 */
int
swapctl(int sc_cmd, void *sc_arg, int *rv)
{
	struct swapinfo *sip, *csip, *tsip;
	int error = 0;
	struct swapent st, *ust;
	struct swapres sr;
	struct vnode *vp;
	int cnt = 0;
	int tmp_nswapfiles;
	int nswap;
	int length, nlen;
	int gplen = 0, plen;
	char *swapname;
	char *pname;
	char *tpname;
	struct anoninfo ai;
	spgcnt_t avail;

	switch (sc_cmd) {
	case SC_GETNSWP:
		*rv = nswapfiles;
		return (0);

	case SC_AINFO:
		/*
		 * Return anoninfo information with these changes:
		 * ani_max = maximum amount of swap space
		 *	(including potentially available physical memory)
		 * ani_free = amount of unallocated anonymous memory
		 *	(some of which might be reserved and including
		 *	 potentially available physical memory)
		 * ani_resv = amount of claimed (reserved) anonymous memory
		 */
		avail = MAX((spgcnt_t)(availrmem - swapfs_minfree), 0);
		ai.ani_max = (k_anoninfo.ani_max +
			k_anoninfo.ani_mem_resv) +avail;

		ai.ani_free = k_anoninfo.ani_free + avail;

		ai.ani_resv = k_anoninfo.ani_phys_resv +
		    k_anoninfo.ani_mem_resv;

		if (copyout(&ai, sc_arg, sizeof (struct anoninfo)) != 0)
			return (EFAULT);
		return (0);

	case SC_LIST:
		if (copyin(sc_arg, &length, sizeof (int)) != 0)
			return (EFAULT);
beginning:
		tmp_nswapfiles = nswapfiles;
		/* Return an error if not enough space for the whole table. */
		if (length < tmp_nswapfiles)
			return (ENOMEM);
		/*
		 * Get memory to hold the swap entries and their names. We'll
		 * copy the real entries into these and then copy these out.
		 * Allocating the pathname memory is only a guess so we may
		 * find that we need more and have to do it again.
		 * All this is because we have to hold the anon lock while
		 * traversing the swapinfo list, and we can't be doing copyouts
		 * and/or kmem_alloc()s during this.
		 */
		csip = kmem_zalloc(tmp_nswapfiles * sizeof (struct swapinfo),
		    KM_SLEEP);
retry:
		nlen = tmp_nswapfiles * (gplen += 100);
		pname = kmem_zalloc(nlen, KM_SLEEP);

		mutex_enter(&swapinfo_lock);

		if (tmp_nswapfiles != nswapfiles) {
			mutex_exit(&swapinfo_lock);
			kmem_free(pname, nlen);
			kmem_free(csip,
			    tmp_nswapfiles * sizeof (struct swapinfo));
			gplen = 0;
			goto beginning;
		}
		for (sip = swapinfo, tsip = csip, tpname = pname, nswap = 0;
		    sip && nswap < tmp_nswapfiles;
		    sip = sip->si_next, tsip++, tpname += plen, nswap++) {
			plen = sip->si_pnamelen;
			if (tpname + plen - pname > nlen) {
				mutex_exit(&swapinfo_lock);
				kmem_free(pname, nlen);
				goto retry;
			}
			*tsip = *sip;
			tsip->si_pname = tpname;
			(void) strcpy(tsip->si_pname, sip->si_pname);
		}
		mutex_exit(&swapinfo_lock);

		if (sip) {
			error = ENOMEM;
			goto lout;
		}
		ust = (swapent_t *)((swaptbl_t *)sc_arg)->swt_ent;
		for (tsip = csip, cnt = 0; cnt < nswap;  tsip++, ust++, cnt++) {
			if (copyin(ust, &st, sizeof (swapent_t)) != 0) {
				error = EFAULT;
				goto lout;
			}
			st.ste_flags = tsip->si_flags;
			st.ste_length =
			    (tsip->si_eoff - tsip->si_soff) >> SCTRSHFT;
			st.ste_start = tsip->si_soff >> SCTRSHFT;
			st.ste_pages = tsip->si_npgs;
			st.ste_free = tsip->si_nfpgs;
			if (copyout(&st, ust, sizeof (swapent_t)) != 0) {
				error = EFAULT;
				goto lout;
			}
			if (!tsip->si_pnamelen)
				continue;
			if (copyout(tsip->si_pname, st.ste_path,
				tsip->si_pnamelen) != 0) {
				error = EFAULT;
				goto lout;
			}
		}
		*rv = nswap;
lout:
		kmem_free(csip, tmp_nswapfiles * sizeof (struct swapinfo));
		kmem_free(pname, nlen);
		return (error);

	case SC_ADD:
	case SC_REMOVE:
		break;
	default:
		return (EINVAL);
	}
	if (!suser(CRED()))
		return (EPERM);

	if (copyin(sc_arg, &sr, sizeof (swapres_t)))
		return (EFAULT);

	/* Allocate the space to read in pathname */
	if ((swapname = kmem_alloc(MAXPATHLEN, KM_NOSLEEP)) == NULL)
		return (ENOMEM);

	error = copyinstr(sr.sr_name, swapname, MAXPATHLEN, 0);
	if (error)
		goto out;

	error = lookupname(swapname, UIO_SYSSPACE, FOLLOW, NULLVPP, &vp);
	if (error) {
		if (sc_cmd == SC_ADD)
			goto out;
		/* see if we match by name */
		vp = swapdel_byname(swapname, (size_t)sr.sr_start);
		if (vp == NULL)
			goto out;
	}

	if (vp->v_flag & (VNOMAP | VNOSWAP)) {
		VN_RELE(vp);
		error = ENOSYS;
		goto out;
	}
	switch (vp->v_type) {
	case VBLK:
		break;

	case VREG:
		if (vp->v_vfsp && (vp->v_vfsp->vfs_flag & VFS_RDONLY))
			error = EROFS;
		else
			error = VOP_ACCESS(vp, VREAD|VWRITE, 0, CRED());
		break;

	case VDIR:
		error = EISDIR;
		break;
	default:
		error = ENOSYS;
		break;
	}
	if (error == 0) {
		if (sc_cmd == SC_REMOVE)
			error = swapdel(vp, sr.sr_start);
		else
			error = swapadd(vp, sr.sr_start,
					sr.sr_length, swapname);
	}
	VN_RELE(vp);
out:
	kmem_free(swapname, MAXPATHLEN);
	return (error);
}

#if defined(_LP64) && defined(_SYSCALL32)

int
swapctl32(int sc_cmd, void *sc_arg, int *rv)
{
	struct swapinfo *sip, *csip, *tsip;
	int error = 0;
	struct swapent32 st, *ust;
	struct swapres32 sr;
	struct vnode *vp;
	int cnt = 0;
	int tmp_nswapfiles;
	int nswap;
	int length, nlen;
	int gplen = 0, plen;
	char *swapname;
	char *pname;
	char *tpname;
	struct anoninfo32 ai;
	size_t s;
	spgcnt_t avail;

	switch (sc_cmd) {
	case SC_GETNSWP:
		*rv = nswapfiles;
		return (0);

	case SC_AINFO:
		/*
		 * Return anoninfo information with these changes:
		 * ani_max = maximum amount of swap space
		 *	(including potentially available physical memory)
		 * ani_free = amount of unallocated anonymous memory
		 *	(some of which might be reserved and including
		 *	 potentially available physical memory)
		 * ani_resv = amount of claimed (reserved) anonymous memory
		 */
		avail = MAX((spgcnt_t)(availrmem - swapfs_minfree), 0);
		s = (k_anoninfo.ani_max + k_anoninfo.ani_mem_resv) + avail;
		if (s > UINT32_MAX)
			return (EOVERFLOW);
		ai.ani_max = s;

		s = k_anoninfo.ani_free + avail;
		if (s > UINT32_MAX)
			return (EOVERFLOW);
		ai.ani_free = s;

		s = k_anoninfo.ani_phys_resv + k_anoninfo.ani_mem_resv;
		if (s > UINT32_MAX)
			return (EOVERFLOW);
		ai.ani_resv = s;

		if (copyout(&ai, sc_arg, sizeof (ai)) != 0)
			return (EFAULT);
		return (0);

	case SC_LIST:
		if (copyin(sc_arg, &length, sizeof (int32_t)) != 0)
			return (EFAULT);
beginning:
		tmp_nswapfiles = nswapfiles;
		/* Return an error if not enough space for the whole table. */
		if (length < tmp_nswapfiles)
			return (ENOMEM);
		/*
		 * Get memory to hold the swap entries and their names. We'll
		 * copy the real entries into these and then copy these out.
		 * Allocating the pathname memory is only a guess so we may
		 * find that we need more and have to do it again.
		 * All this is because we have to hold the anon lock while
		 * traversing the swapinfo list, and we can't be doing copyouts
		 * and/or kmem_alloc()s during this.
		 */
		csip = kmem_zalloc(tmp_nswapfiles * sizeof (*csip), KM_SLEEP);
retry:
		nlen = tmp_nswapfiles * (gplen += 100);
		pname = kmem_zalloc(nlen, KM_SLEEP);

		mutex_enter(&swapinfo_lock);

		if (tmp_nswapfiles != nswapfiles) {
			mutex_exit(&swapinfo_lock);
			kmem_free(pname, nlen);
			kmem_free(csip, tmp_nswapfiles * sizeof (*csip));
			gplen = 0;
			goto beginning;
		}
		for (sip = swapinfo, tsip = csip, tpname = pname, nswap = 0;
		    (sip != NULL) && (nswap < tmp_nswapfiles);
		    sip = sip->si_next, tsip++, tpname += plen, nswap++) {
			plen = sip->si_pnamelen;
			if (tpname + plen - pname > nlen) {
				mutex_exit(&swapinfo_lock);
				kmem_free(pname, nlen);
				goto retry;
			}
			*tsip = *sip;
			tsip->si_pname = tpname;
			(void) strcpy(tsip->si_pname, sip->si_pname);
		}
		mutex_exit(&swapinfo_lock);

		if (sip != NULL) {
			error = ENOMEM;
			goto lout;
		}
		ust = (swapent32_t *)((swaptbl32_t *)sc_arg)->swt_ent;
		for (tsip = csip, cnt = 0; cnt < nswap;  tsip++, ust++, cnt++) {
			if (copyin(ust, &st, sizeof (*ust)) != 0) {
				error = EFAULT;
				goto lout;
			}
			st.ste_flags = tsip->si_flags;
			st.ste_length =
			    (tsip->si_eoff - tsip->si_soff) >> SCTRSHFT;
			st.ste_start = tsip->si_soff >> SCTRSHFT;
			st.ste_pages = tsip->si_npgs;
			st.ste_free = tsip->si_nfpgs;
			if (copyout(&st, ust, sizeof (st)) != 0) {
				error = EFAULT;
				goto lout;
			}
			if (!tsip->si_pnamelen)
				continue;
			if (copyout(tsip->si_pname, (caddr_t)st.ste_path,
			    tsip->si_pnamelen) != 0) {
				error = EFAULT;
				goto lout;
			}
		}
		*rv = nswap;
lout:
		kmem_free(csip, tmp_nswapfiles * sizeof (*csip));
		kmem_free(pname, nlen);
		return (error);

	case SC_ADD:
	case SC_REMOVE:
		break;
	default:
		return (EINVAL);
	}
	if (!suser(CRED()))
		return (EPERM);

	if (copyin(sc_arg, &sr, sizeof (sr)))
		return (EFAULT);

	/* Allocate the space to read in pathname */
	if ((swapname = kmem_alloc(MAXPATHLEN, KM_NOSLEEP)) == NULL)
		return (ENOMEM);

	error = copyinstr((caddr_t)sr.sr_name, swapname, MAXPATHLEN, NULL);
	if (error)
		goto out;

	error = lookupname(swapname, UIO_SYSSPACE, FOLLOW, NULLVPP, &vp);
	if (error) {
		if (sc_cmd == SC_ADD)
			goto out;
		/* see if we match by name */
		vp = swapdel_byname(swapname, (u_int)sr.sr_start);
		if (vp == NULL)
			goto out;
	}

	if (vp->v_flag & (VNOMAP | VNOSWAP)) {
		VN_RELE(vp);
		error = ENOSYS;
		goto out;
	}
	switch (vp->v_type) {
	case VBLK:
		break;

	case VREG:
		if (vp->v_vfsp && (vp->v_vfsp->vfs_flag & VFS_RDONLY))
			error = EROFS;
		else
			error = VOP_ACCESS(vp, VREAD|VWRITE, 0, CRED());
		break;

	case VDIR:
		error = EISDIR;
		break;
	default:
		error = ENOSYS;
		break;
	}
	if (error == 0) {
		if (sc_cmd == SC_REMOVE)
			error = swapdel(vp, sr.sr_start);
		else
			error = swapadd(vp, sr.sr_start, sr.sr_length,
			    swapname);
	}
	VN_RELE(vp);
out:
	kmem_free(swapname, MAXPATHLEN);
	return (error);
}

#endif /* _LP64 && _SYSCALL32 */

/*
 * Add a new swap file.
 */
int
swapadd(struct vnode *vp, u_long lowblk, u_long nblks, char *swapname)
{
	struct swapinfo **sipp, *nsip = NULL, *esip = NULL;
	struct vnode *cvp;
	struct vattr vattr;
	pgcnt_t pages;
	u_offset_t soff, eoff;
	int error;
	ssize_t i, start, end;
	u_short wasswap;
	u_long startblk;
	size_t	returned_mem;

	SWAP_PRINT(SW_CTL, "swapadd: vp %p lowblk %ld nblks %ld swapname %s\n",
	    vp, lowblk, nblks, swapname, 0);
	/*
	 * Get the real vnode. (If vp is not a specnode it just returns vp, so
	 * it does the right thing, but having this code know about specnodes
	 * violates the spirit of having it be indepedent of vnode type.)
	 */
	cvp = common_specvp(vp);

	/*
	 * Or in VISSWAP so file system has chance to deny swap-ons during open.
	 */
	mutex_enter(&cvp->v_lock);
	wasswap = cvp->v_flag & VISSWAP;
	cvp->v_flag |= VISSWAP;
	mutex_exit(&cvp->v_lock);

	mutex_enter(&swap_lock);
	if (error = VOP_OPEN(&cvp, FREAD|FWRITE, CRED())) {
		mutex_exit(&swap_lock);
		/* restore state of v_flag */
		if (!wasswap) {
			mutex_enter(&cvp->v_lock);
			cvp->v_flag &= ~VISSWAP;
			mutex_exit(&cvp->v_lock);
		}
		return (error);
	}
	mutex_exit(&swap_lock);

	/*
	 * Get partition size. Return error if empty partition,
	 * or if request does not fit within the partition.
	 * If this is the first swap device, we can reduce
	 * the size of the swap area to match what is
	 * available.  This can happen if the system was built
	 * on a machine with a different size swap partition.
	 */
	vattr.va_mask = AT_SIZE;
	if (error = VOP_GETATTR(cvp, &vattr, ATTR_COMM, CRED()))
		goto out;
	/*
	 * Large Files: No support for large swap files yet.
	 *
	 * XXX  The old semantics of the not large file specfs was to
	 * return UNKNOWN_SIZE for the size of a block device bigger
	 * than MAXOFF_T and UNKNOWN_SIZE just happens to be 2 gigabytes - 1
	 * or 0x7fffffff.  This will silently truncate any swap devices
	 * bigger than 2 gigabytes down to UNKNOWN_SIZE and allow you to use
	 * the first 2 gigabytes for swap even though you might think that
	 * are getting more.  Unfortunately, we have to keep these broken
	 * semantics for compatibility even though specfs can handle large
	 * block devices bigger than 2 gigabytes now. (:-(
	 */

	if (vattr.va_size > MAXOFF_T) {
		cmn_err(CE_NOTE,
			"!swap device %s truncated from %llx to %x",
			swapname, vattr.va_size, UNKNOWN_SIZE);
		vattr.va_size = UNKNOWN_SIZE;
	}
	/* Fail if file not writeable (try to set size to current size) */
	vattr.va_mask = AT_SIZE;
	if (error = VOP_SETATTR(cvp, &vattr, 0, CRED()))
		goto out;

	/* Fail if fs does not support VOP_PAGEIO */
	error = VOP_PAGEIO(cvp, (page_t *)NULL, (u_offset_t)0, 0, 0, CRED());

	if (error == ENOSYS)
		goto out;
	else
		error = 0;
	/*
	 * If swapping on the root filesystem don't put swap blocks that
	 * correspond to the miniroot filesystem on the swap free list.
	 */
	if (cvp == rootdir)
		startblk = roundup(MINIROOTSIZE<<SCTRSHFT, klustsize)>>SCTRSHFT;
	else				/* Skip 1st page (disk label) */
		startblk = (u_long)(lowblk ? lowblk : 1);


	soff = startblk << SCTRSHFT;

	if ((vattr.va_size == 0) || (soff >= vattr.va_size)) {
		error = EINVAL;
		goto out;
	}

	/*
	 * If user specified 0 blks, use the size of the device
	 */
	eoff = nblks ?  soff + (nblks - (startblk - lowblk) << SCTRSHFT) :
			vattr.va_size;

	SWAP_PRINT(SW_CTL, "swapadd: va_size %ld soff %ld eoff %ld\n",
	    vattr.va_size, soff, eoff, 0, 0);

	if (eoff > vattr.va_size) {
		error = EINVAL;
		goto out;
	}

	/*
	 * The starting and ending offsets must be page aligned.
	 * Round soff up to next page boundary, round eoff
	 * down to previous page boundary.
	 */
	soff = ptob(btopr(soff));
	eoff = ptob(btop(eoff));
	if (soff >= eoff) {
		SWAP_PRINT(SW_CTL, "swapadd: soff %ld >= eoff %ld\n",
		    soff, eoff, 0, 0, 0);
		error = EINVAL;
		goto out;
	}

	pages = btop(eoff - soff);

	/* Allocate and partially set up the new swapinfo */
	nsip = kmem_zalloc(sizeof (struct swapinfo), KM_SLEEP);
	nsip->si_vp = cvp;

	mutex_enter(&cvp->v_lock);
	cvp->v_flag |= VISSWAP;
	mutex_exit(&cvp->v_lock);

	nsip->si_soff = soff;
	nsip->si_eoff = eoff;
	nsip->si_hint = 0;
	nsip->si_checkcnt = nsip->si_alloccnt = 0;

	nsip->si_pnamelen = (int)strlen(swapname) + 1;
	nsip->si_pname = (char *)kmem_zalloc(nsip->si_pnamelen, KM_SLEEP);
	bcopy(swapname, nsip->si_pname, nsip->si_pnamelen - 1);
	SWAP_PRINT(SW_CTL, "swapadd: allocating swapinfo for %s, %ld pages\n",
	    swapname, pages, 0, 0, 0);
	/*
	 * Size of swapslots map in bytes
	 */
	nsip->si_mapsize = roundup(pages, NBBW) / NBBY;
	nsip->si_swapslots = kmem_zalloc(nsip->si_mapsize, KM_SLEEP);

	/*
	 * Permanently set the bits that can't ever be allocated,
	 * i.e. those from the ending offset to the round up slot for the
	 * swapslots bit map.
	 */
	start = pages;
	end = roundup(pages, NBBW);
	for (i = start; i < end; i++) {
		SWAP_PRINT(SW_CTL, "swapadd: set bit for page %ld\n", i,
		    0, 0, 0, 0);
		SETBIT(nsip->si_swapslots, i);
	}
	nsip->si_npgs = nsip->si_nfpgs = pages;
	/*
	 * Now check to see if we can add it. We wait til now to check because
	 * we need the swapinfo_lock and we don't want sleep with it (e.g.,
	 * during kmem_alloc()) while we're setting up the swapinfo.
	 */
	mutex_enter(&swapinfo_lock);
	for (sipp = &swapinfo; (esip = *sipp) != NULL; sipp = &esip->si_next) {
		if (esip->si_vp == cvp) {
			if (esip->si_soff == soff && esip->si_npgs == pages &&
			    (esip->si_flags & ST_DOINGDEL)) {
				/*
				 * We are adding a device that we are in the
				 * middle of deleting. Just clear the
				 * ST_DOINGDEL flag to signal this and
				 * the deletion routine will eventually notice
				 * it and add it back.
				 */
				esip->si_flags &= ~ST_DOINGDEL;
				mutex_exit(&swapinfo_lock);
				goto out;
			}
			/* disallow overlapping swap files */
			if ((soff < esip->si_eoff) && (eoff > esip->si_soff)) {
				error = EEXIST;
				mutex_exit(&swapinfo_lock);
				goto out;
			}
		}
	}

	nswapfiles++;

	/*
	 * add new swap device to list and shift allocations to it
	 * before updating the anoninfo counters
	 */
	*sipp = nsip;
	silast = nsip;

	/*
	 * Update the total amount of reservable swap space
	 * accounting properly for swap space from physical memory
	 */
	/* New swap device soaks up currently reserved memory swap */
	mutex_enter(&anoninfo_lock);

	ASSERT(k_anoninfo.ani_mem_resv >= k_anoninfo.ani_locked_swap);
	ASSERT(k_anoninfo.ani_max >= k_anoninfo.ani_phys_resv);

	k_anoninfo.ani_max += pages;
	ANI_ADD(pages);
	if (k_anoninfo.ani_mem_resv > k_anoninfo.ani_locked_swap) {
		returned_mem = MIN(k_anoninfo.ani_mem_resv -
		    k_anoninfo.ani_locked_swap,
		    k_anoninfo.ani_max - k_anoninfo.ani_phys_resv);

		ANI_ADD(-returned_mem);
		k_anoninfo.ani_free -= returned_mem;
		k_anoninfo.ani_mem_resv -= returned_mem;
		k_anoninfo.ani_phys_resv += returned_mem;

		mutex_enter(&freemem_lock);
		availrmem += returned_mem;
		mutex_exit(&freemem_lock);
	}
	/*
	 * At boot time, to permit booting small memory machines using
	 * only physical memory as swap space, we allowed a dangerously
	 * large amount of memory to be used as swap space; now that
	 * more physical backing store is available bump down the amount
	 * we can get from memory to a safer size.
	 */
	if (swapfs_minfree < swapfs_desfree) {
		mutex_enter(&freemem_lock);
		if (availrmem > swapfs_desfree || !k_anoninfo.ani_mem_resv)
			swapfs_minfree = swapfs_desfree;
		mutex_exit(&freemem_lock);
	}

	SWAP_PRINT(SW_CTL, "swapadd: ani_max %ld ani_free %ld\n",
	    k_anoninfo.ani_free, k_anoninfo.ani_free, 0, 0, 0);

	mutex_exit(&anoninfo_lock);

	mutex_exit(&swapinfo_lock);

	/* Initialize the dump device */
	mutex_enter(&dump_lock);
	if (dumpvp == NULL)
		(void) dumpinit(vp, swapname, 0);
	mutex_exit(&dump_lock);

	VN_HOLD(cvp);
out:
	if (error || esip) {
		SWAP_PRINT(SW_CTL, "swapadd: error (%d)\n", error, 0, 0, 0, 0);

		if (!wasswap) {
			mutex_enter(&cvp->v_lock);
			cvp->v_flag &= ~VISSWAP;
			mutex_exit(&cvp->v_lock);
		}
		if (nsip) {
			kmem_free(nsip->si_swapslots, (size_t)nsip->si_mapsize);
			kmem_free(nsip->si_pname, nsip->si_pnamelen);
			kmem_free(nsip, sizeof (*nsip));
		}
		mutex_enter(&swap_lock);
		(void) VOP_CLOSE(cvp, FREAD|FWRITE, 1, (offset_t)0, CRED());
		mutex_exit(&swap_lock);
	}
	return (error);
}

/*
 * Delete a swap file.
 */
static int
swapdel(
	struct vnode *vp,
	u_long lowblk) /* Low block number of area to delete. */
{
	struct swapinfo **sipp, *osip = NULL;
	struct vnode *cvp;
	u_offset_t soff;
	int error = 0;
	u_offset_t toff = 0;
	struct vnode *tvp = NULL;
	spgcnt_t pages;
	struct anon **app, *ap;
	kmutex_t *ahm;
	pgcnt_t adjust_swap = 0;

	/* Find the swap file entry for the file to be deleted */
	cvp = common_specvp(vp);


	lowblk = lowblk ? lowblk : 1; 	/* Skip first page (disk label) */
	soff = ptob(btopr(lowblk << SCTRSHFT)); /* must be page aligned */

	mutex_enter(&swapinfo_lock);
	for (sipp = &swapinfo; (osip = *sipp) != NULL; sipp = &osip->si_next) {
		if ((osip->si_vp == cvp) &&
		    (osip->si_soff == soff) && (osip->si_flags == 0))
			break;
	}

	/* If the file was not found, error.  */
	if (osip == NULL) {
		error = EINVAL;
		mutex_exit(&swapinfo_lock);
		goto out;
	}

	pages = osip->si_npgs;

	/*
	 * Do not delete if we will be low on swap pages.
	 */
	mutex_enter(&anoninfo_lock);

	ASSERT(k_anoninfo.ani_mem_resv >= k_anoninfo.ani_locked_swap);
	ASSERT(k_anoninfo.ani_max >= k_anoninfo.ani_phys_resv);

	mutex_enter(&freemem_lock);
	if (((k_anoninfo.ani_max - k_anoninfo.ani_phys_resv) +
	    MAX((spgcnt_t)(availrmem - swapfs_minfree), 0)) < pages) {
		mutex_exit(&freemem_lock);
		mutex_exit(&anoninfo_lock);
		error = ENOMEM;
		cmn_err(CE_WARN, "swapdel - too few free pages");
		mutex_exit(&swapinfo_lock);
		goto out;
	}
	mutex_exit(&freemem_lock);

	k_anoninfo.ani_max -= pages;

	/* If needed, reserve memory swap to replace old device */
	if (k_anoninfo.ani_phys_resv > k_anoninfo.ani_max) {
		adjust_swap = k_anoninfo.ani_phys_resv - k_anoninfo.ani_max;
		k_anoninfo.ani_phys_resv -= adjust_swap;
		k_anoninfo.ani_mem_resv += adjust_swap;
		mutex_enter(&freemem_lock);
		availrmem -= adjust_swap;
		mutex_exit(&freemem_lock);
		ANI_ADD(adjust_swap);
	}
	ASSERT(k_anoninfo.ani_mem_resv >= k_anoninfo.ani_locked_swap);
	ASSERT(k_anoninfo.ani_max >= k_anoninfo.ani_phys_resv);
	mutex_exit(&anoninfo_lock);

	ANI_ADD(-pages);

	/*
	 * Set the delete flag.  This prevents anyone from allocating more
	 * pages from this file. Also set ST_DOINGDEL. Someone who wants to
	 * add the file back while we're deleting it will signify by clearing
	 * this flag.
	 */
	osip->si_flags |= ST_INDEL|ST_DOINGDEL;
	mutex_exit(&swapinfo_lock);

	/*
	 * Free all the allocated physical slots for this file. We do this
	 * by walking through the entire anon hash array, because we need
	 * to update all the anon slots that have physical swap slots on
	 * this file, and this is the only way to find them all. We go back
	 * to the beginning of a bucket after each slot is freed because the
	 * anonhash_lock is not held during the free and thus the hash table
	 * may change under us.
	 */
	for (app = anon_hash; app < &anon_hash[ANON_HASH_SIZE]; app++) {
		ahm = &anonhash_lock[(app-anon_hash) & (AH_LOCK_SIZE - 1)];
		mutex_enter(ahm);
top:
		for (ap = *app; ap != NULL; ap = ap->an_hash) {
			if (ap->an_pvp == cvp &&
			    ap->an_poff >= osip->si_soff &&
			    ap->an_poff < osip->si_eoff) {
				ASSERT(TESTBIT(osip->si_swapslots,
				    btop((size_t)(ap->an_poff -
				    osip->si_soff))));
				tvp = ap->an_vp;
				toff = ap->an_off;
				VN_HOLD(tvp);
				mutex_exit(ahm);

				error = swapslot_free(tvp, toff, osip);

				VN_RELE(tvp);
				mutex_enter(ahm);
				if (!error && (osip->si_flags & ST_DOINGDEL)) {
					goto top;
				} else {
					if (error) {
						cmn_err(CE_WARN,
						    "swapslot_free failed %d\n",
						    error);
					}

					/*
					 * Add device back before making it
					 * visible.
					 */
					mutex_enter(&swapinfo_lock);
					osip->si_flags &=
					    ~(ST_INDEL | ST_DOINGDEL);
					mutex_exit(&swapinfo_lock);

					/*
					 * Update the anon space available
					 */
					mutex_enter(&anoninfo_lock);

					k_anoninfo.ani_phys_resv += adjust_swap;
					k_anoninfo.ani_mem_resv -= adjust_swap;
					k_anoninfo.ani_max += pages;

					mutex_enter(&freemem_lock);
					availrmem += adjust_swap;
					mutex_exit(&freemem_lock);

					mutex_exit(&anoninfo_lock);

					ANI_ADD(pages);

					mutex_exit(ahm);
					goto out;
				}
			}
		}
		mutex_exit(ahm);
	}

	/* All done, they'd better all be free! */
	mutex_enter(&swapinfo_lock);
	ASSERT(osip->si_nfpgs == osip->si_npgs);

	/* Now remove it from the swapinfo list */
	for (sipp = &swapinfo; *sipp != NULL; sipp = &(*sipp)->si_next) {
		if (*sipp == osip)
			break;
	}
	ASSERT(*sipp);
	*sipp = osip->si_next;
	if (silast == osip)
		if ((silast = osip->si_next) == NULL)
			silast = swapinfo;
	nswapfiles--;
	mutex_exit(&swapinfo_lock);

	kmem_free(osip->si_swapslots, osip->si_mapsize);
	kmem_free(osip->si_pname, osip->si_pnamelen);
	kmem_free(osip, sizeof (*osip));

	mutex_enter(&dump_lock);
	if (cvp == dumpvp)
		dumpfini();
	mutex_exit(&dump_lock);

	/* Release the vnode */

	mutex_enter(&swap_lock);
	(void) VOP_CLOSE(cvp, FREAD|FWRITE, 1, (offset_t)0, CRED());
	mutex_enter(&cvp->v_lock);
	cvp->v_flag &= ~VISSWAP;
	mutex_exit(&cvp->v_lock);
	VN_RELE(cvp);
	mutex_exit(&swap_lock);
out:
	return (error);
}

/*
 * Free up a physical swap slot on swapinfo sip, currently in use by the
 * anonymous page whose name is (vp, off).
 */
static int
swapslot_free(
	struct vnode *vp,
	u_offset_t off,
	struct swapinfo *sip)
{
	struct page *pl[2], *pp;
	struct anon *ap = NULL;
	int error = 0;
	kmutex_t *ahm;

	/*
	 * Get the page for the old swap slot and i/o lock it.
	 * Users of the physical slot will synchronize on the i/o lock.
	 */
	if (error = VOP_GETPAGE(vp, (offset_t)off, ptob(1), NULL,
	    pl, ptob(1), segkmap, NULL, S_READ, CRED())) {
		/*
		 * Anon slot went away (EIDRM) or vp was truncated (EFAULT)
		 * while we got the page. Thus the physical slot must be
		 * free, so we have succeeded.
		 */
		if (error == EIDRM || error == EFAULT)
			error = 0;
		return (error);
	}
	pp = pl[0];
	page_io_lock(pp);

	ahm = &anonhash_lock[AH_LOCK(vp, off)];
	mutex_enter(ahm);
	/*
	 * Get the anon slot. It may have gone away while we were getting the
	 * page in which case its associated physical swap slot must now be
	 * free and we're done.
	 */
	if ((ap = swap_anon(vp, off)) == NULL) {
		mutex_exit(ahm);
		goto out;
	}
	/*
	 * Free the physical slot. It may have been freed up and replaced with
	 * another one while we were getting the page so we have to re-verify
	 * that this is really one we want. If we do free the slot we have
	 * to mark the page modified, as its backing store is now gone.
	 */
	if (ap->an_pvp == sip->si_vp && ap->an_poff >= sip->si_soff &&
	    ap->an_poff < sip->si_eoff) {
		swap_phys_free(ap->an_pvp, ap->an_poff, PAGESIZE);
		ap->an_pvp = NULL;
		ap->an_poff = NULL;
		mutex_exit(ahm);
		hat_setmod(pp);
	} else {
		mutex_exit(ahm);
	}
out:
	/* Release the page locks */
	page_unlock(pp);
	page_io_unlock(pp);
	return (error);
}

/*
 * Get contig physical backing store for vp, in the range
 * [*offp, *offp + *lenp), May back a subrange of this, but must
 * always include the requested offset or fail. Returns the offsets
 * backed as [*offp, *offp + *lenp) and the physical offsets used to
 * back them from *pvpp in the range [*pstartp, *pstartp + *lenp).
 * Returns 	0 for success
 * 		SE_NOANON -- no anon slot for requested paged
 *		SE_NOSWAP -- no physical swap space available
 */
int
swap_newphysname(
	struct vnode *vp,
	u_offset_t offset,
	u_offset_t *offp,
	size_t *lenp,
	struct vnode **pvpp,
	u_offset_t *poffp)
{
	struct anon *ap = NULL;		/* anon slot for vp, off */
	int error = 0;
	struct vnode *pvp;
	u_offset_t poff, pstart, prem;
	size_t plen;
	u_offset_t off, start;
	kmutex_t *ahm;

	ASSERT(*offp <= offset && offset < *offp + *lenp);

	/* Get new physical swap slots. */
	plen = *lenp;
	if (!swap_phys_alloc(&pvp, &pstart, &plen, 0)) {
		/*
		 * No swap available so return error unless requested
		 * offset is already backed in which case return that.
		 */
		ahm = &anonhash_lock[AH_LOCK(vp, offset)];
		mutex_enter(ahm);
		if ((ap = swap_anon(vp, offset)) == NULL) {
			error = SE_NOANON;
			mutex_exit(ahm);
			return (error);
		}
		error = (ap->an_pvp ? 0 : SE_NOSWAP);
		*offp = offset;
		*lenp = PAGESIZE;
		*pvpp = ap->an_pvp;
		*poffp = ap->an_poff;
		mutex_exit(ahm);
		return (error);
	}

	/*
	 * We got plen (<= *lenp) contig slots. Use these to back a
	 * subrange of [*offp, *offp + *lenp) which includes offset.
	 * For now we just put offset at the end of the kluster.
	 * Clearly there are other possible choices - which is best?
	 */
	start = MAX(*offp,
	    (offset + PAGESIZE > plen) ? (offset + PAGESIZE - plen) : 0);
	ASSERT(start + plen <= *offp + *lenp);

	for (off = start, poff = pstart; poff < pstart + plen;
	    off += PAGESIZE, poff += PAGESIZE) {
		ahm = &anonhash_lock[AH_LOCK(vp, off)];
		mutex_enter(ahm);
		if ((ap = swap_anon(vp, off)) != NULL) {
			/* Free old slot if any, and assign new one */
			if (ap->an_pvp)
				swap_phys_free(ap->an_pvp, ap->an_poff,
				    PAGESIZE);
			ap->an_pvp = pvp;
			ap->an_poff = poff;
		} else {	/* No anon slot for a klustered page, quit. */
			prem = (pstart + plen) - poff;
			/* Already did requested page, do partial kluster */
			if (off > offset) {
				plen = poff - pstart;
				error = 0;
			/* Fail on requested page, error */
			} else if (off == offset)  {
				error = SE_NOANON;
			/* Fail on prior page, fail on requested page, error */
			} else if ((ap = swap_anon(vp, offset)) == NULL) {
				error = SE_NOANON;
			/* Fail on prior page, got requested page, do only it */
			} else {
				/* Free old slot if any, and assign new one */
				if (ap->an_pvp)
					swap_phys_free(ap->an_pvp, ap->an_poff,
					    PAGESIZE);
				ap->an_pvp = pvp;
				ap->an_poff = poff;
				/* One page kluster */
				start = offset;
				plen = PAGESIZE;
				pstart = poff;
				poff += PAGESIZE;
				prem -= PAGESIZE;
			}
			/* Free unassigned slots */
			swap_phys_free(pvp, poff, prem);
			mutex_exit(ahm);
			break;
		}
		mutex_exit(ahm);
	}
	ASSERT(*offp <= start && start + plen <= *offp + *lenp);
	ASSERT(start <= offset && offset < start + plen);
	*offp = start;
	*lenp = plen;
	*pvpp = pvp;
	*poffp = pstart;
	return (error);
}


/*
 * Get the physical swap backing store location for a given anonymous page
 * named (vp, off). The backing store name is returned in (*pvpp, *poffp).
 * Returns	0 		success
 *		EIDRM --	no anon slot (page is not allocated)
 */
int
swap_getphysname(
	struct vnode *vp,
	u_offset_t off,
	struct vnode **pvpp,
	u_offset_t *poffp)
{
	struct anon *ap;
	int error = 0;
	kmutex_t *ahm;

	ahm = &anonhash_lock[AH_LOCK(vp, off)];
	mutex_enter(ahm);

	/* Get anon slot for vp, off */
	ap = swap_anon(vp, off);
	if (ap == NULL) {
		error = EIDRM;
		goto out;
	}
	*pvpp = ap->an_pvp;
	*poffp = ap->an_poff;
out:
	mutex_exit(ahm);
	return (error);
}
