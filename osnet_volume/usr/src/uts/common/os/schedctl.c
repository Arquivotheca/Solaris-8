/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)schedctl.c	1.27	99/08/17 SMI"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/schedctl.h>
#include <sys/proc.h>
#include <sys/thread.h>
#include <sys/class.h>
#include <sys/cred.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/stack.h>
#include <sys/debug.h>
#include <sys/cpuvar.h>
#include <sys/sobject.h>
#include <sys/door.h>
#include <sys/modctl.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <sys/vmsystm.h>
#include <sys/mman.h>
#include <sys/vnode.h>
#include <sys/swap.h>
#include <sys/lwp.h>
#include <sys/bitmap.h>
#include <sys/atomic.h>
#include <sys/fcntl.h>
#include <vm/seg_kp.h>
#include <vm/seg_vn.h>
#include <vm/as.h>
#include <fs/fs_subr.h>


/*
 * Page handling structures.  This is set up as a list of per-pool
 * control structures (sc_pool_list), with sc_pool_head pointing to
 * the first.  Each per-pool structure points to a list of per-page
 * control structures (sc_page_ctl), one per page contained in that
 * pool.  The per-page structures point to the actual pages, along
 * with a list of per-process mapping data (sc_proc_map).  The
 * per-process structures contain pointers to the user address for
 * each mapped page.
 *
 * All data is protected by a single spc_lock.  Since this lock is
 * held while waiting for memory, schedctl_shared_alloc should not
 * be called while holding p_lock.
 */

static kmutex_t	spc_lock;

/*
 * This should probably be changed to a per-page hash table at some point.
 */
typedef struct sc_proc_map {
	struct sc_proc_map *spm_next;
	proc_t		*spm_proc;
	caddr_t		spm_uaddr;
	int		spm_refcnt;
} sc_proc_map_t;

typedef struct sc_page_ctl {
	struct sc_page_ctl *spc_next;
	sc_shared_t	*spc_base;	/* base of kernel page */
	sc_shared_t	*spc_end;	/* end of usable space */
	ulong_t		*spc_map;	/* bitmap of allocated space on page */
	size_t		spc_space;	/* amount of space on page */
	sc_proc_map_t	*spc_proclist;	/* list of mappings to this page */
	struct anon_map	*spc_amp;	/* anonymous memory structure */
} sc_page_ctl_t;

typedef struct sc_pool_list {
	struct sc_pool_list *spl_next;
	long		spl_id;
	sc_page_ctl_t	*spl_pagelist;
} sc_pool_list_t;

static sc_pool_list_t *sc_pool_head = NULL;
static size_t	sc_pagesize;		/* size of usable space on page */
static size_t	sc_bitmap_len;		/* # of bits in allocation bitmap */
static size_t	sc_bitmap_words;	/* # of words in allocation bitmap */

/* Context ops */
static void		schedctl_save(sc_shared_t *);
static void		schedctl_restore(sc_shared_t *);
static void		schedctl_fork(kthread_id_t, kthread_id_t);
static void		schedctl_null(sc_shared_t *);

/* Functions for handling shared pool of pages */
static void		schedctl_shared_init(void);
static int		schedctl_shared_alloc(long, sc_shared_t **,
			    sc_shared_t **);
static void		schedctl_shared_free(long, sc_shared_t *);
static void		schedctl_shared_disown(long, sc_shared_t *,
			    proc_t *, proc_t *);
static sc_pool_list_t	**schedctl_pool_lookup(long);
static sc_page_ctl_t	**schedctl_page_lookup(sc_pool_list_t *, sc_shared_t *);
static sc_proc_map_t	**schedctl_proc_lookup(sc_page_ctl_t *, proc_t *);
static int		schedctl_map(struct anon_map *, caddr_t *);
static int		schedctl_getpage(struct anon_map **, caddr_t *);
static void		schedctl_freepage(struct anon_map *, caddr_t);

static int		schedctl_disabled = 0;

void
schedctl_init()
{
	schedctl_shared_init();
}

/*
 * System call interface to scheduler activations.  This always operates
 * on the current lwp.
 */
