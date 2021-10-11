/*
 * Copyright (c) 1988-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)page_lock.c 1.40     98/11/04 SMI"

/*
 * VM - page locking primitives
 */
#include <sys/param.h>
#include <sys/t_lock.h>
#include <sys/vtrace.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/vnode.h>
#include <sys/bitmap.h>
#include <sys/lockstat.h>
#include <sys/condvar_impl.h>
#include <vm/page.h>
#include <vm/seg_enum.h>

/*
 * This global mutex is for logical page locking.
 * The following fields in the page structure are protected
 * by this lock:
 *
 *	p_lckcnt
 *	p_cowcnt
 */
kmutex_t page_llock;

/*
 * This is a global lock for the logical page free list.  The
 * logical free list, in this implementation, is maintained as two
 * separate physical lists - the cache list and the free list.
 */
kmutex_t  page_freelock;

/*
 * The hash table, page_hash[], the p_selock fields, and the
 * list of pages associated with vnodes are protected by arrays of mutexes.
 *
 * Unless the hashes are changed radically, the table sizes must be
 * a power of two.  Also, we typically need more mutexes for the
 * vnodes since these locks are occasionally held for long periods.
 * And since there seem to be two special vnodes (kvp and swapvp),
 * we make room for private mutexes for them.
 *
 * The pse_mutex[] array holds the mutexes to protect the p_selock
 * fields of all page_t structures.
 *
 * PAGE_SE_MUTEX(pp) returns the address of the appropriate mutex
 * when given a pointer to a page_t.
 *
 * PSE_TABLE_SIZE must be a power of two.  One could argue that we
 * should go to the trouble of setting it up at run time and base it
 * on memory size rather than the number of compile time CPUs.
 */
#if NCPU < 4
#define	PH_TABLE_SIZE	16
#define	VP_SHIFT	7
#else
#define	PH_TABLE_SIZE	128
#define	VP_SHIFT	9
#endif

/*
 * XX64	We should be using physmem size to calculate PSE_TABLE_SIZE,
 *	PSE_SHIFT, PIO_SHIFT.
 *
 *	These might break in 64 bit world.
 */
#define	PSE_SHIFT	6		/* next power of 2 bigger than page_t */

#define	PSE_TABLE_SIZE	128		/* number of mutexes to have */

#define	PIO_SHIFT	PSE_SHIFT	/* next power of 2 bigger than page_t */
#define	PIO_TABLE_SIZE	PSE_TABLE_SIZE	/* number of io mutexes to have */

kmutex_t	ph_mutex[PH_TABLE_SIZE];
kmutex_t	pse_mutex[PSE_TABLE_SIZE];
kmutex_t	pio_mutex[PIO_TABLE_SIZE];
u_int		ph_mutex_shift;

#define	PAGE_SE_MUTEX(pp) \
	    &pse_mutex[(((uintptr_t)pp) >> PSE_SHIFT) & (PSE_TABLE_SIZE - 1)]

#define	PAGE_IO_MUTEX(pp) \
	    &pio_mutex[(((uintptr_t)pp) >> PIO_SHIFT) & (PIO_TABLE_SIZE - 1)]

/*
 * The vph_mutex[] array  holds the mutexes to protect the vnode chains,
 * (i.e., the list of pages anchored by v_pages and connected via p_vpprev
 * and p_vpnext).
 *
 * The page_vnode_mutex(vp) function returns the address of the appropriate
 * mutex from this array given a pointer to a vnode.  It is complicated
 * by the fact that the kernel's vnode and the swapfs vnode are referenced
 * frequently enough to warrent their own mutexes.
 *
 * The VP_HASH_FUNC returns the index into the vph_mutex array given
 * an address of a vnode.
 */

/*
 * XX64	VPH_TABLE_SIZE and VP_HASH_FUNC might break in 64 bit world.
 *	Need to review again.
 */
#define	VPH_TABLE_SIZE	(2 << VP_SHIFT)

#define	VP_HASH_FUNC(vp) \
	((((uintptr_t)(vp) >> 6) + \
	    ((uintptr_t)(vp) >> 10) + \
	    ((uintptr_t)(vp) >> 10) + \
	    ((uintptr_t)(vp) >> 12)) \
	    & (VPH_TABLE_SIZE - 1))

extern	struct vnode	kvp;

kmutex_t	vph_mutex[VPH_TABLE_SIZE + 2];

/*
 * Initialize the locks used by the Virtual Memory Management system.
 */
void
page_lock_init()
{
	/*
	 * page_hashsz gets set up at startup time.
	 */
	ph_mutex_shift = highbit(page_hashsz / PH_TABLE_SIZE);
}

