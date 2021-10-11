/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/
/*	Copyright (c) 1994 Sun Microsystems, Inc. */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)memcntl.c	1.31	98/06/04 SMI" /* from SVR4.0 1.34 */

#include <sys/types.h>
#include <sys/bitmap.h>
#include <sys/sysmacros.h>
#include <sys/kmem.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/mman.h>
#include <sys/tuneable.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/vmsystm.h>
#include <sys/debug.h>

#include <vm/as.h>
#include <vm/seg.h>

static void mem_unlock(struct as *, caddr_t, int, caddr_t, ulong *,
	size_t, size_t);

/*
 * Memory control operations
 */

int
memcntl(caddr_t addr, size_t len, int cmd, caddr_t arg, int attr, int mask)
{
	struct seg *seg;			/* working segment */
	size_t rlen = (size_t)0;		/* rounded as length */
	struct as *as = ttoproc(curthread)->p_as;
	ulong *mlock_map;			/* pointer to bitmap used */
						/* to represent the locked */
						/* pages. */
	caddr_t	raddr;				/* rounded address counter */
	size_t  mlock_size;			/* size of bitmap */
	size_t inx;
	size_t npages;
	int error = 0;
	faultcode_t fc;
	uintptr_t iarg;

	if (mask)
		return (set_errno(EINVAL));
	if ((cmd == MC_LOCKAS) || (cmd == MC_UNLOCKAS)) {
		if ((addr != 0) || (len != 0)) {
			return (set_errno(EINVAL));
		}
	} else {
		if (((uintptr_t)addr & PAGEOFFSET) != 0)
			return (set_errno(EINVAL));

		/*
		 * We're only concerned with the address range
		 * here, not the protections.  The protections
		 * are only used as a "filter" in this code,
		 * they aren't set or modified here.
		 */
		if (valid_usr_range(addr, len, 0, as,
		    as->a_userlimit) != RANGE_OKAY)
			return (set_errno(ENOMEM));
	}

	if ((VALID_ATTR & attr) != attr)
		return (set_errno(EINVAL));

	if ((attr & SHARED) && (attr & PRIVATE))
		return (set_errno(EINVAL));

	if (((cmd == MC_LOCKAS) || (cmd == MC_LOCK) ||
	    (cmd == MC_UNLOCKAS) || (cmd == MC_UNLOCK)) &&
	    (!suser(CRED())))
		return (set_errno(EPERM));

	if (attr)
		attr |= PROT_USER;

	switch (cmd) {
	case MC_SYNC:
		/*
		 * MS_SYNC used to be defined to be zero but is now non-zero.
		 * For binary compatibility we still accept zero
		 * (the absence of MS_ASYNC) to mean the same thing.
		 */
		iarg = (uintptr_t)arg;
		if ((iarg & ~MS_INVALIDATE) == 0)
			iarg |= MS_SYNC;

		if (((iarg & ~(MS_SYNC|MS_ASYNC|MS_INVALIDATE)) != 0) ||
			((iarg & (MS_SYNC|MS_ASYNC)) == (MS_SYNC|MS_ASYNC)))

			error = set_errno(EINVAL);
		else {
			error = as_ctl(as, addr, len, cmd, attr, iarg, NULL, 0);
			if (error)
				(void) set_errno(error);
		}
		return (error);
	case MC_LOCKAS:
		if ((uintptr_t)arg & ~(MCL_FUTURE|MCL_CURRENT) ||
							(uintptr_t)arg == 0)
			return (set_errno(EINVAL));

		AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
		seg = AS_SEGP(as, as->a_segs);
		if (seg == NULL) {
			AS_LOCK_EXIT(as, &as->a_lock);
			return (0);
		}
		do {
			raddr = (caddr_t)((uintptr_t)seg->s_base & PAGEMASK);
			rlen += (((uintptr_t)(seg->s_base + seg->s_size) +
				PAGEOFFSET) & PAGEMASK) - (uintptr_t)raddr;
		} while ((seg = AS_SEGP(as, seg->s_next)) != NULL);
		AS_LOCK_EXIT(as, &as->a_lock);

		break;
	case MC_LOCK:
		/*
		 * Normalize addresses and lengths
		 */
		raddr = (caddr_t)((uintptr_t)addr & PAGEMASK);
		rlen  = (((uintptr_t)(addr + len) + PAGEOFFSET) & PAGEMASK) -
				(uintptr_t)raddr;
		break;
	case MC_UNLOCKAS:
	case MC_UNLOCK:
		mlock_map = NULL;
		mlock_size = NULL;
		break;
	case MC_ADVISE:
		switch ((uintptr_t)arg) {
		case MADV_WILLNEED:
			fc = as_faulta(as, addr, len);
			if (fc) {
				if (FC_CODE(fc) == FC_OBJERR)
					error = set_errno(FC_ERRNO(fc));
				else
					error = set_errno(EINVAL);
				return (error);
			}
			break;

		case MADV_DONTNEED:
			/*
			 * For now, don't need is turned into an as_ctl(MC_SYNC)
			 * operation flagged for async invalidate.
			 */
			error = as_ctl(as, addr, len, MC_SYNC, attr,
			    MS_ASYNC | MS_INVALIDATE, NULL, 0);
			if (error)
				(void) set_errno(error);
			return (error);

		default:
			error = as_ctl(as, addr, len, cmd, attr,
			    (uintptr_t)arg, NULL, 0);
			if (error)
				(void) set_errno(error);
			return (error);
		}
		break;
	default:
		return (set_errno(EINVAL));
	}

	if ((cmd == MC_LOCK) || (cmd == MC_LOCKAS)) {
		mlock_size = BT_BITOUL(btopr(rlen));
		mlock_map = (ulong *)kmem_zalloc(mlock_size *
		    sizeof (ulong), KM_SLEEP);
	}

	error = as_ctl(as, addr, len, cmd, attr,
			(uintptr_t)arg, mlock_map, 0);

	if (cmd == MC_LOCK || cmd == MC_LOCKAS) {
		if (error) {
			if (cmd == MC_LOCKAS) {
				inx = 0;
				npages = 0;
				AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
				for (seg = AS_SEGP(as, as->a_segs);
				    seg != NULL;
				    seg = AS_SEGP(as, seg->s_next)) {
					raddr = (caddr_t)((uintptr_t)seg->s_base
							& PAGEMASK);
					npages += seg_pages(seg);
					AS_LOCK_EXIT(as, &as->a_lock);
					mem_unlock(as, raddr, attr, arg,
					    mlock_map, inx, npages);
					AS_LOCK_ENTER(as, &as->a_lock,
					    RW_READER);
					inx += seg_pages(seg);
				}
				AS_LOCK_EXIT(as, &as->a_lock);
			} else  /* MC_LOCK */
				mem_unlock(as, raddr, attr, arg,
				    mlock_map, (size_t)0, btopr(rlen));
		}
		kmem_free(mlock_map, mlock_size * sizeof (ulong));
	}
	if (error)
		(void) set_errno(error);
	return (error);
}

static void
mem_unlock(struct as *as, caddr_t addr, int attr, caddr_t arg,
	ulong *bitmap, size_t position, size_t nbits)
{
	caddr_t	range_start;
	size_t	pos1, pos2;
	size_t	size;

	pos1 = position;

	while (bt_range(bitmap, &pos1, &pos2, nbits)) {
		size = ptob((pos2 - pos1) + 1);
		range_start = (caddr_t)((uintptr_t)addr + ptob(pos1));
		(void) as_ctl(as, range_start, size, MC_UNLOCK,
			attr, (uintptr_t)arg, NULL, 0);
		pos1 = pos2 + 1;
	}
}