int
schedctl(unsigned int flags, int upcall_did, sc_shared_t **addrp)
{
	kthread_t	*t = curthread;
	proc_t		*p = curproc;
	vnode_t		*vp = NULL;
	file_t		*fp;
	sc_data_t	*tdp;
	door_node_t	*dp;
	sc_shared_t	*ssp = NULL;
	uint_t		flagdiff;
	sc_shared_t	*uaddr;
	int		error;
	long		tag;
	int		did;

	if (schedctl_disabled)
		return (set_errno(ENOSYS));

	/* check arguments */
	if (flags &
	    ~(SC_STATE | SC_BLOCK | SC_PRIORITY | SC_PREEMPT | SC_DOOR))
		return (set_errno(EINVAL));

	if (flags & SC_DOOR) {
		/* can't be combined with any other flags */
		if (flags & ~SC_DOOR)
			return (set_errno(EINVAL));

		if ((vp = p->p_sc_door) == NULL)
			return (set_errno(EINVAL));

		VN_HOLD(vp);
		if (falloc(vp, FREAD | FWRITE, &fp, &did)) {
			VN_RELE(vp);
			return (set_errno(EMFILE));
		}
		setf(did, fp);
		mutex_exit(&fp->f_tlock);
		f_setfd(did, FD_CLOEXEC);
		return (did);
	}

	if (t->t_schedctl == NULL) {
		tdp = kmem_zalloc(sizeof (sc_data_t), KM_SLEEP);
	} else {
		tdp = t->t_schedctl;
	}

	/*
	 * Get list of flags being added for this thread.  Note that
	 * flags can never be removed.
	 */
	flagdiff = flags & ~(tdp->sc_flags);

	if ((flagdiff & (SC_STATE | SC_PRIORITY | SC_PREEMPT)) &&
	    tdp->sc_shared == NULL) {
		/*
		 * Allocate space for shared data.
		 */
		tag = (long)(CRED()->cr_uid);
		if (error = schedctl_shared_alloc(tag, &ssp, &uaddr)) {
			if (t->t_schedctl == NULL)
				kmem_free(tdp, sizeof (sc_data_t));
			return (set_errno(error));
		}
		tdp->sc_shared = ssp;
		tdp->sc_tag = tag;
		tdp->sc_uaddr = uaddr;
	} else {
		uaddr = tdp->sc_uaddr;
		ssp = tdp->sc_shared;
	}

	/*
	 * Get and verify door referred to by upcall_did.  If the door is
	 * invalid, set the door vnode pointer to NULL but keep SC_BLOCK
	 * so SIGWAITING will still work.
	 */
	if (flagdiff & SC_BLOCK) {
		if (upcall_did >= 0 && (fp = getf(upcall_did)) != NULL) {
			if (VOP_REALVP(fp->f_vnode, &vp))
				vp = fp->f_vnode;
			if (vp != NULL && vp->v_type == VDOOR) {
				VN_HOLD(vp);
				releasef(upcall_did);
				dp = VTOD(vp);

				if ((dp->door_flags & DOOR_PRIVATE) == 0 ||
				    (dp->door_flags & (DOOR_UNREF | DOOR_DELAY
				    | DOOR_REVOKED)) != 0) {
					VN_RELE(vp);
					vp = NULL;
				}
			} else {
				releasef(upcall_did);
				vp = NULL;
			}
		}

		/*
		 * If a door was passed in, and p_sc_door is NULL,
		 * replace it with vp.  Otherwise release the hold
		 * on the door since we already have one.
		 */
		if (vp && casptr(&p->p_sc_door, NULL, vp) != NULL)
			VN_RELE(vp);

		/* do an atomic increment of number of unblocked lwps */
		atomic_add_32(&p->p_sc_unblocked, 1);
	}

	/*
	 * There is always a ctx op installed for any thread that
	 * calls schedctl().  This is required to support correct
	 * operation on fork and exec.  Normally, the ctx op has
	 * null functions installed for the "save" and "restore"
	 * operations.  If the SC_STATE flag is set, it installs
	 * the real functions that update the lwp state information.
	 */
	if (flagdiff & SC_STATE) {
		ASSERT(ssp != NULL);
		(void) removectx(t, NULL, schedctl_null, schedctl_null,
		    schedctl_fork, schedctl_null, schedctl_null);
		installctx(t, (void *)ssp, schedctl_save, schedctl_restore,
		    schedctl_fork, schedctl_null, schedctl_null);
	} else if (t->t_schedctl == NULL) {
		/*
		 * This is the first time schedctl was called by
		 * this thread.
		 */
		installctx(t, NULL, schedctl_null, schedctl_null,
		    schedctl_fork, schedctl_null, schedctl_null);
	}

	thread_lock(t);		/* protect against ts_tick and ts_update */
	tdp->sc_flags |= flags;
	ASSERT(tdp == t->t_schedctl || t->t_schedctl == NULL);
	t->t_schedctl = tdp;
	thread_unlock(t);

	if (addrp != NULL) {
#ifdef	_SYSCALL32_IMPL
		if (get_udatamodel() == DATAMODEL_ILP32) {
			caddr32_t	uaddr32;

			uaddr32 = (caddr32_t)uaddr;
			if (copyout(&uaddr32, addrp, sizeof (caddr32_t))
			    != 0) {
				return (set_errno(EFAULT));
			}
		} else
#endif	/* _SYSCALL32_IMPL */
		if (copyout(&uaddr, addrp, sizeof (sc_shared_t *)) != 0) {
			return (set_errno(EFAULT));
		}
	}
	return (0);
}

