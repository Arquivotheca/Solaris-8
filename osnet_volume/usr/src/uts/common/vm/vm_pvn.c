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
 * 	(c) 1986, 1987, 1988, 1989, 1990, 1993, 1996  Sun Microsystems, Inc
 * 	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 *
 */

/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)vm_pvn.c	1.158	99/07/16 SMI"

/*
 * VM - paged vnode.
 *
 * This file supplies vm support for the vnode operations that deal with pages.
 */
#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <sys/vmmeter.h>
#include <sys/vmsystm.h>
#include <sys/mman.h>
#include <sys/vfs.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/cpuvar.h>
#include <sys/vtrace.h>
#include <sys/tnf_probe.h>

#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/rm.h>
#include <vm/pvn.h>
#include <vm/page.h>
#include <vm/seg_map.h>
#include <vm/seg_kmem.h>
#include <sys/fs/swapnode.h>

int pvn_nofodklust = 0;
int pvn_write_noklust = 0;

/*
 * Find the largest contiguous block which contains `addr' for file offset
 * `offset' in it while living within the file system block sizes (`vp_off'
 * and `vp_len') and the address space limits for which no pages currently
 * exist and which map to consecutive file offsets.
 */
page_t *
pvn_read_kluster(
	struct vnode *vp,
	u_offset_t off,
	struct seg *seg,
	caddr_t addr,
	u_offset_t *offp,			/* return values */
	size_t *lenp,				/* return values */
	u_offset_t vp_off,
	size_t vp_len,
	int isra)
{
	ssize_t deltaf, deltab;
	page_t *pp;
	page_t *plist = NULL;
	spgcnt_t pagesavail;
	u_offset_t vp_end;

	ASSERT(off >= vp_off && off < vp_off + vp_len);

	/*
	 * We only want to do klustering/read ahead if there
	 * is more than minfree pages currently available.
	 */
	pagesavail = freemem - minfree;

	if (pagesavail <= 0)
		if (isra)
			return ((page_t *)NULL);    /* ra case - give up */
		else
			pagesavail = 1;		    /* must return a page */

	/* We calculate in pages instead of bytes due to 32-bit overflows */
	if (pagesavail < (spgcnt_t)btopr(vp_len)) {
		/*
		 * Don't have enough free memory for the
		 * max request, try sizing down vp request.
		 */
		deltab = (ssize_t)(off - vp_off);
		vp_len -= deltab;
		vp_off += deltab;
		if (pagesavail < btopr(vp_len)) {
			/*
			 * Still not enough memory, just settle for
			 * pagesavail which is at least 1.
			 */
			vp_len = ptob(pagesavail);
		}
	}

	vp_end = vp_off + vp_len;
	ASSERT(off >= vp_off && off < vp_end);

	if (isra && SEGOP_KLUSTER(seg, addr, 0))
		return ((page_t *)NULL);	/* segment driver says no */

	if ((plist = page_create_va(vp, off,
	    PAGESIZE, PG_EXCL | PG_WAIT, seg, addr)) == NULL)
		return ((page_t *)NULL);

	if (vp_len <= PAGESIZE || pvn_nofodklust) {
		*offp = off;
		*lenp = MIN(vp_len, PAGESIZE);
	} else {
		/*
		 * Scan back from front by incrementing "deltab" and
		 * comparing "off" with "vp_off + deltab" to avoid
		 * "signed" versus "unsigned" conversion problems.
		 */
		for (deltab = PAGESIZE; off >= vp_off + deltab;
		    deltab += PAGESIZE) {
			/*
			 * Call back to the segment driver to verify that
			 * the klustering/read ahead operation makes sense.
			 */
			if (SEGOP_KLUSTER(seg, addr, -deltab))
				break;		/* page not eligible */
			if ((pp = page_create_va(vp, off - deltab,
			    PAGESIZE, PG_EXCL, seg, addr - deltab))
			    == NULL)
				break;		/* already have the page */
			/*
			 * Add page to front of page list.
			 */
			page_add(&plist, pp);
		}
		deltab -= PAGESIZE;

		/* scan forward from front */
		for (deltaf = PAGESIZE; off + deltaf < vp_end;
		    deltaf += PAGESIZE) {
			/*
			 * Call back to the segment driver to verify that
			 * the klustering/read ahead operation makes sense.
			 */
			if (SEGOP_KLUSTER(seg, addr, deltaf))
				break;		/* page not file extension */
			if ((pp = page_create_va(vp, off + deltaf,
			    PAGESIZE, PG_EXCL, seg, addr + deltaf))
			    == NULL)
				break;		/* already have page */

			/*
			 * Add page to end of page list.
			 */
			page_add(&plist, pp);
			plist = plist->p_next;
		}
		*offp = off = off - deltab;
		*lenp = deltab + deltaf;
		ASSERT(off >= vp_off);

		/*
		 * If we ended up getting more than was actually
		 * requested, retract the returned length to only
		 * reflect what was requested.  This might happen
		 * if we were allowed to kluster pages across a
		 * span of (say) 5 frags, and frag size is less
		 * than PAGESIZE.  We need a whole number of
		 * pages to contain those frags, but the returned
		 * size should only allow the returned range to
		 * extend as far as the end of the frags.
		 */
		if ((vp_off + vp_len) < (off + *lenp)) {
			ASSERT(vp_end > off);
			*lenp = vp_end - off;
		}
	}
	TRACE_3(TR_FAC_VM, TR_PVN_READ_KLUSTER,
		"pvn_read_kluster:seg %p addr %x isra %x",
		seg, addr, isra);
	return (plist);
}

