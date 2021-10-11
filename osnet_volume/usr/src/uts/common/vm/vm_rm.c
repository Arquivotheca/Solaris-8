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
 * 	(c) 1986,1987,1988,1989,1996  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */

#ident	"@(#)vm_rm.c	1.40	96/08/08 SMI"
/*	From:	SVr4.0	"kernel:vm/vm_rm.c	1.9"		*/

/*
 * VM - resource manager
 */

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mman.h>
#include <sys/sysmacros.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>

#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg_vn.h>
#include <vm/rm.h>
#include <vm/seg.h>
#include <vm/page.h>

/*
 * This routine is called when we couldn't allocate an anon slot.
 * For now, we simply print out a message and kill of the process
 * who happened to have gotten burned.
 *
 * XXX - swap reservation needs lots of work so this only happens in
 * `nice' places or we need to have a method to allow for recovery.
 */
void
rm_outofanon()
{
	struct proc *p;

	p = ttoproc(curthread);
	cmn_err(CE_WARN,
	    "Sorry, pid %d (%s) was killed due to lack of swap space\n",
	    (int)p->p_pid, u.u_comm);
	/*
	 * To be sure no looping (e.g. in vmsched trying to
	 * swap out) mark process locked in core (as though
	 * done by user) after killing it so no one will try
	 * to swap it out.
	 */
	psignal(p, SIGKILL);
	mutex_enter(&p->p_lock);
	p->p_flag |= SLOCK;
	mutex_exit(&p->p_lock);
	/*NOTREACHED*/
}

void
rm_outofhat()
{
	cmn_err(CE_PANIC, "out of mapping resources");		/* XXX */
	/*NOTREACHED*/
}

/*
 * Yield the size of an address space.
 *
 * The size can only be used as a hint since we cannot guarantee it
 * will stay the same size unless the as->a_lock is held by the caller.
 */
size_t
rm_assize(struct as *as)
{
	size_t size = 0;
	struct seg *seg;
	struct segvn_data *svd;
	extern struct seg_ops segdev_ops;	/* needs a header file */

	ASSERT(as != NULL && AS_READ_HELD(as, &as->a_lock));

	if (as == &kas)
		return (0);

	for (seg = AS_SEGP(as, as->a_segs); seg != NULL;
	    seg = AS_SEGP(as, seg->s_next)) {
		if (seg->s_ops == &segdev_ops &&
		    SEGOP_GETTYPE(seg, seg->s_base) == 0) {
			/*
			 * Don't include mappings of /dev/null.  These just
			 * reserve address space ranges and have no memory.
			 * We cheat by knowing that these segments come
			 * from segdev and have no mapping type.
			 */
			/* EMPTY */;
		} else if (seg->s_ops == &segvn_ops &&
		    (svd = (struct segvn_data *)seg->s_data) != NULL &&
		    (svd->vp == NULL || svd->vp->v_type != VREG) &&
		    (svd->flags & MAP_NORESERVE)) {
			/*
			 * Don't include MAP_NORESERVE pages in the
			 * address range unless their mappings have
			 * actually materialized.  We cheat by knowing
			 * that segvn is the only segment driver that
			 * supports MAP_NORESERVE and that the actual
			 * number of bytes reserved is in the segment's
			 * private data structure.
			 */
			size += svd->swresv;
		} else {
			caddr_t addr = seg->s_base;
			size_t segsize = seg->s_size;
			vnode_t *vp;
			vattr_t vattr;

			/*
			 * If the segment is mapped beyond the end of the
			 * underlying mapped file, if any, then limit the
			 * segment's size contribution to the file size.
			 */
			vattr.va_mask = AT_SIZE;
			if (seg->s_ops == &segvn_ops &&
			    SEGOP_GETVP(seg, addr, &vp) == 0 &&
			    vp != NULL && vp->v_type == VREG &&
			    VOP_GETATTR(vp, &vattr, ATTR_HINT, CRED()) == 0) {
				u_offset_t filesize = vattr.va_size;
				u_offset_t offset = SEGOP_GETOFFSET(seg, addr);

				if (filesize < offset)
					filesize = 0;
				else
					filesize -= offset;
				filesize =
					roundup(filesize, (u_offset_t)PAGESIZE);
				if ((u_offset_t)segsize > filesize)
					segsize = filesize;
			}
			size += segsize;
		}
	}

	return (size);
}

/*
 * Yield the memory claim requirement for an address space.
 *
 * This is currently implemented as the number of active hardware
 * translations that have page structures.  Therefore, it can
 * underestimate the traditional resident set size, eg, if the
 * physical page is present and the hardware translation is missing;
 * and it can overestimate the rss, eg, if there are active
 * translations to a frame buffer with page structs.
 * Also, it does not take sharing into account.
 */
size_t
rm_asrss(as)
	register struct as *as;
{
	if (as != (struct as *)NULL && as != &kas)
		return ((size_t)btop(hat_get_mapped_size(as->a_hat)));
	else
		return (0);
}

/*
 * Return a 16-bit binary fraction representing the percent of total memory
 * used by this address space.  Binary point is to right of high-order bit.
 * Defined as the ratio of a_rss for the process to total physical memory.
 * This assumes 2s-complement arithmetic and that shorts and longs are
 * 16 bits and 32 bits, respectively.
 */
u_short
rm_pctmemory(struct as *as)
{
	/* This can't overflow */
	u_long num = (u_long)rm_asrss(as) << (PAGESHIFT-1);
	int shift = 16 - PAGESHIFT;
	u_long total = total_pages;

	if (shift < 0) {
		num >>= (-shift);
		shift = 0;
	}
	while (shift > 0 && (num & 0x80000000) == 0) {
		shift--;
		num <<= 1;
	}
	if (shift > 0)
		total >>= shift;

	return (num / total);
}