/*
 * At present we only use page ownership to aid debugging, so it's
 * OK if the owner field isn't exact.  In the 32-bit world two thread ids
 * can map to the same owner because we just 'or' in 0x80000000, so that
 * (for example) 0x2faced00 and 0xafaced00 both map to 0xafaced00.
 * In the 64-bit world, p_selock may not be large enough to hold a full
 * thread pointer.  If we ever need precise ownership (e.g. if we implement
 * priority inheritance for page locks) then p_selock should become a
 * uintptr_t and SE_WRITER should be -((uintptr_t)curthread >> 1).
 */
#define	SE_WRITER	((selock_t)curthread | INT_MIN)
#define	SE_READER	1
/*
 * A page that is deleted must be marked as such using the
 * page_lock_delete() function. The page must be exclusively locked.
 * The SE_DELETED marker is put in p_selock when this function is called.
 * SE_DELETED must be distinct from any SE_WRITER value.
 */
#define	SE_DELETED	(1 | INT_MIN)

#ifdef VM_STATS
u_int	vph_kvp_count;
u_int	vph_swapfsvp_count;
u_int	vph_other;
#endif /* VM_STATS */

#ifdef VM_STATS
u_int	page_lock_count;
u_int	page_lock_miss;
u_int	page_lock_miss_lock;
u_int	page_lock_reclaim;
u_int	page_lock_bad_reclaim;
u_int	page_lock_same_page;
u_int	page_lock_upgrade;
u_int	page_lock_upgrade_failed;
u_int	page_lock_deleted;

u_int	page_trylock_locked;
u_int	page_trylock_missed;

u_int	page_try_reclaim_upgrade;
#endif /* VM_STATS */


/*
 * Acquire the "shared/exclusive" lock on a page.
 *
 * Returns 1 on success and locks the page appropriately.
 *	   0 on failure and does not lock the page.
 *
 * If `lock' is non-NULL, it will be dropped and and reacquired in the
 * failure case.  This routine can block, and if it does
 * it will always return a failure since the page identity [vp, off]
 * or state may have changed.
 */

int
page_lock(page_t *pp, se_t se, kmutex_t *lock, reclaim_t reclaim)
{
	int		retval;
	kmutex_t	*pse = PAGE_SE_MUTEX(pp);
	int		upgraded;
	int		reclaim_it;

	ASSERT(lock != NULL ? MUTEX_HELD(lock) : 1);

	VM_STAT_ADD(page_lock_count);

	upgraded = 0;
	reclaim_it = 0;

	mutex_enter(pse);

	if ((reclaim == P_RECLAIM) && (PP_ISFREE(pp))) {

		reclaim_it = 1;
		if (se == SE_SHARED) {
			/*
			 * This is an interesting situation.
			 *
			 * Remember that p_free can only change if
			 * p_selock < 0.
			 * p_free does not depend on our holding `pse'.
			 * And, since we hold `pse', p_selock can not change.
			 * So, if p_free changes on us, the page is already
			 * exclusively held, and we would fail to get p_selock
			 * regardless.
			 *
			 * We want to avoid getting the share
			 * lock on a free page that needs to be reclaimed.
			 * It is possible that some other thread has the share
			 * lock and has left the free page on the cache list.
			 * pvn_vplist_dirty() does this for brief periods.
			 * If the se_share is currently SE_EXCL, we will fail
			 * to acquire p_selock anyway.  Blocking is the
			 * right thing to do.
			 * If we need to reclaim this page, we must get
			 * exclusive access to it, force the upgrade now.
			 * Again, we will fail to acquire p_selock if the
			 * page is not free and block.
			 */
			upgraded = 1;
			se = SE_EXCL;
			VM_STAT_ADD(page_lock_upgrade);
		}
	}

	retval = 0;
	if (se == SE_EXCL) {
		if (pp->p_selock == 0) {
			THREAD_KPRI_REQUEST();
			pp->p_selock = SE_WRITER;
			retval = 1;
		}
	} else {
		if (pp->p_selock >= 0) {
			pp->p_selock += SE_READER;
			retval = 1;
		}
	}

	if (retval == 0) {
		if (pp->p_selock == SE_DELETED) {
			VM_STAT_ADD(page_lock_deleted);
			mutex_exit(pse);
			return (retval);
		}

#ifdef VM_STATS
		VM_STAT_ADD(page_lock_miss);
		if (upgraded) {
			VM_STAT_ADD(page_lock_upgrade_failed);
		}
#endif
		if (lock) {
			VM_STAT_ADD(page_lock_miss_lock);
			mutex_exit(lock);
		}

		/*
		 * Now, wait for the page to be unlocked and
		 * release the lock protecting p_cv and p_selock.
		 */
		cv_wait(&pp->p_cv, pse);
		mutex_exit(pse);

		/*
		 * The page identity may have changed while we were
		 * blocked.  If we are willing to depend on "pp"
		 * still pointing to a valid page structure (i.e.,
		 * assuming page structures are not dynamically allocated
		 * or freed), we could try to lock the page if its
		 * identity hasn't changed.
		 *
		 * This needs to be measured, since we come back from
		 * cv_wait holding pse (the expensive part of this
		 * operation) we might as well try the cheap part.
		 * Though we would also have to confirm that dropping
		 * `lock' did not cause any grief to the callers.
		 */
		if (lock) {
			mutex_enter(lock);
		}
	} else {
		/*
		 * We have the page lock.
		 * If we needed to reclaim the page, and the page
		 * needed reclaiming (ie, it was free), then we
		 * have the page exclusively locked.  We may need
		 * to downgrade the page.
		 */
		ASSERT((upgraded) ?
		    ((PP_ISFREE(pp)) && PAGE_EXCL(pp)) : 1);
		mutex_exit(pse);

		/*
		 * We now hold this page's lock, either shared or
		 * exclusive.  This will prevent its identity from changing.
		 * The page, however, may or may not be free.  If the caller
		 * requested, and it is free, go reclaim it from the
		 * free list.  If the page can't be reclaimed, return failure
		 * so that the caller can start all over again.
		 *
		 * NOTE:page_reclaim() releases the page lock (p_selock)
		 *	if it can't be reclaimed.
		 */
		if (reclaim_it) {
			if (!page_reclaim(pp, lock)) {
				VM_STAT_ADD(page_lock_bad_reclaim);
				retval = 0;
			} else {
				VM_STAT_ADD(page_lock_reclaim);
				if (upgraded) {
					page_downgrade(pp);
				}
			}
		}
	}
	return (retval);
}