/*
 * Handle pages for this vnode on either side of the page "pp"
 * which has been locked by the caller.  This routine will also
 * do klustering in the range [vp_off, vp_off + vp_len] up
 * until a page which is not found.  The offset and length
 * of pages included is returned in "*offp" and "*lenp".
 *
 * Returns a list of dirty locked pages all ready to be
 * written back.
 */
page_t *
pvn_write_kluster(
	struct vnode *vp,
	page_t *pp,
	u_offset_t *offp,		/* return values */
	size_t *lenp,			/* return values */
	u_offset_t vp_off,
	size_t vp_len,
	int flags)
{
	u_offset_t off;
	page_t *dirty;
	size_t deltab, deltaf;
	se_t se;
	u_offset_t vp_end;

	off = pp->p_offset;

	/*
	 * Kustering should not be done if we are invalidating
	 * pages since we could destroy pages that belong to
	 * some other process if this is a swap vnode.
	 */
	if (pvn_write_noklust || (flags & B_INVAL)) {
		*offp = off;
		*lenp = PAGESIZE;
		return (pp);
	}

	if (flags & B_FREE)
		se = SE_EXCL;
	else
		se = SE_SHARED;

	dirty = pp;
	/*
	 * Scan backwards looking for pages to kluster by incrementing
	 * "deltab" and comparing "off" with "vp_off + deltab" to
	 * avoid "signed" versus "unsigned" conversion problems.
	 */
	for (deltab = PAGESIZE; off >= vp_off + deltab; deltab += PAGESIZE) {
		pp = page_lookup_nowait(vp, off - deltab, se);
		if (pp == NULL)
			break;		/* page not found */
		if (pvn_getdirty(pp, flags | B_DELWRI) == 0)
			break;
		page_add(&dirty, pp);
	}
	deltab -= PAGESIZE;

	vp_end = vp_off + vp_len;
	/* now scan forwards looking for pages to kluster */
	for (deltaf = PAGESIZE; off + deltaf < vp_end; deltaf += PAGESIZE) {
		pp = page_lookup_nowait(vp, off + deltaf, se);
		if (pp == NULL)
			break;		/* page not found */
		if (pvn_getdirty(pp, flags | B_DELWRI) == 0)
			break;
		page_add(&dirty, pp);
		dirty = dirty->p_next;
	}

	*offp = off - deltab;
	*lenp = deltab + deltaf;
	return (dirty);
}

/*
 * Generic entry point used to release the "shared/exclusive" lock
 * and the "p_iolock" on pages after i/o is complete.
 */
void
pvn_io_done(page_t *plist)
{
	page_t *pp;

	while (plist != NULL) {
		pp = plist;
		page_sub(&plist, pp);
		page_io_unlock(pp);
		page_unlock(pp);
	}
}

/*
 * Entry point to be used by file system getpage subr's and
 * other such routines which either want to unlock pages (B_ASYNC
 * request) or destroy a list of pages if an error occurred.
 */