/*
 * Called when an lwp is about to do a long-term block.  If this is
 * the last running lwp with SC_BLOCK set in the process, look for a
 * thread in the scheduler activations door pool available to run in
 * its place.  If there is none, try to send a SIGWAITING signal.
 * We can't block waiting for a thread to be added to the door pool
 * since this may never happen.  Returns 1 if we had to go the
 * signal route and the lock was dropped in the process, 0 otherwise.
 */
int
schedctl_block(kmutex_t *lp)
{
	kthread_t	*t = curthread;
	proc_t		*p = ttoproc(curthread);

	ASSERT(t->t_schedctl != NULL && (t->t_schedctl->sc_flags & SC_BLOCK));

	ASSERT(p->p_sc_unblocked >= 1);
	if (atomic_add_32_nv(&p->p_sc_unblocked, -1) == 0) {
		if (p->p_flag & SWAITSIG) {
			if (p->p_sc_door == NULL ||
			    (t->t_handoff = door_get_activation(p->p_sc_door))
			    == NULL) {
				if (sigwaiting_send(lp)) {	/* ick */
					/* not really blocking */
					atomic_add_32(&p->p_sc_unblocked, 1);
					return (1);
				}
			}
		}
	}
	return (0);
}


void
schedctl_unblock()
{
	proc_t		*p = curproc;

	ASSERT(curthread->t_schedctl != NULL &&
	    (curthread->t_schedctl->sc_flags & SC_BLOCK));

	atomic_add_32(&p->p_sc_unblocked, 1);
}



/*
 * Clean up scheduler activations state associated with an exiting
 * (or execing) lwp.  t is always the current thread except when this
 * is called by forklwp_fail.
 */
void
schedctl_cleanup(kthread_t *t)
{
	sc_data_t	*tdp = t->t_schedctl;
	proc_t		*p = ttoproc(t);

	ASSERT(tdp != NULL);
	ASSERT(MUTEX_NOT_HELD(&p->p_lock));

	thread_lock(t);		/* protect against ts_tick and ts_update */
	t->t_schedctl = NULL;
	thread_unlock(t);
	if (tdp->sc_flags & SC_BLOCK) {
		ASSERT(p->p_sc_unblocked >= 1);
		atomic_add_32(&p->p_sc_unblocked, -1);
	}
	if (tdp->sc_flags & SC_STATE) {
		/*
		 * Remove the context op to avoid the final call to
		 * schedctl_save when switching away from this lwp.
		 */
		(void) removectx(t, (void *)(tdp->sc_shared),
		    schedctl_save, schedctl_restore, schedctl_fork,
		    schedctl_null, schedctl_null);
	} else {
		/*
		 * Remove the context op in case this is called from exec
		 * (don't want to later call schedctl_fork now that the
		 * t_schedctl pointer is NULL).
		 */
		(void) removectx(t, NULL, schedctl_null,
		    schedctl_null, schedctl_fork, schedctl_null, schedctl_null);
	}
	if (tdp->sc_shared != NULL)
		schedctl_shared_free(tdp->sc_tag, tdp->sc_shared);
	kmem_free(tdp, sizeof (sc_data_t));
}