/*
 * Read the comments inside of page_lock() carefully.
 */
int
page_try_reclaim_lock(page_t *pp, se_t se)
{
	kmutex_t *pse = PAGE_SE_MUTEX(pp);
	selock_t old;

	mutex_enter(pse);

	old = pp->p_selock;

	if (se == SE_SHARED) {
		if (!PP_ISFREE(pp)) {
			if (old >= 0) {
				pp->p_selock = old + SE_READER;
				mutex_exit(pse);
				return (1);
			}
			mutex_exit(pse);
			return (0);
		}
		/*
		 * The page is free, so we really want SE_EXCL (below)
		 */
		VM_STAT_ADD(page_try_reclaim_upgrade);
	}

	if (old == 0) {
		THREAD_KPRI_REQUEST();
		pp->p_selock = SE_WRITER;
		mutex_exit(pse);
		return (1);
	}

	mutex_exit(pse);
	return (0);
}

/*
 * Acquire a page's "shared/exclusive" lock, but never block.
 * Returns 1 on success, 0 on failure.
 */
int
page_trylock(page_t *pp, se_t se)
{
	kmutex_t *pse = PAGE_SE_MUTEX(pp);

	mutex_enter(pse);

	if (se == SE_EXCL) {
		if (pp->p_selock == 0) {
			THREAD_KPRI_REQUEST();
			pp->p_selock = SE_WRITER;
			mutex_exit(pse);
			return (1);
		}
	} else {
		if (pp->p_selock >= 0) {
			pp->p_selock += SE_READER;
			mutex_exit(pse);
			return (1);
		}
	}

	mutex_exit(pse);
	return (0);
}

/*
 * Release the page's "shared/exclusive" lock and wake up anyone
 * who might be waiting for it.
 */
void
page_unlock(page_t *pp)
{
	kmutex_t *pse = PAGE_SE_MUTEX(pp);
	selock_t old;

	mutex_enter(pse);
	old = pp->p_selock;
	if (old == SE_READER) {
		pp->p_selock = 0;
		if (CV_HAS_WAITERS(&pp->p_cv))
			cv_broadcast(&pp->p_cv);
	} else if (old == SE_DELETED) {
		panic("page_unlock: page %p is deleted", pp);
	} else if (old < 0) {
		THREAD_KPRI_RELEASE();
		pp->p_selock = 0;
		if (CV_HAS_WAITERS(&pp->p_cv))
			cv_broadcast(&pp->p_cv);
	} else if (old > SE_READER) {
		pp->p_selock = old - SE_READER;
	} else {
		panic("page_unlock: page %p is not locked", pp);
	}
	mutex_exit(pse);
}