void
pvn_read_done(page_t *plist, int flags)
{
	page_t *pp;

	while (plist != NULL) {
		pp = plist;
		page_sub(&plist, pp);
		page_io_unlock(pp);
		if (flags & B_ERROR) {
			/*LINTED: constant in conditional context*/
			VN_DISPOSE(pp, B_INVAL, 0, kcred);
		} else {
			(void) page_release(pp, 0);
		}
	}
}

/*
 * Automagic pageout.
 * When memory gets tight, start freeing pages popping out of the
 * write queue.
 */
int	write_free = 1;
pgcnt_t	pages_before_pager = 200;	/* LMXXX */

/*
 * Routine to be called when page-out's complete.
 * The caller, typically VOP_PUTPAGE, has to explicity call this routine
 * after waiting for i/o to complete (biowait) to free the list of
 * pages associated with the buffer.  These pages must be locked
 * before i/o is initiated.
 *
 * If a write error occurs, the pages are marked as modified
 * so the write will be re-tried later.
 */

void
pvn_write_done(page_t *plist, int flags)
{
	int dfree = 0;
	int pgrec = 0;
	int pgout = 0;
	int pgpgout = 0;
	int anonpgout = 0;
	int anonfree = 0;
	int fspgout = 0;
	int fsfree = 0;
	int execpgout = 0;
	int execfree = 0;
	page_t *pp;
	struct cpu *cpup;
	struct vnode *vp = NULL;	/* for probe */
	u_int ppattr;

	ASSERT((flags & B_READ) == 0);

	/*
	 * If we are about to start paging anyway, start freeing pages.
	 */
	if (write_free && freemem < lotsfree + pages_before_pager &&
	    (flags & B_ERROR) == 0) {
		flags |= B_FREE;
	}

	/*
	 * Handle each page involved in the i/o operation.
	 */
	while (plist != NULL) {
		pp = plist;
		ASSERT(PAGE_LOCKED(pp) && page_iolock_assert(pp));
		page_sub(&plist, pp);

		/* Kernel probe support */
		if (vp == NULL)
			vp = pp->p_vnode;

		if (flags & B_ERROR) {
			/*
			 * Write operation failed.  We don't want
			 * to destroy (or free) the page unless B_FORCE
			 * is set. We set the mod bit again and release
			 * all locks on the page so that it will get written
			 * back again later when things are hopefully
			 * better again.
			 * If B_INVAL and B_FORCE is set we really have
			 * to destroy the page.
			 */
			if ((flags & (B_INVAL|B_FORCE)) == (B_INVAL|B_FORCE)) {
				page_io_unlock(pp);
				/*LINTED: constant in conditional context*/
				VN_DISPOSE(pp, B_INVAL, 0, kcred);
			} else {
				hat_setmod(pp);
				page_io_unlock(pp);
				page_unlock(pp);
			}
		} else if (flags & B_INVAL) {
			/*
			 * XXX - Failed writes with B_INVAL set are
			 * not handled appropriately.
			 */
			page_io_unlock(pp);
			/*LINTED: constant in conditional context*/
			VN_DISPOSE(pp, B_INVAL, 0, kcred);
		} else if (flags & B_FREE ||!hat_page_is_mapped(pp)) {
			/*
			 * Update statistics for pages being paged out
			 */
			if (pp->p_vnode) {
				if (IS_SWAPVP(pp->p_vnode)) {
					anonpgout++;
				} else {
					if (pp->p_vnode->v_flag & VVMEXEC) {
						execpgout++;
					} else {
						fspgout++;
					}
				}
			}
			page_io_unlock(pp);
			pgout = 1;
			pgpgout++;
			TRACE_3(TR_FAC_VM, TR_PAGE_WS_OUT,
				"page_ws_out:pp %p vp %p offset %llx",
				pp, pp->p_vnode, pp->p_offset);

			/*
			 * The page_struct_lock need not be acquired to
			 * examine "p_lckcnt" and "p_cowcnt" since we'll
			 * have an "exclusive" lock if the upgrade succeeds.
			 */
			if (page_tryupgrade(pp) &&
			    pp->p_lckcnt == 0 && pp->p_cowcnt == 0) {
				/*
				 * Check if someone has reclaimed the
				 * page.  If ref and mod are not set, no
				 * one is using it so we can free it.
				 * The rest of the system is careful
				 * to use the NOSYNC flag to unload
				 * translations set up for i/o w/o
				 * affecting ref and mod bits.
				 *
				 * Obtain a copy of the real hardware
				 * mod bit using hat_pagesync(pp, HAT_DONTZERO)
				 * to avoid having to flush the cache.
				 */
				ppattr = hat_pagesync(pp, HAT_SYNC_DONTZERO |
					HAT_SYNC_STOPON_MOD);
			ck_refmod:
				if (!(ppattr & (P_REF | P_MOD))) {
					if (hat_page_is_mapped(pp)) {
						/*
						 * Doesn't look like the page
						 * was modified so now we
						 * really have to unload the
						 * translations.  Meanwhile
						 * another CPU could've
						 * modified it so we have to
						 * check again.  We don't loop
						 * forever here because now
						 * the translations are gone
						 * and no one can get a new one
						 * since we have the "exclusive"
						 * lock on the page.
						 */
						(void) hat_pageunload(pp,
							HAT_FORCE_PGUNLOAD);
						ppattr = hat_page_getattr(pp,
							P_REF | P_MOD);
						goto ck_refmod;
					}
					/*
					 * Update statistics for pages being
					 * freed
					 */
					if (pp->p_vnode) {
						if (IS_SWAPVP(pp->p_vnode)) {
							anonfree++;
						} else {
							if (pp->p_vnode->v_flag
							    & VVMEXEC) {
								execfree++;
							} else {
								fsfree++;
							}
						}
					}
					/*LINTED: constant in conditional ctx*/
					VN_DISPOSE(pp, B_FREE,
						(flags & B_DONTNEED), kcred);
					dfree++;
				} else {
					page_unlock(pp);
					pgrec++;
					TRACE_3(TR_FAC_VM, TR_PAGE_WS_FREE,
					"page_ws_free:pp %p vp %p offset %llx",
					pp, pp->p_vnode, pp->p_offset);
				}
			} else {
				/*
				 * Page is either `locked' in memory
				 * or was reclaimed and now has a
				 * "shared" lock, so release it.
				 */
				page_unlock(pp);
			}
		} else {
			/*
			 * Neither B_FREE nor B_INVAL nor B_ERROR.
			 * Just release locks.
			 */
			page_io_unlock(pp);
			page_unlock(pp);
		}
	}

	CPU_STAT_ENTER_K();
	cpup = CPU;		/* get cpup now that CPU cannot change */
	CPU_STAT_ADDQ(cpup, cpu_vminfo.dfree, dfree);
	CPU_STAT_ADDQ(cpup, cpu_vminfo.pgrec, pgrec);
	CPU_STAT_ADDQ(cpup, cpu_vminfo.pgout, pgout);
	CPU_STAT_ADDQ(cpup, cpu_vminfo.pgpgout, pgpgout);
	CPU_STAT_ADDQ(cpup, cpu_vminfo.anonpgout, anonpgout);
	CPU_STAT_ADDQ(cpup, cpu_vminfo.anonfree, anonfree);
	CPU_STAT_ADDQ(cpup, cpu_vminfo.fspgout, fspgout);
	CPU_STAT_ADDQ(cpup, cpu_vminfo.fsfree, fsfree);
	CPU_STAT_ADDQ(cpup, cpu_vminfo.execpgout, execpgout);
	CPU_STAT_ADDQ(cpup, cpu_vminfo.execfree, execfree);
	CPU_STAT_EXIT_K();

	/* Kernel probe */
	TNF_PROBE_4(pageout, "vm pageio io", /* CSTYLED */,
		tnf_opaque,	vnode,			vp,
		tnf_ulong,	pages_pageout,		pgpgout,
		tnf_ulong,	pages_freed,		dfree,
		tnf_ulong,	pages_reclaimed,	pgrec);
}