/*
 * Called by resume just before switching away from the current thread.
 * Save new thread state.
 */
void
schedctl_save(sc_shared_t *ssp)
{
	ssp->sc_state = curthread->t_state;
}


/*
 * Called by resume after switching to the current thread.
 * Save new thread state and CPU.
 */
void
schedctl_restore(sc_shared_t *ssp)
{
	ssp->sc_state = SC_ONPROC;
	ssp->sc_cpu = CPU->cpu_id;
}


/*
 * On fork remove inherited mappings from the child's address space.
 * Also create a new schedctl structure, copy the SC_BLOCK flag, and
 * install new context ops.  We could copy other flags but since they
 * require the shared data they can't be supported until the user calls
 * schedctl() to get the shared data pointer.
 */
void
schedctl_fork(kthread_id_t pt, kthread_id_t ct)
{
	proc_t *cp = ttoproc(ct);
	sc_data_t *tdp = pt->t_schedctl;

	ASSERT(ct->t_schedctl == NULL);
	if ((cp->p_flag & SVFORK) == 0)		/* ignore vfork */
		schedctl_shared_disown(tdp->sc_tag, tdp->sc_shared,
		    ttoproc(pt), cp);
	ct->t_schedctl = kmem_zalloc(sizeof (sc_data_t), KM_SLEEP);
	if (tdp->sc_flags & SC_BLOCK) {
		ct->t_schedctl->sc_flags = SC_BLOCK;
		cp->p_sc_unblocked++;		/* doesn't need to be locked */
	}
	installctx(ct, NULL, schedctl_null, schedctl_null, schedctl_fork,
	    schedctl_null, schedctl_null);
}


/*
 * Stub for installctx.
 */
/*ARGSUSED*/
void
schedctl_null(sc_shared_t *ssp)
{}


/*
 * Returns 1 if the specified thread shouldn't be preempted at this time.
 * Called by ts_preempt, ts_tick, and ts_update.
 */
int
schedctl_get_nopreempt(kthread_t *t)
{
	sc_data_t	*tdp = t->t_schedctl;

	ASSERT(THREAD_LOCK_HELD(t));
	return (tdp->sc_shared->sc_preemptctl.sc_nopreempt != 0);
}


/*
 * Sets the value of the nopreempt field for the specified thread.
 * Called by ts_preempt to clear the field on preemption.
 */
void
schedctl_set_nopreempt(kthread_t *t, short val)
{
	ASSERT(THREAD_LOCK_HELD(t));
	t->t_schedctl->sc_shared->sc_preemptctl.sc_nopreempt = val;
}


/*
 * Sets the value of the yield field for the specified thread.  Called by
 * ts_preempt and ts_tick to set the field, and ts_yield to clear it.
 * The kernel never looks at this field so we don't need a schedctl_get_yield
 * function.
 */
void
schedctl_set_yield(kthread_t *t, short val)
{
	ASSERT(THREAD_LOCK_HELD(t));
	t->t_schedctl->sc_shared->sc_preemptctl.sc_yield = val;
}


/*
 * Page handling code.
 */

void
schedctl_shared_init()
{
	/*
	 * Amount of page that can hold sc_shared_t structures.  If
	 * sizeof (sc_shared_t) is a power of 2, this should just be
	 * PAGESIZE.
	 */
	sc_pagesize = PAGESIZE - (PAGESIZE % sizeof (sc_shared_t));

	/*
	 * Allocation bitmap is one bit per struct on a page.
	 */
	sc_bitmap_len = sc_pagesize / sizeof (sc_shared_t);
	sc_bitmap_words = howmany(sc_bitmap_len, BT_NBIPUL);
}