/*
 * Try to upgrade the lock on the page from a "shared" to an
 * "exclusive" lock.  Since this upgrade operation is done while
 * holding the mutex protecting this page, no one else can acquire this page's
 * lock and change the page. Thus, it is safe to drop the "shared"
 * lock and attempt to acquire the "exclusive" lock.
 *
 * Returns 1 on success, 0 on failure.
 */
int
page_tryupgrade(page_t *pp)
{
	kmutex_t *pse = PAGE_SE_MUTEX(pp);

	ASSERT(PAGE_SHARED(pp));

	mutex_enter(pse);
	if (pp->p_selock == SE_READER) {
		THREAD_KPRI_REQUEST();
		pp->p_selock = SE_WRITER;	/* convert to exclusive lock */
		mutex_exit(pse);
		return (1);
	}
	mutex_exit(pse);
	return (0);
}

/*
 * Downgrade the "exclusive" lock on the page to a "shared" lock
 * while holding the mutex protecting this page's p_selock field.
 */
void
page_downgrade(page_t *pp)
{
	kmutex_t *pse = PAGE_SE_MUTEX(pp);

	ASSERT(pp->p_selock != SE_DELETED);
	ASSERT(PAGE_EXCL(pp));

	mutex_enter(pse);
	THREAD_KPRI_RELEASE();
	pp->p_selock = SE_READER;
	if (CV_HAS_WAITERS(&pp->p_cv))
		cv_broadcast(&pp->p_cv);
	mutex_exit(pse);
}

void
page_lock_delete(page_t *pp)
{
	kmutex_t *pse = PAGE_SE_MUTEX(pp);

	ASSERT(PAGE_EXCL(pp));
	ASSERT(pp->p_vnode == NULL);
	ASSERT(pp->p_offset == (u_offset_t)-1);
	ASSERT(!PP_ISFREE(pp));

	mutex_enter(pse);
	THREAD_KPRI_RELEASE();
	pp->p_selock = SE_DELETED;
	if (CV_HAS_WAITERS(&pp->p_cv))
		cv_broadcast(&pp->p_cv);
	mutex_exit(pse);
}

/*
 * Implement the io lock for pages
 */
void
page_iolock_init(page_t *pp)
{
	pp->p_iolock_state = 0;
	cv_init(&pp->p_io_cv, NULL, CV_DEFAULT, NULL);
}

/*
 * Acquire the i/o lock on a page.
 */
void
page_io_lock(page_t *pp)
{
	kmutex_t *pio;

	pio = PAGE_IO_MUTEX(pp);
	mutex_enter(pio);
	while (pp->p_iolock_state & PAGE_IO_INUSE) {
		cv_wait(&(pp->p_io_cv), pio);
	}
	pp->p_iolock_state |= PAGE_IO_INUSE;
	mutex_exit(pio);
}

/*
 * Release the i/o lock on a page.
 */
void
page_io_unlock(page_t *pp)
{
	kmutex_t *pio;

	pio = PAGE_IO_MUTEX(pp);
	mutex_enter(pio);
	cv_signal(&pp->p_io_cv);
	pp->p_iolock_state &= ~PAGE_IO_INUSE;
	mutex_exit(pio);
}

/*
 * Try to acquire the i/o lock on a page without blocking.
 * Returns 1 on success, 0 on failure.
 */
int
page_io_trylock(page_t *pp)
{
	kmutex_t *pio;

	if (pp->p_iolock_state & PAGE_IO_INUSE)
		return (0);

	pio = PAGE_IO_MUTEX(pp);
	mutex_enter(pio);

	if (pp->p_iolock_state & PAGE_IO_INUSE) {
		mutex_exit(pio);
		return (0);
	}
	pp->p_iolock_state |= PAGE_IO_INUSE;
	mutex_exit(pio);

	return (1);
}

/*
 * Assert that the i/o lock on a page is held.
 * Returns 1 on success, 0 on failure.
 */
int
page_iolock_assert(page_t *pp)
{
	return (pp->p_iolock_state & PAGE_IO_INUSE);
}

/*
 * Wrapper exported to kernel routines that are built
 * platform-independent (the macro is platform-dependent;
 * the size of vph_mutex[] is based on NCPU).
 */
kmutex_t *
page_vnode_mutex(vnode_t *vp)
{
	if (vp == &kvp)
		return (&vph_mutex[VPH_TABLE_SIZE + 0]);

	return (&vph_mutex[VP_HASH_FUNC(vp)]);
}

kmutex_t *
page_se_mutex(page_t *pp)
{
	return (PAGE_SE_MUTEX(pp));
}