/*
 * Flags are composed of {B_ASYNC, B_INVAL, B_FREE, B_DONTNEED, B_DELWRI,
 * B_TRUNC, B_FORCE}.  B_DELWRI indicates that this page is part of a kluster
 * operation and is only to be considered if it doesn't involve any
 * waiting here.  B_TRUNC indicates that the file is being truncated
 * and so no i/o needs to be done. B_FORCE indicates that the page
 * must be destroyed so don't try wrting it out.
 *
 * The caller must ensure that the page is locked.  Returns 1, if
 * the page should be written back (the "iolock" is held in this
 * case), or 0 if the page has been dealt with or has been
 * unlocked.
 */
int
pvn_getdirty(page_t *pp, int flags)
{
	ASSERT((flags & (B_INVAL | B_FREE)) ?
	    PAGE_EXCL(pp) : PAGE_SHARED(pp));
	ASSERT(PP_ISFREE(pp) == 0);

	/*
	 * If trying to invalidate or free a logically `locked' page,
	 * forget it.  Don't need page_struct_lock to check p_lckcnt and
	 * p_cowcnt as the page is exclusively locked.
	 */
	if ((flags & (B_INVAL | B_FREE)) && !(flags & (B_TRUNC|B_FORCE)) &&
	    (pp->p_lckcnt != 0 || pp->p_cowcnt != 0)) {
		page_unlock(pp);
		return (0);
	}

	/*
	 * Now acquire the i/o lock so we can add it to the dirty
	 * list (if necessary).  We avoid blocking on the i/o lock
	 * in the following cases:
	 *
	 *	If B_DELWRI is set, which implies that this request is
	 *	due to a klustering operartion.
	 *
	 *	If this is an async (B_ASYNC) operation and we are not doing
	 *	invalidation (B_INVAL) [The current i/o or fsflush will ensure
	 *	that the the page is written out].
	 */
	if ((flags & B_DELWRI) || ((flags & (B_INVAL|B_ASYNC)) == B_ASYNC)) {
		if (!page_io_trylock(pp)) {
			page_unlock(pp);
			return (0);
		}
	} else {
		page_io_lock(pp);
	}

	/*
	 * If we want to free or invalidate the page then
	 * we need to unload it so that anyone who wants
	 * it will have to take a minor fault to get it.
	 * Otherwise, we're just writing the page back so we
	 * need to sync up the hardwre and software mod bit to
	 * detect any future modifications.  We clear the
	 * software mod bit when we put the page on the dirty
	 * list.
	 */
	if (flags & (B_INVAL | B_FREE)) {
		(void) hat_pageunload(pp, HAT_FORCE_PGUNLOAD);
	} else {
		(void) hat_pagesync(pp, HAT_SYNC_ZERORM);
	}

	if (!hat_ismod(pp) || (flags & B_TRUNC)) {
		/*
		 * Don't need to add it to the
		 * list after all.
		 */
		page_io_unlock(pp);
		if (flags & B_INVAL) {
			/*LINTED: constant in conditional context*/
			VN_DISPOSE(pp, B_INVAL, 0, kcred);
		} else if (flags & B_FREE) {
			/*LINTED: constant in conditional context*/
			VN_DISPOSE(pp, B_FREE, (flags & B_DONTNEED), kcred);
		} else {
			(void) page_release(pp, 0);
		}
		return (0);
	}

	/*
	 * Page is dirty, get it ready for the write back
	 * and add page to the dirty list.
	 */
	hat_clrrefmod(pp);

	/*
	 * If we're going to free the page when we're done
	 * then we can let others try to use it starting now.
	 * We'll detect the fact that they used it when the
	 * i/o is done and avoid freeing the page.
	 */
	if (flags & B_FREE)
		page_downgrade(pp);


	TRACE_3(TR_FAC_VM, TR_PVN_GETDIRTY,
		"pvn_getdirty:pp %p vp %p offset %llx",
		pp, pp->p_vnode, pp->p_offset);

	return (1);
}