int
schedctl_shared_alloc(long tag, sc_shared_t **kaddrp, sc_shared_t **uaddrp)
{
	sc_pool_list_t	*poolp;
	sc_page_ctl_t	*pagep;
	sc_proc_map_t	*procp;
	sc_shared_t	*ssp;
	proc_t		*p = curproc;
	caddr_t		base = NULL;
	index_t		index;
	int		error;

	ASSERT(MUTEX_NOT_HELD(&p->p_lock));
	mutex_enter(&spc_lock);

	/*
	 * Find the right pool of pages.
	 */
	poolp = *(schedctl_pool_lookup(tag));
	if (poolp == NULL) {
		poolp = kmem_alloc(sizeof (sc_pool_list_t), KM_SLEEP);
		poolp->spl_next = sc_pool_head;
		poolp->spl_id = tag;
		poolp->spl_pagelist = NULL;
		sc_pool_head = poolp;
	}

	/*
	 * Try to find space for the new data in existing pages
	 * within the pool.
	 */
	for (pagep = poolp->spl_pagelist; pagep != NULL;
	    pagep = pagep->spc_next)
		if (pagep->spc_space > 0)
			break;
	if (pagep == NULL) {
		struct anon_map *amp;
		caddr_t kaddr;

		/*
		 * No room, need to allocate a new page.  Also set up
		 * a mapping to the kernel address space for the new
		 * page and lock it in memory.
		 */
		error = schedctl_getpage(&amp, &kaddr);
		if (error)
			goto err;
		/*
		 * Allocate and initialize page control structure.
		 */
		pagep = kmem_alloc(sizeof (sc_page_ctl_t), KM_SLEEP);
		pagep->spc_amp = amp;
		pagep->spc_base = (sc_shared_t *)kaddr;
		pagep->spc_end = (sc_shared_t *)
		    ((uintptr_t)kaddr + sc_pagesize);
		pagep->spc_map = kmem_zalloc(sizeof (ulong_t) * sc_bitmap_words,
		    KM_SLEEP);
		pagep->spc_space = sc_pagesize;
		pagep->spc_proclist = NULL;
		pagep->spc_next = poolp->spl_pagelist;
		poolp->spl_pagelist = pagep;
	}

	/*
	 * See if we have the page mapped already.
	 */
	procp = *(schedctl_proc_lookup(pagep, p));
	if (procp == NULL) {
		/*
		 * Page isn't mapped in this process.
		 */
		error = schedctl_map(pagep->spc_amp, &base);
		if (error)
			goto err;
		procp = kmem_alloc(sizeof (sc_proc_map_t), KM_SLEEP);
		procp->spm_next = pagep->spc_proclist;
		procp->spm_proc = p;
		procp->spm_uaddr = base;
		procp->spm_refcnt = 1;
		pagep->spc_proclist = procp;
	} else {
		base = procp->spm_uaddr;
		procp->spm_refcnt++;
	}

	/*
	 * Got a page, now allocate space for the data.  There should
	 * be space unless something's wrong.
	 */
	ASSERT(pagep != NULL && pagep->spc_space >= sizeof (sc_shared_t));
	index = bt_availbit(pagep->spc_map, sc_bitmap_len);
	ASSERT(index != -1);

	/*
	 * Get location with pointer arithmetic.  spc_base is of type
	 * sc_shared_t *.  Mark as allocated.
	 */
	ssp = pagep->spc_base + index;
	BT_SET(pagep->spc_map, index);
	pagep->spc_space -= sizeof (sc_shared_t);

	mutex_exit(&spc_lock);

	/*
	 * Return kernel and user addresses.
	 */
	*kaddrp = ssp;
	*uaddrp = (sc_shared_t *)((uintptr_t)base +
	    ((uintptr_t)ssp & PAGEOFFSET));
	return (0);

err:
	if (pagep && pagep->spc_proclist == NULL) {
		/*
		 * If we just created the page and got an error in
		 * schedctl_map, we should free the page and associated
		 * data structures.
		 */
		ASSERT(pagep->spc_space == sc_pagesize);
		ASSERT(poolp->spl_pagelist == pagep);
		schedctl_freepage(pagep->spc_amp,
		    (caddr_t)(pagep->spc_base));
		poolp->spl_pagelist = pagep->spc_next;
		kmem_free(pagep->spc_map,
		    sizeof (ulong_t) * sc_bitmap_words);
		kmem_free(pagep, sizeof (sc_page_ctl_t));
	}

	if (poolp->spl_pagelist == NULL) {
		/*
		 * If the whole pool is now empty, free it.
		 */
		ASSERT(sc_pool_head == poolp);
		sc_pool_head = poolp->spl_next;
		kmem_free(poolp, sizeof (sc_pool_list_t));
	}
	mutex_exit(&spc_lock);
	return (error);
}