#ifdef VM_STATS
u_int		start_over_count;
u_int		destroy_free_count;
u_int		under_count;
u_int		begin_count;
u_int		pvn_page_lock_count;
u_int		plain_free_count;
u_int		trylock_count;
u_int		loop_count;
u_int		continue_count;
u_int		pvn_getdirty_count;
u_int		pvn_getdirty_doit;
u_int		pvn_getdirty_mapped;
u_int		pvn_getdirty_inval;
u_int		pvn_vpages_race;
#endif /* VM_STATS */

/*
 * Run down the vplist and handle all pages whose offset is >= off.
 * Call the putapage routine to write back each dirty page.  The
 * "exclusive" lock is acquired for each page if B_INVAL or B_FREE
 * is specified, otherwise they are "shared" locked.
 *
 * Flags are {B_ASYNC, B_INVAL, B_FREE, B_DONTNEED, B_TRUNC}
 *
 * It is important that pages are *always* added to the end of the
 * vplist in order to guarantee that this operation will terminate.
 */
int
pvn_vplist_dirty(
	struct vnode *vp,
	u_offset_t	off,
	int		(*putapage)(vnode_t *, page_t *, u_offset_t *,
			size_t *, int, cred_t *),
	int		flags,
	struct cred	*cred)
{
	page_t	*pp;
	page_t		*ppnext;
	page_t		mark;		/* next page marker */
	page_t		end;		/* end page marker */
	int		err = 0;
	kmutex_t	*vphm;

	ASSERT(vp->v_type != VCHR);

	if (vp->v_pages == NULL)
		return (0);

	/*
	 * Serialize vplist_dirty operations on this vnode. We
	 * cannot allow more than 1 thread to insert marker pages
	 * in the vplist.
	 */
	mutex_enter(&vp->v_lock);

	/*
	 * Don't block on VVMLOCK if B_ASYNC is set; this prevents
	 * sync() getting blocked while flushing pages to a dead
	 * NFS server. BugId 1203600.
	 */
	if ((vp->v_flag & VVMLOCK) && (flags & B_ASYNC)) {
		mutex_exit(&vp->v_lock);
		return (EAGAIN);
	}

	while (vp->v_flag & VVMLOCK)
		cv_wait(&vp->v_cv, &vp->v_lock);

	if (vp->v_pages) {
		/*
		 * Since mark and end are local, we must keep the
		 * stack from being swapped out while they're on the
		 * page list.  If some other thread walks the list while
		 * we let go of the lock, there would be trouble.
		 */
		vp->v_flag |= VVMLOCK;
		mutex_exit(&vp->v_lock);

		/*
		 * Build an end marker for the vplist to keep track of
		 * the current end.
		 * All pages on the vplist at the time of the call are
		 * processed.  New pages may be added (always at the
		 * end of the list) and pages may be deleted (from the middle
		 * or end) during this operation.  The new pages added
		 * will not be processed during this invocation.
		 */
		end.p_vnode = vp;
		end.p_offset = (u_offset_t)-1;
		end.p_fsdata = 0;
		mark.p_vnode = vp;
		mark.p_offset = (u_offset_t)-2;
		mark.p_fsdata = 0;

		/*
		 * Grab the lock protecting the list, and
		 * place the `end' marker at the current end of the list.
		 */
		vphm = page_vnode_mutex(vp);
		mutex_enter(vphm);
		/* Test again, since v_pages may have changed */
		if (vp->v_pages)
			page_vpadd(&vp->v_pages->p_vpprev->p_vpnext, &end);
		else {
			/* Nothing to do */
			VM_STAT_ADD(pvn_vpages_race);
			goto leave;
		}
	} else {
		/* Nothing to do */
		mutex_exit(&vp->v_lock);
		return (0);
	}
	VM_STAT_ADD(begin_count);
top:
	for (pp = vp->v_pages; pp != &end; pp = ppnext) {
		VM_STAT_ADD(loop_count);
		ASSERT(pp->p_vnode == vp);
		ppnext = pp->p_vpnext;
		if (pp->p_offset < off) {
			/*
			 * Skip this page.
			 */
			VM_STAT_ADD(under_count);
			continue;
		}

		/*
		 * Always acquire the page lock for all synchronous
		 * operations (invalidate, free and write).
		 */
		if ((flags & B_INVAL) || ((flags & B_ASYNC) == 0)) {
			se_t se;

			/*
			 * If we are supposed to invalidate or synchronously
			 * free this page, then we need to exclusively lock
			 * the page and ensure not to reclaim it if free.
			 */
			se = (flags & (B_INVAL | B_FREE)) ? SE_EXCL : SE_SHARED;
			if (!page_lock(pp, se, vphm, P_NO_RECLAIM)) {
				/*
				 * Can`t lock the page, since page_lock()
				 * dropped `vphm' while it waited,
				 * we have to start all over again.
				 */
				VM_STAT_ADD(start_over_count);
				goto top;
			}
			VM_STAT_ADD(pvn_page_lock_count);
		} else {
			/*
			 * Again, if we are going to free the page,
			 * it needs to be exclusively locked.
			 */
			if (!page_trylock(pp, (flags & B_FREE) ?
			    SE_EXCL : SE_SHARED)) {
				/*
				 * Couldn't lock the page.  This
				 * implies that the page contents
				 * are invalid or it is in the
				 * process of being destroyed.
				 * Since we were only attempting
				 * to write the page out, skip it
				 * on the assumption that someone
				 * else is (or will) deal with it
				 * later.
				 */
				VM_STAT_ADD(continue_count);
				continue;
			}
			VM_STAT_ADD(trylock_count);
		}
		ASSERT(pp->p_vnode == vp);

		/*
		 * Successfully locked the page, now figure out what to
		 * do with it.
		 */
		if (PP_ISFREE(pp)) {
			if (flags & B_INVAL) {
				VM_STAT_ADD(destroy_free_count);

				/*
				 * Invalidate (destroy) it.  In order to
				 * drop 'vphm' and continue from this point
				 * in the list, a marker gets inserted.
				 */
				page_vpadd(&ppnext, &mark);
				mutex_exit(vphm);
				page_destroy_free(pp);
				mutex_enter(vphm);
				ppnext = mark.p_vpnext;
				page_vpsub(&vp->v_pages, &mark);
			} else {
				VM_STAT_ADD(plain_free_count);
				/*
				 * The page is already free and we don't need
				 * to invalidate it, so just unlock it and
				 * go onto the next page.
				 */
				page_unlock(pp);
			}
		} else {
			/*
			 * pvn_getdirty() will figure out what to do
			 * with the page.  Since 'vphm' gets
			 * dropped, the marker is inserted so we
			 * can continue from where we left off.
			 * pvn_getdirty() and `(*putapage)' will take
			 * care of unlocking the page.
			 *
			 * XXX - hat_page_is_mapped is mostly NULL for
			 * pages while running on a long AIM3 benchmark.  It
			 * turns out that 60 million times out of 60.5
			 * million times through the loop, pvn_getdirty only
			 * returned * true 29 thousand times.  That's 1 in 2000.
			 * It appears we drop the lock and pick it up
			 * a few too many times.
			 */
			VM_STAT_ADD(pvn_getdirty_count);
			ASSERT((flags & B_INVAL) ?
			    (VM_STAT_ADD(pvn_getdirty_inval), 1) :
			    (VM_STAT_ADD(pvn_getdirty_mapped), 1));

			page_vpadd(&ppnext, &mark);
			mutex_exit(vphm);

			if (pvn_getdirty(pp, flags)) {
				int	error;

				VM_STAT_ADD(pvn_getdirty_doit);
				/*
				 * The page is dirty and needs to
				 * be written back.  The putapage()
				 * routine will write it, and will
				 * kluster any other adjacent dirty
				 * pages it can.
				 */
				error = (*putapage)(vp, pp, NULL,
				    NULL, flags, cred);
				if (!err)
					err = error;
			}
			mutex_enter(vphm);

			/*
			 * Recompute the next page (i.e., the one right
			 * after the marker page) to be dealt with and
			 * remove the marker page.
			 */
			ppnext = mark.p_vpnext;
			page_vpsub(&vp->v_pages, &mark);
		}
	}