/*
 * Free up scheduling control state associated with an exiting lwp.
 */
static void
schedctl_shared_free(long tag, sc_shared_t *ssp)
{
	sc_pool_list_t	**poolpp, *poolp;
	sc_page_ctl_t	**pagepp, *pagep;
	sc_proc_map_t	**procpp, *procp;
	proc_t		*p = curproc;
	index_t		index;

	mutex_enter(&spc_lock);
	poolpp = schedctl_pool_lookup(tag);
	if ((poolp = *poolpp) == NULL) {
		/* No pool */
		mutex_exit(&spc_lock);
		return;
	}

	pagepp = schedctl_page_lookup(poolp, ssp);
	if ((pagep = *pagepp) == NULL) {
		/* No page */
		mutex_exit(&spc_lock);
		return;
	}

	/*
	 * Mark as free space.
	 */
	ssp->sc_state = SC_FREE;
	index = (index_t)(ssp - pagep->spc_base);
	BT_CLEAR(pagep->spc_map, index);
	pagep->spc_space += sizeof (sc_shared_t);

	procpp = schedctl_proc_lookup(pagep, p);
	if ((procp = *procpp) == NULL) {
		/* No proc */
		mutex_exit(&spc_lock);
		return;
	}
	if (--procp->spm_refcnt == 0) {
		/*
		 * Last lwp in process using this page is exiting,
		 * so unmap the user space and free the mapping
		 * structure for this process.
		 * This also happens on exec.
		 */
		*procpp = procp->spm_next;
		(void) as_unmap(p->p_as, procp->spm_uaddr, PAGESIZE);
		kmem_free(procp, sizeof (sc_proc_map_t));

		if (pagep->spc_proclist == NULL) {
			/*
			 * When all processes with mappings to the page
			 * have exited we can free the page.  It should
			 * now be empty.
			 */
			ASSERT(pagep->spc_space == sc_pagesize);
			schedctl_freepage(pagep->spc_amp,
			    (caddr_t)(pagep->spc_base));
			*pagepp = pagep->spc_next;
			kmem_free(pagep->spc_map,
			    sizeof (ulong_t) * sc_bitmap_words);
			kmem_free(pagep, sizeof (sc_page_ctl_t));

			/*
			 * See if the whole pool is now empty.  If it is,
			 * free it.
			 */
			if (poolp->spl_pagelist == NULL) {
				*poolpp = poolp->spl_next;
				kmem_free(poolp, sizeof (sc_pool_list_t));
			}
		}
	}
	mutex_exit(&spc_lock);
}


/*
 * Removes the mapping in the child process corresponding to the
 * location in the parent process.  This is done at fork time so
 * the mappings won't be inherited.
 */
static void
schedctl_shared_disown(long tag, sc_shared_t *ssp, proc_t *pp, proc_t *cp)
{
	sc_pool_list_t	*poolp;
	sc_page_ctl_t	*pagep;
	sc_proc_map_t	*procp;

	mutex_enter(&spc_lock);
	if ((poolp = *(schedctl_pool_lookup(tag))) == NULL)
		goto out;
	if ((pagep = *(schedctl_page_lookup(poolp, ssp))) == NULL)
		goto out;
	if ((procp = *(schedctl_proc_lookup(pagep, pp))) == NULL)
		goto out;
	(void) as_unmap(cp->p_as, procp->spm_uaddr, PAGESIZE);
out:
	mutex_exit(&spc_lock);
}


/*
 * Find the pool structure corresponding to a tag.
 */
static sc_pool_list_t **
schedctl_pool_lookup(long tag)
{
	sc_pool_list_t **poolpp;

	ASSERT(MUTEX_HELD(&spc_lock));
	for (poolpp = &sc_pool_head; *poolpp != NULL;
	    poolpp = &((*poolpp)->spl_next)) {
		if ((*poolpp)->spl_id == tag)
			break;
	}
	return (poolpp);
}


/*
 * Find the page control structure corresponding to a kernel address.
 */