	/*
	 * Remove the `end' marker page and release vplist lock.
	 */
	page_vpsub(&vp->v_pages, &end);
leave:
	mutex_exit(vphm);

	/*
	 * Now, wakeup blocked threads.
	 */
	mutex_enter(&vp->v_lock);
	vp->v_flag &= ~VVMLOCK;
	cv_broadcast(&vp->v_cv);
	mutex_exit(&vp->v_lock);

	return (err);
}

/*
 * Zero out zbytes worth of data. Caller should be aware that this
 * routine may enter back into the fs layer (xxx_getpage). Locks
 * that the xxx_getpage routine may need should not be held while
 * calling this.
 */
void
pvn_vpzero(struct vnode *vp, u_offset_t vplen, size_t zbytes)
{
	caddr_t addr;

	ASSERT(vp->v_type != VCHR);

	if (vp->v_pages == NULL)
		return;

	/*
	 * zbytes may be zero but there still may be some portion of
	 * a page which needs clearing (since zbytes is a function
	 * of filesystem block size, not pagesize.)
	 */
	if (zbytes == 0 && (PAGESIZE - (vplen & PAGEOFFSET)) == 0)
		return;

	/*
	 * We get the last page and handle the partial
	 * zeroing via kernel mappings.  This will make the page
	 * dirty so that we know that when this page is written
	 * back, the zeroed information will go out with it.  If
	 * the page is not currently in memory, then the kzero
	 * operation will cause it to be brought it.  We use kzero
	 * instead of bzero so that if the page cannot be read in
	 * for any reason, the system will not panic.  We need
	 * to zero out a minimum of the fs given zbytes, but we
	 * might also have to do more to get the entire last page.
	 */

	if ((zbytes + (vplen & MAXBOFFSET)) > MAXBSIZE)
		cmn_err(CE_PANIC, "pvn_vptrunc zbytes");
	addr = segmap_getmapflt(segkmap, vp, vplen,
	    MAX(zbytes, PAGESIZE - (vplen & PAGEOFFSET)), 1, S_WRITE);
	(void) kzero(addr + (vplen & MAXBOFFSET),
	    MAX(zbytes, PAGESIZE - (vplen & PAGEOFFSET)));
	(void) segmap_release(segkmap, addr, SM_WRITE | SM_ASYNC);
}