static sc_page_ctl_t **
schedctl_page_lookup(sc_pool_list_t *poolp, sc_shared_t *ssp)
{
	sc_page_ctl_t **pagepp;

	ASSERT(MUTEX_HELD(&spc_lock));
	for (pagepp = &(poolp->spl_pagelist); *pagepp != NULL;
	    pagepp = &((*pagepp)->spc_next)) {
		if (ssp >= (*pagepp)->spc_base && ssp < (*pagepp)->spc_end)
			break;
	}
	return (pagepp);
}


/*
 * Find the process map structure corresponding to a process.
 */
static sc_proc_map_t **
schedctl_proc_lookup(sc_page_ctl_t *pagep, proc_t *p)
{
	sc_proc_map_t **procpp;

	ASSERT(MUTEX_HELD(&spc_lock));
	for (procpp = &(pagep->spc_proclist); *procpp != NULL;
	    procpp = &((*procpp)->spm_next)) {
		if ((*procpp)->spm_proc == p)
			break;
	}
	return (procpp);
}


/*
 * This function is called when a page needs to be mapped into a
 * process's address space.  Allocate the user address space and
 * set up the mapping to the page.  Assumes the page has already
 * been allocated and locked in memory via schedctl_getpage.
 */
static int
schedctl_map(struct anon_map *amp, caddr_t *uaddrp)
{
	caddr_t addr;
	struct as *as = curproc->p_as;
	struct segvn_crargs vn_a;
	int error;

	as_rangelock(as);
	map_addr(&addr, PAGESIZE, 0, 1, 0);
	if (addr == NULL) {
		as_rangeunlock(as);
		return (ENOMEM);
	}

	/*
	 * Use segvn to set up the mapping to the page.
	 */
	vn_a.vp = NULL;
	vn_a.offset = 0;
	vn_a.cred = NULL;
	vn_a.type = MAP_SHARED;
	vn_a.prot = vn_a.maxprot = PROT_ALL;
	vn_a.flags = 0;
	vn_a.amp = amp;
	error = as_map(as, addr, PAGESIZE, segvn_create, &vn_a);
	as_rangeunlock(as);

	if (error)
		return (error);

	*uaddrp = addr;
	return (0);
}


/*
 * Allocate a new page from anonymous memory.  Also, create a kernel
 * mapping to the page and lock the page in memory.
 */
static int
schedctl_getpage(struct anon_map **newamp, caddr_t *newaddr)
{
	struct anon_map *amp;
	caddr_t kaddr;
	int error;

	/*
	 * Pre-allocate swap space for page
	 */
	if (anon_resv(PAGESIZE) == 0)
		return (ENOMEM);

	/*
	 * Set up anonymous memory struct
	 */
	amp = anonmap_alloc(PAGESIZE, PAGESIZE);

	/*
	 * Allocate the page.
	 */
	mutex_enter(&amp->serial_lock);
	kaddr = segkp_get_withanonmap(segkp, PAGESIZE, KPD_LOCKED | KPD_ZERO,
	    amp);
	mutex_exit(&amp->serial_lock);
	if (kaddr == NULL) {
		error = ENOMEM;
		amp->refcnt--;
		anonmap_free(amp);
		anon_unresv(PAGESIZE);
		return (error);
	}

	/*
	 * The page is left SE_SHARED locked so that it won't be
	 * paged out or relocated (KPD_LOCKED above).
	 */

	*newamp = amp;
	*newaddr = kaddr;
	return (0);
}


/*
 * Take the necessary steps to allow a page to be released.  This is
 * called when all processes accessing the page have exited or are in
 * the process of exiting.  There should be no accesses to the page
 * after this.  The kernel mapping of the page is released and the
 * page is unlocked.
 */
static void
schedctl_freepage(struct anon_map *amp, caddr_t kaddr)
{

	/*
	 * Release the lock on the page and remove the kernel mapping.
	 */
	mutex_enter(&amp->lock);
	segkp_release(segkp, kaddr);

	/*
	 * Decrement the refcnt so the anon_map structure will be freed
	 * when the last process mapping the page exits.
	 */
	if (--amp->refcnt == 0) {
		/*
		 * The current process no longer has the page mapped, so
		 * we have to free everything rather than letting as_free
		 * do the work.
		 */
		anon_free(amp->ahp, 0, PAGESIZE);
		anon_unresv(PAGESIZE);
		mutex_exit(&amp->lock);
		anonmap_free(amp);
	} else
		mutex_exit(&amp->lock);
}