/*
 * Handles common work of the VOP_GETPAGE routines when more than
 * one page must be returned by calling a file system specific operation
 * to do most of the work.  Must be called with the vp already locked
 * by the VOP_GETPAGE routine.
 */
int
pvn_getpages(
	int (*getpage)(vnode_t *, u_offset_t, size_t, u_int *, page_t *[],
		size_t, struct seg *, caddr_t, enum seg_rw, cred_t *),
	struct vnode *vp,
	u_offset_t off,
	size_t len,
	u_int *protp,
	page_t *pl[],
	size_t plsz,
	struct seg *seg,
	caddr_t addr,
	enum seg_rw rw,
	struct cred *cred)
{
	page_t **ppp;
	u_offset_t o, eoff;
	size_t sz, xlen;
	int err;

	ASSERT(plsz >= len);		/* insure that we have enough space */

	/*
	 * Loop one page at a time and let getapage function fill
	 * in the next page in array.  We only allow one page to be
	 * returned at a time (except for the last page) so that we
	 * don't have any problems with duplicates and other such
	 * painful problems.  This is a very simple minded algorithm,
	 * but it does the job correctly.  We hope that the cost of a
	 * getapage call for a resident page that we might have been
	 * able to get from an earlier call doesn't cost too much.
	 */
	ppp = pl;
	sz = PAGESIZE;
	eoff = off + len;
	xlen = len;
	for (o = off; o < eoff; o += PAGESIZE, addr += PAGESIZE,
	    xlen -= PAGESIZE) {
		if (o + PAGESIZE >= eoff) {
			/*
			 * Last time through - allow the all of
			 * what's left of the pl[] array to be used.
			 */
			sz = plsz - (o - off);
		}
		err = (*getpage)(vp, o, xlen, protp, ppp, sz, seg, addr,
		    rw, cred);
		if (err) {
			/*
			 * Release any pages we already got.
			 */
			if (o > off && pl != NULL) {
				for (ppp = pl; *ppp != NULL; *ppp++ = NULL)
					(void) page_release(*ppp, 1);
			}
			break;
		}
		if (pl != NULL)
			ppp++;
	}
	return (err);
}

/*
 * Initialize the page list array.
 */
void
pvn_plist_init(page_t *pp, page_t *pl[], size_t plsz,
    u_offset_t off, size_t io_len, enum seg_rw rw)
{
	ssize_t sz;
	page_t *ppcur, **ppp;

	if (plsz >= io_len) {
		/*
		 * Everything fits, set up to load
		 * all the pages.
		 */
		sz = io_len;
	} else {
		/*
		 * Set up to load plsz worth
		 * starting at the needed page.
		 */
		while (pp->p_offset != off) {
			/* XXX - Do we need this assert? */
			ASSERT(pp->p_next->p_offset !=
			    pp->p_offset);
			/*
			 * Remove page from the i/o list,
			 * release the i/o and the page lock.
			 */
			ppcur = pp;
			page_sub(&pp, ppcur);
			page_io_unlock(ppcur);
			(void) page_release(ppcur, 1);
		}
		sz = plsz;
	}

	/*
	 * Initialize the page list array.
	 */
	ppp = pl;
	do {
		ppcur = pp;
		*ppp++ = ppcur;
		page_sub(&pp, ppcur);
		page_io_unlock(ppcur);
		if (rw != S_CREATE)
			page_downgrade(ppcur);
		sz -= PAGESIZE;
	} while (sz > 0 && pp != NULL);
	*ppp = NULL;		/* terminate list */

	/*
	 * Now free the remaining pages that weren't
	 * loaded in the page list.
	 */
	while (pp != NULL) {
		ppcur = pp;
		page_sub(&pp, ppcur);
		page_io_unlock(ppcur);
		(void) page_release(ppcur, 1);
	}
}
